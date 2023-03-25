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

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;

static JSClassID js_vec_class_id;
static JSClassID js_mesh_class_id;
static JSClassID js_image_class_id;
static JSClassID js_goxel_class_id;

typedef struct {
    int size;
    double values[4];
} vec_t;

static void get_vec_int(JSContext *ctx, JSValue val, int size, int *out)
{
    int i;
    JSValue v;
    for (i = 0; i < size; i++) {
        v = JS_GetPropertyUint32(ctx, val, i);
        JS_ToInt32(ctx, &out[i], v);
    }
}

static void get_vec_uint8(JSContext *ctx, JSValue val, int size, uint8_t *out)
{
    int *buf;
    int i;
    buf = calloc(size, sizeof(int));
    get_vec_int(ctx, val, size, buf);
    for (i = 0; i < size; i++) out[i] = buf[i];
    free(buf);
}

static JSValue js_vec_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValueConst *argv)
{
    int i;
    JSValue ret;
    vec_t *vec;

    vec = calloc(1, sizeof(*vec));
    vec->size = argc;
    for (i = 0; i < argc; i++) {
        JS_ToFloat64(ctx, &vec->values[i], argv[i]);
    }
    ret = JS_NewObjectClass(ctx, js_vec_class_id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static void js_vec_finalizer(JSRuntime *rt, JSValue val)
{
    vec_t *vec = JS_GetOpaque(val, js_vec_class_id);
    js_free_rt(rt, vec);
}

static JSValue new_js_vec3(JSContext *ctx, double x, double y, double z)
{
    JSValue ret;
    vec_t *vec;
    vec = js_mallocz(ctx, sizeof(*vec));
    vec->size = 3;
    vec->values[0] = x;
    vec->values[1] = y;
    vec->values[2] = z;
    ret = JS_NewObjectClass(ctx, js_vec_class_id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static JSValue new_js_vec4(JSContext *ctx,
                           double x, double y, double z, double w)
{
    JSValue ret;
    vec_t *vec;
    vec = js_mallocz(ctx, sizeof(*vec));
    vec->size = 4;
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


static JSValue js_mesh_ctor(JSContext *ctx, JSValueConst new_target,
                            int argc, JSValueConst *argv)
{
    JSValue ret;
    mesh_t *mesh;
    mesh = mesh_new();
    ret = JS_NewObjectClass(ctx, js_mesh_class_id);
    JS_SetOpaque(ret, mesh);
    return ret;
}

static void js_mesh_finalizer(JSRuntime *ctx, JSValue this_val)
{
    mesh_t *mesh;
    mesh = JS_GetOpaque(this_val, js_mesh_class_id);
    mesh_delete(mesh);
}

static JSValue js_mesh_iter(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    JSValue args[2];
    mesh_iterator_t iter;
    int pos[3];
    uint8_t value[4];

    mesh = JS_GetOpaque(this_val, js_mesh_class_id);
    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, &iter, pos, value);
        args[0] = new_js_vec3(ctx, pos[0], pos[1], pos[2]);
        args[1] = new_js_vec4(ctx, value[0], value[1], value[2], value[3]);
        JS_Call(ctx, argv[0], JS_UNDEFINED, 2, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
    };
    return JS_UNDEFINED;
}

static JSValue js_mesh_setAt(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    int pos[3];
    uint8_t v[4];

    get_vec_int(ctx, argv[0], 3, pos);
    get_vec_uint8(ctx, argv[1], 4, v);
    mesh = JS_GetOpaque2(ctx, this_val, js_mesh_class_id);
    mesh_set_at(mesh, NULL, pos, v);
    return JS_UNDEFINED;
}

static JSValue js_mesh_save(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    image_t *img;
    const char *path, *format = NULL;
    const file_format_t *f;
    int err;

    mesh = JS_GetOpaque2(ctx, this_val, js_mesh_class_id);
    path = JS_ToCString(ctx, argv[0]);
    if (argc > 1)
        format = JS_ToCString(ctx, argv[1]);

    f = file_format_for_path(path, format, "w");
    if (!f) {
        fprintf(stderr, "Cannot find format for file %s\n", path);
        return JS_EXCEPTION;
    }
    img = image_new();
    mesh_set(img->active_layer->mesh, mesh);
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

static void bind_mesh(JSContext *ctx)
{
    JSValue proto, ctor, global_obj;

    static const JSCFunctionListEntry js_mesh_proto_funcs[] = {
        JS_CFUNC_DEF("iter", 1, js_mesh_iter),
        JS_CFUNC_DEF("setAt", 1, js_mesh_setAt),
        JS_CFUNC_DEF("save", 2, js_mesh_save),
    };

    static JSClassDef js_mesh_class = {
        "Mesh",
        .finalizer = js_mesh_finalizer,
    };

    JS_NewClassID(&js_mesh_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_mesh_class_id, &js_mesh_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_mesh_proto_funcs,
                               ARRAY_SIZE(js_mesh_proto_funcs));
    JS_SetClassProto(ctx, js_mesh_class_id, proto);

    ctor = JS_NewCFunction2(ctx, js_mesh_ctor, "Mesh", 0,
                            JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);

    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "Mesh", ctor);
    JS_FreeValue(ctx, global_obj);
}

static void js_image_finalizer(JSRuntime *ctx, JSValue this_val)
{
    image_t *image;
    image = JS_GetOpaque(this_val, js_image_class_id);
    if (0)
        image_delete(image);
}

static JSValue js_image_getLayersMesh(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue ret;
    const mesh_t *mesh;
    image_t *image;

    image = JS_GetOpaque(this_val, js_image_class_id);
    mesh = goxel_get_layers_mesh(image);
    ret = JS_NewObjectClass(ctx, js_mesh_class_id);
    // XXX: should make it non destructable somehow?  Or add ref counting
    // to image_t?
    JS_SetOpaque(ret, (void*)mesh);
    return ret;
}

static void bind_image(JSContext *ctx)
{
    JSValue proto;
    static const JSCFunctionListEntry js_image_proto_funcs[] = {
        JS_CFUNC_DEF("getLayersMesh", 0, js_image_getLayersMesh),
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

static void bind_goxel(JSContext *ctx)
{
    JSValue proto, global_obj, obj;
    static const JSCFunctionListEntry js_goxel_proto_funcs[] = {
        JS_CFUNC_DEF("registerFormat", 1, js_goxel_registerFormat),
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
    bind_mesh(g_ctx);
    bind_image(g_ctx);
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

int script_run(const char *filename, int argc, const char **argv)
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
