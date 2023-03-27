/* Goxel 3D voxels editor
 *
 * copyright (c) 2023-present Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

#include "file_format.h"

#include "quickjs/quickjs.h"
#include "quickjs/quickjs-libc.h"

#define STB_DS_IMPLEMENTATION
#include "stb/stb_ds.h"

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;

static JSClassID js_vec_class_id;
static JSClassID js_box_class_id;
static JSClassID js_volume_class_id;
static JSClassID js_image_class_id;
static JSClassID js_layer_class_id;
static JSClassID js_goxel_class_id;


typedef struct {
    char name[128];
    JSValue execute_fn;
} script_t;

// stb array of registered scripts
static script_t *g_scripts = NULL;

typedef struct {
    int size;
    float *values;
    JSValue owner;
} vec_t;

typedef struct {
    float (*mat)[4];
    JSValue owner;
} box_t;

static void get_vec_int(JSContext *ctx, JSValue val, int size, int *out,
                        int default_val)
{
    int i;
    JSValue v;
    vec_t *vec;

    vec = JS_GetOpaque2(ctx, val, js_vec_class_id);
    if (vec) {
        for (i = 0; i < size; i++) {
            out[i] = i < vec->size ? vec->values[i] : default_val;
        }
        return;
    }

    for (i = 0; i < size; i++) {
        v = JS_GetPropertyUint32(ctx, val, i);
        JS_ToInt32(ctx, &out[i], v);
    }
}

static void get_vec_uint8(JSContext *ctx, JSValue val, int size, uint8_t *out,
                          uint8_t default_val)
{
    int buf[4];
    int i;
    get_vec_int(ctx, val, size, buf, default_val);
    for (i = 0; i < size; i++) out[i] = buf[i];
}

static JSValue js_vec_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValueConst *argv)
{
    int i;
    double v;
    JSValue ret;
    vec_t *vec;

    vec = calloc(1, sizeof(*vec));
    vec->size = argc;
    vec->values = calloc(argc, sizeof(float));
    for (i = 0; i < argc; i++) {
        JS_ToFloat64(ctx, &v, argv[i]);
        vec->values[i] = v;
    }
    ret = JS_NewObjectClass(ctx, js_vec_class_id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static void js_vec_finalizer(JSRuntime *rt, JSValue val)
{
    vec_t *vec = JS_GetOpaque(val, js_vec_class_id);
    if (JS_IsUndefined(vec->owner)) js_free_rt(rt, vec->values);
    JS_FreeValueRT(rt, vec->owner);
    js_free_rt(rt, vec);
}

static JSValue new_js_vec3(JSContext *ctx, float x, float y, float z)
{
    JSValue ret;
    vec_t *vec;
    vec = js_mallocz(ctx, sizeof(*vec));
    vec->owner = JS_UNDEFINED;
    vec->size = 3;
    vec->values = calloc(3, sizeof(float));
    vec->values[0] = x;
    vec->values[1] = y;
    vec->values[2] = z;
    ret = JS_NewObjectClass(ctx, js_vec_class_id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static JSValue new_js_vec4(JSContext *ctx, float x, float y, float z, float w)
{
    JSValue ret;
    vec_t *vec;
    vec = js_mallocz(ctx, sizeof(*vec));
    vec->owner = JS_UNDEFINED;
    vec->size = 4;
    vec->values = calloc(4, sizeof(float));
    vec->values[0] = x;
    vec->values[1] = y;
    vec->values[2] = z;
    vec->values[3] = z;
    ret = JS_NewObjectClass(ctx, js_vec_class_id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static JSValue js_vec_get_at(JSContext *ctx, JSValueConst this_val, int idx)
{
    vec_t *vec = JS_GetOpaque2(ctx, this_val, js_vec_class_id);
    if (!vec)
        return JS_EXCEPTION;
    if (idx >= vec->size)
        return JS_EXCEPTION;
    return JS_NewFloat64(ctx, vec->values[idx]);
}

static JSValue js_vec_set_at(
        JSContext *ctx, JSValueConst this_val, JSValue val, int idx)
{
    vec_t *vec;
    double v;

    vec = JS_GetOpaque2(ctx, this_val, js_vec_class_id);
    if (!vec)
        return JS_EXCEPTION;
    if (idx >= vec->size)
        return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &v, val))
        return JS_EXCEPTION;
    vec->values[idx] = v;
    return JS_UNDEFINED;
}

static void bind_vec(JSContext *ctx)
{
    JSValue proto, ctor, global_obj;
    static const JSCFunctionListEntry js_vec_proto_funcs[] = {
        JS_CGETSET_MAGIC_DEF("x", js_vec_get_at, js_vec_set_at, 0),
        JS_CGETSET_MAGIC_DEF("y", js_vec_get_at, js_vec_set_at, 1),
        JS_CGETSET_MAGIC_DEF("z", js_vec_get_at, js_vec_set_at, 2),
        JS_CGETSET_MAGIC_DEF("w", js_vec_get_at, js_vec_set_at, 3),
        JS_CGETSET_MAGIC_DEF("r", js_vec_get_at, js_vec_set_at, 0),
        JS_CGETSET_MAGIC_DEF("g", js_vec_get_at, js_vec_set_at, 1),
        JS_CGETSET_MAGIC_DEF("b", js_vec_get_at, js_vec_set_at, 2),
        JS_CGETSET_MAGIC_DEF("a", js_vec_get_at, js_vec_set_at, 3),
    };
    static JSClassDef js_vec_class = {
        "Vec",
        .finalizer = js_vec_finalizer,
    }; 

    JS_NewClassID(&js_vec_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_vec_class_id, &js_vec_class);

    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_vec_proto_funcs,
                               ARRAY_SIZE(js_vec_proto_funcs));
    JS_SetClassProto(ctx, js_vec_class_id, proto);
    ctor = JS_NewCFunction2(ctx, js_vec_ctor, "Vec", 0,
                            JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);
    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "Vec", ctor);
    JS_FreeValue(ctx, global_obj);
}

static JSValue new_js_box(JSContext *ctx, JSValueConst owner, float mat[4][4])
{
    JSValue ret;
    box_t *box;
    if (mat == NULL) return JS_NULL;
    box = js_mallocz(ctx, sizeof(*box));
    box->owner = JS_DupValue(ctx, owner);
    if (JS_IsUndefined(owner)) {
        box->mat = calloc(1, 16 * sizeof(float));
        mat4_copy(mat, box->mat);
    } else {
        box->mat = mat;
    }
    ret = JS_NewObjectClass(ctx, js_box_class_id);
    JS_SetOpaque(ret, box);
    return ret;
}

static JSValue js_box_iterVoxels(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    float inv[4][4];
    int aabb[2][3];
    int x, y, z;
    float pos[3], localpos[3];
    box_t *box;
    JSValue js_pos, val;
    const float EPS = 1e-8;
    const float L = 1 + EPS;

    if (argc != 1) return JS_EXCEPTION;
    box = JS_GetOpaque(this_val, js_box_class_id);
    mat4_invert(box->mat, inv);
    box_get_aabb(box->mat, aabb);
    for (z = aabb[0][2]; z < aabb[1][2]; z++) {
        for (y = aabb[0][1]; y < aabb[1][1]; y++) {
            for (x = aabb[0][0]; x < aabb[1][0]; x++) {
                vec3_set(pos, x, y, z);
                mat4_mul_vec3(inv, pos, localpos);
                if (    localpos[0] < -L || localpos[0] > +L ||
                        localpos[1] < -L || localpos[1] > +L ||
                        localpos[2] < -L || localpos[2] > +L)
                    continue;
                js_pos = new_js_vec3(ctx, x, y, z);
                val = JS_Call(ctx, argv[0], JS_NULL, 1, &js_pos);
                JS_FreeValue(ctx, js_pos);
                if (JS_IsException(val))
                    return val;
                JS_FreeValue(ctx, val);
            }
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_box_worldToLocal(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    return JS_EXCEPTION;
}

static void js_box_finalizer(JSRuntime *rt, JSValue val)
{
    box_t *box = JS_GetOpaque(val, js_box_class_id);
    if (JS_IsUndefined(box->owner)) js_free_rt(rt, box->mat);
    JS_FreeValueRT(rt, box->owner);
    js_free_rt(rt, box);
}

static void bind_box(JSContext *ctx)
{
    JSValue proto;
    static const JSCFunctionListEntry js_box_proto_funcs[] = {
        JS_CFUNC_DEF("iterVoxels", 1, js_box_iterVoxels),
        JS_CFUNC_DEF("worldToLocal", 1, js_box_worldToLocal),
    };
    static JSClassDef js_box_class = {
        "Box",
        .finalizer = js_box_finalizer,
    }; 

    JS_NewClassID(&js_box_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_box_class_id, &js_box_class);

    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_box_proto_funcs,
                               ARRAY_SIZE(js_box_proto_funcs));
    JS_SetClassProto(ctx, js_box_class_id, proto);
}


static JSValue js_volume_ctor(JSContext *ctx, JSValueConst new_target,
                            int argc, JSValueConst *argv)
{
    JSValue ret;
    volume_t *volume;
    volume = volume_new();
    ret = JS_NewObjectClass(ctx, js_volume_class_id);
    JS_SetOpaque(ret, volume);
    return ret;
}

static void js_volume_finalizer(JSRuntime *ctx, JSValue this_val)
{
    volume_t *volume;
    volume = JS_GetOpaque(this_val, js_volume_class_id);
    volume_delete(volume);
}

static JSValue js_volume_iter(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    volume_t *volume;
    JSValue args[2];
    volume_iterator_t iter;
    int pos[3];
    uint8_t value[4];

    volume = JS_GetOpaque(this_val, js_volume_class_id);
    iter = volume_get_iterator(volume, VOLUME_ITER_VOXELS);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, value);
        args[0] = new_js_vec3(ctx, pos[0], pos[1], pos[2]);
        args[1] = new_js_vec4(ctx, value[0], value[1], value[2], value[3]);
        JS_Call(ctx, argv[0], JS_UNDEFINED, 2, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
    };
    return JS_UNDEFINED;
}

static JSValue js_volume_setAt(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    volume_t *volume;
    int pos[3];
    uint8_t v[4];

    get_vec_int(ctx, argv[0], 3, pos, 0);
    get_vec_uint8(ctx, argv[1], 4, v, 255);
    volume = JS_GetOpaque2(ctx, this_val, js_volume_class_id);
    volume_set_at(volume, NULL, pos, v);
    return JS_UNDEFINED;
}

static JSValue js_volume_save(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    volume_t *volume;
    image_t *img;
    const char *path, *format = NULL;
    const file_format_t *f;
    int err;

    volume = JS_GetOpaque2(ctx, this_val, js_volume_class_id);
    path = JS_ToCString(ctx, argv[0]);
    if (argc > 1)
        format = JS_ToCString(ctx, argv[1]);

    f = file_format_for_path(path, format, "w");
    if (!f) {
        fprintf(stderr, "Cannot find format for file %s\n", path);
        return JS_EXCEPTION;
    }
    img = image_new();
    volume_set(img->active_layer->volume, volume);
    err = f->export_func(f, img, path);
    if (err) {
        fprintf(stderr, "Internal error saving file %s\n", path);
        return JS_EXCEPTION;
    }
    image_delete(img);

    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, format);
    return JS_UNDEFINED;
}

static void bind_volume(JSContext *ctx)
{
    JSValue proto, ctor, global_obj;

    static const JSCFunctionListEntry js_volume_proto_funcs[] = {
        JS_CFUNC_DEF("iter", 1, js_volume_iter),
        JS_CFUNC_DEF("setAt", 2, js_volume_setAt),
        JS_CFUNC_DEF("save", 2, js_volume_save),
    };

    static JSClassDef js_volume_class = {
        "Volume",
        .finalizer = js_volume_finalizer,
    };

    JS_NewClassID(&js_volume_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_volume_class_id, &js_volume_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_volume_proto_funcs,
                               ARRAY_SIZE(js_volume_proto_funcs));
    JS_SetClassProto(ctx, js_volume_class_id, proto);

    ctor = JS_NewCFunction2(ctx, js_volume_ctor, "Volume", 0,
                            JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);

    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "Volume", ctor);
    JS_FreeValue(ctx, global_obj);
}

static void js_image_finalizer(JSRuntime *ctx, JSValue this_val)
{
    image_t *image;
    image = JS_GetOpaque(this_val, js_image_class_id);
    image_delete(image);
}

static JSValue js_image_getLayersVolume(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue ret;
    volume_t *volume;
    image_t *image;

    image = JS_GetOpaque(this_val, js_image_class_id);
    volume = volume_copy(goxel_get_layers_volume(image));
    ret = JS_NewObjectClass(ctx, js_volume_class_id);
    JS_SetOpaque(ret, (void*)volume);
    return ret;
}

static JSValue js_image_activeLayer_get(JSContext *ctx, JSValueConst this_val)
{
    image_t *image;
    JSValue ret;

    image = JS_GetOpaque(this_val, js_image_class_id);
    ret = JS_NewObjectClass(ctx, js_layer_class_id);
    image->active_layer->ref++;
    JS_SetOpaque(ret, (void*)image->active_layer);
    return ret;
}

static void bind_image(JSContext *ctx)
{
    JSValue proto;
    static const JSCFunctionListEntry js_image_proto_funcs[] = {
        JS_CGETSET_DEF("activeLayer", js_image_activeLayer_get, NULL),
        JS_CFUNC_DEF("getLayersVolume", 0, js_image_getLayersVolume),
    };
    static JSClassDef js_image_class = {
        "Image",
        .finalizer = js_image_finalizer,
    };
    JS_NewClassID(&js_image_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_image_class_id, &js_image_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_image_proto_funcs,
                               ARRAY_SIZE(js_image_proto_funcs));
    JS_SetClassProto(ctx, js_image_class_id, proto);
}

static JSValue js_layer_volume_get(JSContext *ctx, JSValueConst this_val)
{
    layer_t *layer;
    JSValue ret;

    layer = JS_GetOpaque(this_val, js_layer_class_id);
    ret = JS_NewObjectClass(ctx, js_volume_class_id);
    JS_SetOpaque(ret, (void*)volume_dup(layer->volume));
    return ret;
}

static void js_layer_finalizer(JSRuntime *ctx, JSValue this_val)
{
    layer_t *layer;
    layer = JS_GetOpaque(this_val, js_layer_class_id);
    layer_delete(layer);
}

static void bind_layer(JSContext *ctx)
{
    JSValue proto;
    static const JSCFunctionListEntry js_layer_proto_funcs[] = {
        JS_CGETSET_DEF("volume", js_layer_volume_get, NULL),
    };
    static JSClassDef js_layer_class = {
        "Layer",
        .finalizer = js_layer_finalizer,
    };
    JS_NewClassID(&js_layer_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_layer_class_id, &js_layer_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_layer_proto_funcs,
                               ARRAY_SIZE(js_layer_proto_funcs));
    JS_SetClassProto(ctx, js_layer_class_id, proto);
}

typedef struct {
    file_format_t format;
    JSValue data;
} script_file_format_t;

int script_format_export_func(const file_format_t *format_,
                              const image_t *img, const char *path)
{
    JSContext *ctx = g_ctx;
    const script_file_format_t *format = (void*)format_;
    JSValue export, image;
    JSValueConst argv[2];

    image = JS_NewObjectClass(ctx, js_image_class_id);
    goxel.image->ref++;
    JS_SetOpaque(image, (void*)goxel.image);
    argv[0] = image;
    argv[1] = JS_NewString(ctx, path);
    export = JS_GetPropertyStr(ctx, format->data, "export");
    JS_Call(ctx, export, JS_NULL, 2, argv);
    return 0;
}

static JSValue js_goxel_registerFormat(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    const char *name, *ext;
    JSValueConst args;
    script_file_format_t *format;

    args = argv[0];
    name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, args, "name"));
    ext = JS_ToCString(ctx, JS_GetPropertyStr(ctx, args, "ext"));

    LOG_I("Register format %s", name);
    // TODO: finish this.
    format = calloc(1, sizeof(*format));
    *format = (script_file_format_t) {
        .format = {
            .name = name,
            .ext = ext,
            .export_func = script_format_export_func,
        },
        .data = JS_DupValue(ctx, args),
    };
    file_format_register(&format->format);
    // JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

static JSValue js_goxel_registerScript(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValueConst data;
    script_t script = {};
    const char *name;

    data = argv[0];
    name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, data, "name"));
    LOG_I("Register script %s", name);
    snprintf(script.name, sizeof(script.name), "%s", name);
    script.execute_fn = JS_GetPropertyStr(ctx, data, "onExecute");
    arrput(g_scripts, script);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

static JSValue js_goxel_selection_get(JSContext *ctx, JSValueConst this_val)
{
    return new_js_box(ctx, this_val, goxel.selection);
}

static JSValue js_goxel_selection_set(JSContext *ctx, JSValueConst this_val,
                                      JSValue val)
{
    return JS_UNDEFINED;
}

static JSValue js_goxel_image_get(JSContext *ctx, JSValueConst this_val)
{
    JSValue ret;
    ret = JS_NewObjectClass(ctx, js_image_class_id);
    goxel.image->ref++;
    JS_SetOpaque(ret, (void*)goxel.image);
    return ret;
}

static void bind_goxel(JSContext *ctx)
{
    JSValue proto, global_obj, obj;
    static const JSCFunctionListEntry js_goxel_proto_funcs[] = {
        JS_CFUNC_DEF("registerFormat", 1, js_goxel_registerFormat),
        JS_CFUNC_DEF("registerScript", 1, js_goxel_registerScript),
        JS_CGETSET_DEF("selection",
                js_goxel_selection_get, js_goxel_selection_set),
        JS_CGETSET_DEF("image", js_goxel_image_get, NULL),
    };
    static JSClassDef js_goxel_class = {
        "Goxel",
    };

    JS_NewClassID(&js_goxel_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_goxel_class_id, &js_goxel_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_goxel_proto_funcs,
                               ARRAY_SIZE(js_goxel_proto_funcs));
    JS_SetClassProto(ctx, js_goxel_class_id, proto);

    obj = JS_NewObjectClass(ctx, js_goxel_class_id);
    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "goxel", obj);
    JS_FreeValue(ctx, global_obj);
}

static void init_runtime(void)
{
    if (g_ctx) return;
    g_rt = JS_NewRuntime();
    g_ctx = JS_NewContext(g_rt);
    js_init_module_std(g_ctx, "std");
    js_init_module_os(g_ctx, "os");
    bind_vec(g_ctx);
    bind_box(g_ctx);
    bind_volume(g_ctx);
    bind_image(g_ctx);
    bind_layer(g_ctx);
    bind_goxel(g_ctx);
}

static int script_run_from_str(
        const char *script, int len, const char *filename, int argc,
        const char **argv)
{
    int ret = 0;
    JSValue val;

    init_runtime();
    js_std_add_helpers(g_ctx, argc, (char**)argv);

    val = JS_Eval(g_ctx, script, len, filename, JS_EVAL_TYPE_MODULE);
    if (JS_IsException(val)) {
        js_std_dump_error(g_ctx);
        ret = -1;
    }
    JS_FreeValue(g_ctx, val);
    return ret;
}

int script_run_from_file(const char *filename, int argc, const char **argv)
{
    char *script;
    int ret, size;

    script = read_file(filename, &size);
    if (!script) {
        fprintf(stderr, "Cannot read '%s'\n", filename);
        return -1;
    }

    ret = script_run_from_str(script, size, filename, argc, argv);
    free(script);
    return ret;
}

static int on_script(int i, const char *path, void *user)
{
    const char *data;

    LOG_D("Run script %s", path);
    data = assets_get(path, NULL);
    script_run_from_str(data, strlen(data), path, 0, NULL);
    return 0;
}

static int on_user_script(const char *dir, const char *name, void *user)
{
    char path[1024];
    char *data;
    int size;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    data = read_file(path, &size);
    if (!data) return -1;
    script_run_from_str(data, strlen(data), path, 0, NULL);
    return 0;
}

void script_init(void)
{
    char dir[1024];

    assets_list("data/scripts/", NULL, on_script);
    if (sys_get_user_dir()) {
        snprintf(dir, sizeof(dir), "%s/scripts", sys_get_user_dir());
        LOG_I("Loading scripts from %s\n", dir);
        sys_list_dir(dir, on_user_script, NULL);
    }
}

void script_release(void)
{
    JS_FreeContext(g_ctx);
    JS_FreeRuntime(g_rt);
}

void script_iter_all(void *user, void (*f)(void *user, const char *name))
{
    int i;
    for (i = 0; i < arrlen(g_scripts); i++) {
        f(user, g_scripts[i].name);
    }
}

int script_execute(const char *name)
{
    int i;
    int ret = 0;
    script_t *script;
    JSContext *ctx = g_ctx;
    JSValue val;

    for (i = 0; i < arrlen(g_scripts); i++) {
        script = &g_scripts[i];
        if (strcmp(script->name, name) == 0) break;
    }
    if (i == arrlen(g_scripts)) return -1;
    LOG_I("Run script %s", name);
    val = JS_Call(ctx, script->execute_fn, JS_NULL, 0, NULL);
    if (JS_IsException(val)) {
        LOG_E("Error executing script");
        js_std_dump_error(ctx);
        ret = -1;
    }
    JS_FreeValue(ctx, val);
    return ret;
}
