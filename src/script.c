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

#include "../ext_src/quickjs/quickjs.h"
#include "../ext_src/quickjs/quickjs-libc.h"

#define STB_DS_IMPLEMENTATION
#include "../ext_src/stb/stb_ds.h"

static JSRuntime *g_rt = NULL;
static JSContext *g_ctx = NULL;

typedef struct klass klass_t;
typedef struct attribute attribute_t;


/*
 * Represent a bound attribute of a class.
 */
struct attribute {
    const char *name;
    klass_t *klass;
    int flags;

    struct {
        int offset;
        int size;
    } member;

    JSValue (*get)(JSContext *ctx, JSValueConst this_val, int magic);
    JSValue (*set)(JSContext *ctx, JSValueConst this_val, JSValueConst val,
                   int magic);
    JSCFunction *fn;
    int magic;
};

#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

/*
 * Info struct for all the bound classes.
 */
struct klass {
    JSClassID id;
    JSClassDef def;
    JSCFunction *ctor;
    JSValue (*ctor_from_ptr)(
            JSContext * ctx, JSValueConst owner, void *ptr, size_t size);
    attribute_t attributes[];
};

/*
 * Base struct for all reference counted objects.
 */
typedef struct {
    int ref;
} obj_t;

/*
 * Pre declaration of all the registered classes.
 */
static klass_t vec_klass;
static klass_t box_klass;
static klass_t image_klass;
static klass_t palette_klass;
static klass_t layer_klass;
static klass_t volume_klass;
static klass_t goxel_klass;

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

    vec = JS_GetOpaque2(ctx, val, vec_klass.id);
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
    ret = JS_NewObjectClass(ctx, vec_klass.id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static void js_vec_finalizer(JSRuntime *rt, JSValue val)
{
    vec_t *vec = JS_GetOpaque(val, vec_klass.id);
    if (JS_IsUndefined(vec->owner)) js_free_rt(rt, vec->values);
    JS_FreeValueRT(rt, vec->owner);
    js_free_rt(rt, vec);
}

static JSValue js_vec_from_ptr(
        JSContext *ctx, JSValueConst owner, void *ptr, size_t size)
{
    JSValue ret;
    vec_t *vec;

    assert(ptr != NULL);
    vec = js_mallocz(ctx, sizeof(*vec));
    vec->size = size / sizeof(float);
    assert(!JS_IsUndefined(owner));
    vec->owner = JS_DupValue(ctx, owner);
    vec->values = ptr;
    ret = JS_NewObjectClass(ctx, vec_klass.id);
    JS_SetOpaque(ret, vec);
    return ret;
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
    ret = JS_NewObjectClass(ctx, vec_klass.id);
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
    vec->values[3] = w;
    ret = JS_NewObjectClass(ctx, vec_klass.id);
    JS_SetOpaque(ret, vec);
    return ret;
}

static JSValue js_vec_get(JSContext *ctx, JSValueConst this_val, int idx)
{
    vec_t *vec = JS_GetOpaque2(ctx, this_val, vec_klass.id);
    if (!vec)
        return JS_EXCEPTION;
    if (idx >= vec->size)
        return JS_EXCEPTION;
    return JS_NewFloat64(ctx, vec->values[idx]);
}

static JSValue js_vec_set(
        JSContext *ctx, JSValueConst this_val, JSValue val, int idx)
{
    vec_t *vec;
    double v;

    vec = JS_GetOpaque2(ctx, this_val, vec_klass.id);
    if (!vec)
        return JS_EXCEPTION;
    if (idx >= vec->size)
        return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &v, val))
        return JS_EXCEPTION;
    vec->values[idx] = v;
    return JS_UNDEFINED;
}

static klass_t vec_klass = {
    .def.class_name = "Vec",
    .def.finalizer = js_vec_finalizer,
    .ctor = js_vec_ctor,
    .ctor_from_ptr = js_vec_from_ptr,
    .attributes = {
        {"x", .get=js_vec_get, .set=js_vec_set, .magic=0},
        {"y", .get=js_vec_get, .set=js_vec_set, .magic=1},
        {"z", .get=js_vec_get, .set=js_vec_set, .magic=2},
        {"w", .get=js_vec_get, .set=js_vec_set, .magic=3},
        {"r", .get=js_vec_get, .set=js_vec_set, .magic=0},
        {"g", .get=js_vec_get, .set=js_vec_set, .magic=1},
        {"b", .get=js_vec_get, .set=js_vec_set, .magic=2},
        {"a", .get=js_vec_get, .set=js_vec_set, .magic=3},
        { .name = NULL }
    },
};

static JSValue js_box_from_ptr(
        JSContext *ctx, JSValueConst owner, void *ptr, size_t size)
{
    JSValue ret;
    box_t *box;
    if (ptr == NULL) return JS_NULL;
    assert(size == 16 * sizeof(float));
    box = js_mallocz(ctx, sizeof(*box));
    box->owner = JS_DupValue(ctx, owner);
    if (JS_IsUndefined(owner)) {
        box->mat = calloc(1, size);
        memcpy(box->mat, ptr, size);
    } else {
        box->mat = ptr;
    }
    ret = JS_NewObjectClass(ctx, box_klass.id);
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
    box = JS_GetOpaque(this_val, box_klass.id);
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
    box_t *box = JS_GetOpaque(val, box_klass.id);
    if (JS_IsUndefined(box->owner)) js_free_rt(rt, box->mat);
    JS_FreeValueRT(rt, box->owner);
    js_free_rt(rt, box);
}

static klass_t box_klass = {
    .def.class_name = "Box",
    .def.finalizer = js_box_finalizer,
    .ctor_from_ptr = js_box_from_ptr,
    .attributes = {
        {"iterVoxels", .fn=js_box_iterVoxels},
        {"worldToLocal", .fn=js_box_worldToLocal},
        { .name = NULL }
    },
};

static JSValue js_volume_ctor(JSContext *ctx, JSValueConst new_target,
                            int argc, JSValueConst *argv)
{
    JSValue ret;
    volume_t *volume;
    volume = volume_new();
    ret = JS_NewObjectClass(ctx, volume_klass.id);
    JS_SetOpaque(ret, volume);
    return ret;
}

static void js_volume_finalizer(JSRuntime *ctx, JSValue this_val)
{
    volume_t *volume;
    volume = JS_GetOpaque(this_val, volume_klass.id);
    volume_delete(volume);
}

static JSValue js_volume_copy(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue ret;
    volume_t *volume;
    volume = JS_GetOpaque(this_val, volume_klass.id);
    volume = volume_copy(volume);
    ret = JS_NewObjectClass(ctx, volume_klass.id);
    JS_SetOpaque(ret, volume);
    return ret;
}

static JSValue js_volume_iter(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    volume_t *volume;
    JSValue args[2];
    volume_iterator_t iter;
    int pos[3];
    uint8_t value[4];

    volume = JS_GetOpaque(this_val, volume_klass.id);
    iter = volume_get_iterator(volume,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, value);
        if (value[3] == 0) continue;
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
    volume = JS_GetOpaque2(ctx, this_val, volume_klass.id);
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

    volume = JS_GetOpaque2(ctx, this_val, volume_klass.id);
    path = JS_ToCString(ctx, argv[0]);
    if (argc > 1)
        format = JS_ToCString(ctx, argv[1]);

    f = file_format_get(path, format, "w");
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

static klass_t volume_klass = {
    .def.class_name = "Volume",
    .def.finalizer = js_volume_finalizer,
    .ctor = js_volume_ctor,
    .attributes = {
        {"copy", .fn=js_volume_copy},
        {"iter", .fn=js_volume_iter},
        {"setAt", .fn=js_volume_setAt},
        {"save", .fn=js_volume_save},
        { .name = NULL }
    }
};

static void js_image_finalizer(JSRuntime *ctx, JSValue this_val)
{
    image_t *image;
    image = JS_GetOpaque(this_val, image_klass.id);
    image_delete(image);
}

static JSValue js_image_addLayer(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    image_t *image;
    layer_t *layer;
    JSValue ret;

    image = JS_GetOpaque(this_val, image_klass.id);
    layer = image_add_layer(image, NULL);
    ret = JS_NewObjectClass(ctx, layer_klass.id);
    layer->ref++;
    JS_SetOpaque(ret, (void*)layer);
    return ret;
}

static JSValue js_image_getLayersVolume(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue ret;
    volume_t *volume;
    image_t *image;

    image = JS_GetOpaque(this_val, image_klass.id);
    volume = volume_copy(goxel_get_layers_volume(image));
    ret = JS_NewObjectClass(ctx, volume_klass.id);
    JS_SetOpaque(ret, (void*)volume);
    return ret;
}

static klass_t image_klass = {
    .def.class_name = "Image",
    .def.finalizer = js_image_finalizer,
    .attributes = {
        {"addLayer", .fn=js_image_addLayer},
        {"activeLayer", .klass=&layer_klass, MEMBER(image_t, active_layer)},
        {"getLayersVolume", .fn=js_image_getLayersVolume},
        {"selectionBox", .klass=&box_klass, MEMBER(image_t, selection_box)},
        { .name = NULL }
    }
};

static void js_layer_finalizer(JSRuntime *ctx, JSValue this_val)
{
    layer_t *layer;
    layer = JS_GetOpaque(this_val, layer_klass.id);
    layer_delete(layer);
}

static klass_t layer_klass = {
    .def.class_name = "Layer",
    .def.finalizer = js_layer_finalizer,
    .attributes = {
        {"volume", .klass=&volume_klass, MEMBER(layer_t, volume)},
        { .name = NULL }
    }
};

/* Modify the palette struct to store the created vec_value */
typedef struct {
    palette_t *palette;
    JSValue color_vec;  // Store the created color vector
} js_palette_t;

static JSValue js_palette_get_color(JSContext *ctx, JSValueConst this_val, int magic)
{
    js_palette_t *js_palette;
    uint8_t *color;
    
    js_palette = JS_GetOpaque2(ctx, this_val, palette_klass.id);
    if (!js_palette) {
        LOG_E("Error in palette_get_color: No palette object found");
        return JS_EXCEPTION;
    }
    
    color = goxel.painter.color;
    
    if (!color) {
        LOG_E("Error in palette_get_color: color is NULL");
        return JS_EXCEPTION;
    }
    
    // If we already have a color vec, free it first to prevent memory leaks
    if (!JS_IsUndefined(js_palette->color_vec)) {
        JS_FreeValue(ctx, js_palette->color_vec);
    }
    
    // Create a new vec and store it in our js_palette struct
    js_palette->color_vec = new_js_vec4(ctx, color[0], color[1], color[2], color[3]);
    
    return JS_DupValue(ctx, js_palette->color_vec);
}

static void js_palette_finalizer(JSRuntime *rt, JSValue this_val)
{
    js_palette_t *js_palette;
    
    js_palette = JS_GetOpaque(this_val, palette_klass.id);
    if (!js_palette)
        return;

    LOG_I("Palette finalizer called for palette at %p", (void*)js_palette);
    
    // Free the color vector value if it exists
    if (!JS_IsUndefined(js_palette->color_vec)) {
        JS_FreeValueRT(rt, js_palette->color_vec);
    }
    
    // Free the wrapper struct
    js_free_rt(rt, js_palette);
}

static JSValue js_palette_from_ptr(
        JSContext *ctx, JSValueConst owner, void *ptr, size_t size)
{
    JSValue ret;
    js_palette_t *js_palette;
    
    js_palette = js_mallocz(ctx, sizeof(*js_palette));
    js_palette->palette = ptr;
    js_palette->color_vec = JS_UNDEFINED;
    
    ret = JS_NewObjectClass(ctx, palette_klass.id);
    JS_SetOpaque(ret, js_palette);
    return ret;
}

static klass_t palette_klass = {
    .def.class_name = "Palette",
    .def.finalizer = js_palette_finalizer,
    .ctor_from_ptr = js_palette_from_ptr,
    .attributes = {
        {"color", .get=js_palette_get_color, .set=NULL, .magic=0},
        { .name = NULL }
    }
};

typedef struct {
    file_format_t format;
    JSValue data;
} script_file_format_t;

int script_format_import_func(const file_format_t *format_, image_t *image,
                              const char *path)
{
    JSContext *ctx = g_ctx;
    const script_file_format_t *format = (void*)format_;
    JSValue js_import, js_image, js_path, val;
    JSValueConst argv[2];
    js_image = JS_NewObjectClass(ctx, image_klass.id);
    goxel.image->ref++;
    JS_SetOpaque(js_image, (void*)goxel.image);
    js_path = JS_NewString(ctx, path);
    argv[0] = js_image;
    argv[1] = js_path;
    js_import = JS_GetPropertyStr(ctx, format->data, "import");
    val = JS_Call(ctx, js_import, JS_NULL, 2, argv);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, js_import);
    JS_FreeValue(ctx, js_image);
    JS_FreeValue(ctx, js_path);
    return 0;
}

int script_format_export_func(const file_format_t *format_,
                              const image_t *img, const char *path)
{
    JSContext *ctx = g_ctx;
    const script_file_format_t *format = (void*)format_;
    JSValue export, image;
    JSValueConst argv[2];

    image = JS_NewObjectClass(ctx, image_klass.id);
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
    JSValue val, ext_val;
    script_file_format_t *format;
    uint32_t idx;

    args = argv[0];
    name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, args, "name"));

    LOG_I("Register format %s", name);
    format = calloc(1, sizeof(*format));
    *format = (script_file_format_t) {
        .format = {
            .name = name,
        },
        .data = JS_DupValue(ctx, args),
    };

    val = JS_GetPropertyStr(ctx, args, "exts");
    if (JS_IsArray(ctx, val)) {
        for (idx = 0; idx < ARRAY_SIZE(format->format.exts); idx++) {
            ext_val = JS_GetPropertyUint32(ctx, val, idx);
            ext = JS_ToCString(ctx, ext_val);
            format->format.exts[idx] = ext;
        }
    }
    JS_FreeValue(ctx, val);

    val = JS_GetPropertyStr(ctx, args, "import");
    if (!JS_IsUndefined(val))
        format->format.import_func = script_format_import_func;
    JS_FreeValue(ctx, val);

    val = JS_GetPropertyStr(ctx, args, "export");
    if (!JS_IsUndefined(val))
        format->format.export_func = script_format_export_func;
    JS_FreeValue(ctx, val);

    file_format_register(&format->format);
    return JS_UNDEFINED;
}

static JSValue js_goxel_registerScript(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValueConst data;
    script_t script; memset(&script, 0, sizeof(script_t));
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

static klass_t goxel_klass = {
    .def.class_name = "Goxel",
    .attributes = {
        {"image", .klass=&image_klass, MEMBER(goxel_t, image)},
        {"palette", .klass=&palette_klass, MEMBER(goxel_t, palette)},
        {"registerFormat", .fn=js_goxel_registerFormat},
        {"registerScript", .fn=js_goxel_registerScript},
        { .name = NULL }
    },
};

static JSValue attr_getter(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSValue proto, ret;
    const klass_t *klass;
    void *this, *ptr;
    const attribute_t *attr;

    proto = JS_GetPrototype(ctx, this_val);
    klass = JS_GetOpaque(proto, 1);
    if (!klass) {
        JS_FreeValue(ctx, proto);
        return JS_EXCEPTION;
    }
    
    this = JS_GetOpaque(this_val, klass->id);
    JS_FreeValue(ctx, proto);
   attr = &klass->attributes[magic];

    if (!this) {
        LOG_E("No object instance found for class %s", klass->def.class_name);
        return JS_EXCEPTION;
    }
    
    attr = &klass->attributes[magic];
    
    assert(this);


    if (attr->klass && attr->klass->ctor_from_ptr && attr->member.size) {
        ptr = (char*)this + attr->member.offset; // Cast 'this' to char* for pointer arithmetic
        ret = attr->klass->ctor_from_ptr(ctx, this_val, ptr, attr->member.size);
        return ret;
    }

    if (attr->klass && attr->member.size) {
        ptr = *(void**)((char*)this + attr->member.offset); // Cast 'this' to char* for pointer arithmetic
        if (!ptr) return JS_NULL;
        ((obj_t*)ptr)->ref++;
        ret = JS_NewObjectClass(ctx, attr->klass->id);
        JS_SetOpaque(ret, ptr);
        return ret;
    }

    LOG_E("No attribute handler found for %s", attr->name);
    return JS_EXCEPTION;
}

static JSValue attr_setter(JSContext *ctx, JSValueConst this_val,
                           JSValueConst val, int magic)
{
    // Not implemented yet.
    return JS_EXCEPTION;
}

static void init_klass(JSContext *ctx, klass_t *klass)
{
    JSValue proto, getter, setter, obj_class, global_obj;
    attribute_t *attr;
    int i;
    JSAtom name;

    JS_NewClassID(&klass->id);
    JS_NewClass(JS_GetRuntime(ctx), klass->id, &klass->def);
    proto = JS_NewObject(ctx);
    JS_SetOpaque(proto, klass);
    JS_SetClassProto(ctx, klass->id, proto);

    if (klass->ctor) {
        obj_class = JS_NewCFunction2(ctx, klass->ctor, klass->def.class_name, 0,
                                     JS_CFUNC_constructor, 0);
        JS_SetConstructor(ctx, obj_class, proto);
        global_obj = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global_obj, klass->def.class_name, obj_class);
        JS_FreeValue(ctx, global_obj);
    }

    for (i = 0, attr = &klass->attributes[0]; attr->name; attr++, i++) {
        name = JS_NewAtom(ctx, attr->name);
        if (attr->get) {
            getter = JS_NewCFunction2(ctx, (void*)attr->get, NULL, 0,
                                      JS_CFUNC_getter_magic, attr->magic);
            setter = JS_NewCFunction2(ctx, (void*)attr->set, NULL, 0,
                                      JS_CFUNC_setter_magic, attr->magic);
            JS_DefinePropertyGetSet(ctx, proto, name, getter, setter, 0);
        } else if (attr->fn) {
            JS_DefinePropertyValue(ctx, proto, name,
                           JS_NewCFunction(ctx, attr->fn, NULL, 0),
                           JS_DEF_CFUNC);
        } else {
            getter = JS_NewCFunction2(ctx, (void*)attr_getter, NULL, 0,
                                      JS_CFUNC_getter_magic, i);
            setter = JS_NewCFunction2(ctx, (void*)attr_setter, NULL, 0,
                                      JS_CFUNC_setter_magic, i);
            JS_DefinePropertyGetSet(ctx, proto, name, getter, setter, 0);
        }
    }
}

static void init_runtime(void)
{
    JSContext *ctx;
    JSValue obj, global_obj;

    if (g_ctx) return;
    g_rt = JS_NewRuntime();
    g_ctx = JS_NewContext(g_rt);
    ctx = g_ctx;
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    init_klass(ctx, &vec_klass);
    init_klass(ctx, &box_klass);
    init_klass(ctx, &volume_klass);
    init_klass(ctx, &layer_klass);
    init_klass(ctx, &image_klass);
    init_klass(ctx, &palette_klass);
    init_klass(ctx, &goxel_klass);

    // Add global 'goxel' object.
    obj = JS_NewObjectClass(ctx, goxel_klass.id);
    JS_SetOpaque(obj, &goxel);
    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "goxel", obj);
    JS_FreeValue(ctx, global_obj);
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

static int on_dir(void *arg, const char *path)
{
    LOG_I("Loading scripts from %s\n", path);
    sys_list_dir(path, on_user_script, NULL);
    return 0;
}

void script_init(void)
{
    assets_list("data/scripts/", NULL, on_script);
    sys_iter_paths(SYS_LOCATION_CONFIG, SYS_DIR, "scripts", NULL, on_dir);
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
    script_t *script = NULL;
    JSContext *ctx = g_ctx;
    JSValue val;

    for (i = 0; i < arrlen(g_scripts); i++) {
        script = &g_scripts[i];
        if (strcmp(script->name, name) == 0) break;
    }
    if (i == arrlen(g_scripts)) return -1;
    assert(script);

    LOG_I("Run script %s", name);
    val = JS_Call(ctx, script->execute_fn, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(val)) {
        LOG_E("Error executing script");
        js_std_dump_error(ctx);
        ret = -1;
    }
    JS_FreeValue(ctx, val);
    return ret;
}
