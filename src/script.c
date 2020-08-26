/* Goxel 3D voxels editor
 *
 * copyright (c) 2020 Guillaume Chereau <guillaume@noctua-software.com>
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
#include "../ext_src/quickjs/quickjs.h"

JSValue JS_NewInt32Array(JSContext *ctx, JSValueConst arrayBuffer,
                         int offset, int length);

enum {
    ATTR_STR_BUF =      1 << 0,
};

typedef struct klass klass_t;
typedef struct attribute attribute_t;

struct attribute {
    const char *name;
    klass_t *klass;
    int flags;

    struct {
        int offset;
        int size;
    } member;

    JSValue (*getter)(JSContext *ctx, JSValueConst this_val);
    JSValue (*setter)(JSContext *ctx, JSValueConst this_val, JSValueConst val);
    JSCFunction *fn;
};

struct klass {
    JSClassID id;
    JSClassDef def;
    JSCFunction *ctor;
    attribute_t attributes[];
};

typedef struct list_el list_el_t;
struct list_el {
    list_el_t *next, *prev;
};

typedef struct {
    const klass_t *klass;
    void *parent;
    list_el_t **active; // Point to parent attribute for active item.
    list_el_t **ptr;
    void *(*add_fn)(void *parent, void *val);
    void (*delete_fn)(void *parent, void *val);
    void *(*copy_fn)(void *val);
} list_t;

#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

static klass_t image_klass;
static klass_t list_klass;
static klass_t layer_klass;
static klass_t volume_klass;

typedef struct obj {
    void *ptr;
    bool owned;
} obj_t;

static JSValue obj_new(JSContext *ctx, void *ptr, const klass_t *klass,
                       bool owned)
{
    JSValue ret;
    obj_t *obj;

    obj = calloc(1, sizeof(*obj));
    obj->ptr = ptr;
    obj->owned = owned;
    ret = JS_NewObjectClass(ctx, klass->id);
    JS_SetOpaque(ret, obj);
    return ret;
}

static void *obj_delete(JSValue val, const klass_t *klass)
{
    void *ret;
    obj_t *obj;
    obj = JS_GetOpaque(val, klass->id);
    ret = obj->owned ? obj->ptr : NULL;
    free(obj);
    return ret;
}

static void *obj_get_ptr(JSValue val, const klass_t *klass)
{
    obj_t *obj;
    obj = JS_GetOpaque(val, klass->id);
    return obj->ptr;
}

static void *obj_take_ptr(JSValue val, const klass_t *klass)
{
    obj_t *obj;
    obj = JS_GetOpaque(val, klass->id);
    obj->owned = false;
    return obj->ptr;
}

static JSValue js_list_remove(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    list_t *list;
    list_el_t *el;
    list = obj_get_ptr(this_val, &list_klass);
    el = obj_get_ptr(argv[0], list->klass);
    list->delete_fn(list->parent, el);
    return JS_UNDEFINED;
}

static JSValue js_list_duplicate(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue active_val, new_val;
    list_t *list;
    list_el_t *el;

    // XXX: to simplify we could let the add_xxx function directly
    // check if the passed item already exists.
    list = obj_get_ptr(this_val, &list_klass);
    active_val = obj_new(ctx, *list->active, list->klass, false);
    new_val = JS_Invoke(ctx, active_val, JS_NewAtom(ctx, "copy"), 0, NULL);
    el = obj_take_ptr(new_val, list->klass);
    list->add_fn(list->parent, el);
    JS_FreeValue(ctx, active_val);
    JS_FreeValue(ctx, new_val);
    *list->active = el;
    return JS_UNDEFINED;
}

static JSValue js_list_move(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    int32_t d;
    list_t *list;
    list_el_t *el, *other = NULL;

    list = obj_get_ptr(this_val, &list_klass);
    el = obj_get_ptr(argv[0], list->klass);
    JS_ToInt32(ctx, &d, argv[1]); // TODO: Add error check.
    if (d == -1) {
        other = el->next;
        SWAP(other, el);
    } else if (el != *list->ptr) {
        other = el->prev;
    }
    if (!other || !el) return JS_UNDEFINED;
    DL_DELETE(*list->ptr, el);
    DL_PREPEND_ELEM(*list->ptr, other, el);
    return JS_UNDEFINED;
}

static JSValue js_list_add(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    list_t *list;
    list_el_t *el;
    list = obj_get_ptr(this_val, &list_klass);
    el = obj_take_ptr(argv[0], list->klass);
    el = list->add_fn(list->parent, el);
    return obj_new(ctx, el, list->klass, false);
}

static JSValue list_length_get(JSContext *ctx, JSValueConst this_val)
{
    int length;
    list_t *list;
    list_el_t *el;
    list = obj_get_ptr(this_val, &list_klass);
    DL_COUNT(*list->ptr, el, length);
    return JS_NewInt32(ctx, length);
}

static JSValue js_list_active(JSContext *ctx, JSValueConst this_val)
{
    list_t *list;
    list = obj_get_ptr(this_val, &list_klass);
    return obj_new(ctx, *list->active, list->klass, false);
}

static JSValue js_list_active_set(JSContext *ctx, JSValueConst this_val,
                                  JSValueConst val)
{
    list_t *list;
    list_el_t *el;
    list = obj_get_ptr(this_val, &list_klass);
    el = obj_get_ptr(val, list->klass);
    *list->active = el;
    return JS_UNDEFINED;
}

static void js_list_finalizer(JSRuntime *rt, JSValue val)
{
    list_t *list;
    if ((list = obj_delete(val, &list_klass))) {
        free(list);
    }
}

static klass_t list_klass = {
    .def = { "List", .finalizer = js_list_finalizer },
    .attributes = {
        {"add", .fn=js_list_add},
        {"active", .getter=js_list_active, .setter=js_list_active_set},
        {"duplicate", .fn=js_list_duplicate},
        {"length", .getter=list_length_get},
        {"move", .fn=js_list_move},
        {"remove", .fn=js_list_remove},
        {},
    },
};

static klass_t goxel_klass = {
    .def = { "Goxel" },
    .attributes = {
        {"image", .klass=&image_klass, MEMBER(goxel_t, image)},
        {}
    },
};


static JSValue js_image_layers_get(JSContext *ctx, JSValueConst this_val)
{
    list_t *list;
    image_t *image;

    image = obj_get_ptr(this_val, &image_klass);
    list = calloc(1, sizeof(*list));
    list->klass = &layer_klass;
    list->parent = image;
    list->ptr = (void*)&image->layers;
    list->active = (void*)&image->active_layer;
    list->add_fn = (void*)image_add_layer;
    list->delete_fn = (void*)image_delete_layer;
    return obj_new(ctx, list, &list_klass, true);
}

static JSValue js_image_merge_visible_layers(
        JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    image_t *image;
    image = obj_get_ptr(this_val, &image_klass);
    image_merge_visible_layers(image);
    return JS_UNDEFINED;
}

static void js_image_finalizer(JSRuntime *rt, JSValue val)
{
    image_t *image;
    if ((image = obj_delete(val, &image_klass))) {
        image_delete(image);
    }
}

static klass_t image_klass = {
    .def = { "Image", .finalizer = js_image_finalizer },
    .attributes = {
        {"layers", .getter=js_image_layers_get},
        {"mergeVisibleLayers", .fn=js_image_merge_visible_layers},
        {}
    },
};

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

static JSValue volume_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    mesh = mesh_new();
    return obj_new(ctx, mesh, &volume_klass, true);
}

static JSValue js_volume_clear(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    mesh = obj_get_ptr(this_val, &volume_klass);
    mesh_clear(mesh);
    return JS_UNDEFINED;
}

static JSValue js_volume_fill(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    int aabb[2][3] = {}, pos[3];
    uint8_t c[4];
    JSValue args[2], pos_buf, aabb_buf, c_val;

    mesh = obj_get_ptr(this_val, &volume_klass);
    get_vec_int(ctx, argv[0], 3, aabb[1]);

    aabb_buf = JS_NewArrayBuffer(ctx, (void*)aabb, sizeof(aabb),
                                 NULL, NULL, true);
    pos_buf = JS_NewArrayBuffer(ctx, (void*)pos, sizeof(pos),
                                NULL, NULL, true);
    args[0] = JS_NewInt32Array(ctx, pos_buf, 0, 3);
    args[1] = JS_NewInt32Array(ctx, aabb_buf, 12, 3);

    for (pos[2] = aabb[0][2]; pos[2] < aabb[1][2]; pos[2]++)
    for (pos[1] = aabb[0][1]; pos[1] < aabb[1][1]; pos[1]++)
    for (pos[0] = aabb[0][0]; pos[0] < aabb[1][0]; pos[0]++) {
        c_val = JS_Call(ctx, argv[1], JS_UNDEFINED, 2, args);
        get_vec_uint8(ctx, c_val, 4, c);
        mesh_set_at(mesh, NULL, pos, c);
        JS_FreeValue(ctx, c_val);
    }

    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, aabb_buf);
    JS_FreeValue(ctx, pos_buf);

    return JS_UNDEFINED;
}

static JSValue js_volume_set_at(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    int pos[3];
    uint8_t v[4];

    get_vec_int(ctx, argv[0], 3, pos);
    get_vec_uint8(ctx, argv[1], 4, v);
    mesh = obj_get_ptr(this_val, &volume_klass);
    mesh_set_at(mesh, NULL, pos, v);
    return JS_UNDEFINED;
}

static JSValue js_volume_save(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    const char *path;

    mesh = obj_get_ptr(this_val, &volume_klass);
    path = JS_ToCString(ctx, argv[0]);
    action_exec2("mesh_save", "ps", mesh, path); // XXX: should be removed.
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static void js_volume_finalizer(JSRuntime *rt, JSValue val)
{
    mesh_t *mesh;
    if ((mesh = obj_delete(val, &volume_klass))) {
        mesh_delete(mesh);
    }
}


static klass_t volume_klass = {
    .def = { "Volume", .finalizer = js_volume_finalizer },
    .ctor = volume_ctor,
    .attributes = {
        {"clear", .fn=js_volume_clear},
        {"fill", .fn=js_volume_fill},
        {"setAt", .fn=js_volume_set_at},
        {"save", .fn=js_volume_save},
        {}
    },
};

static void js_layer_finalizer(JSRuntime *rt, JSValue val)
{
    layer_t *layer;
    if ((layer = obj_delete(val, &layer_klass))) {
        layer_delete(layer);
    }
}

static JSValue js_layer_clone(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    layer_t *layer;
    layer = obj_get_ptr(this_val, &layer_klass);
    layer = layer_clone(layer);
    return obj_new(ctx, layer, &layer_klass, true);
}

static JSValue js_layer_unclone(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    layer_t *layer;
    layer = obj_get_ptr(this_val, &layer_klass);
    layer_unclone(layer);
    return JS_UNDEFINED;
}

static JSValue js_layer_copy(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    layer_t *layer;
    layer = obj_get_ptr(this_val, &layer_klass);
    layer = layer_copy(layer);
    return obj_new(ctx, layer, &layer_klass, true);
}

static JSValue layer_ctor(JSContext *ctx, JSValueConst new_target,
                          int argc, JSValueConst *argv)
{
    layer_t *layer;
    layer = layer_new(NULL);
    return obj_new(ctx, layer, &layer_klass, true);
}

static klass_t layer_klass = {
    .def = { "Layer", .finalizer = js_layer_finalizer },
    .ctor = layer_ctor,
    .attributes = {
        {"clone", .fn=js_layer_clone},
        {"unclone", .fn=js_layer_unclone},
        {"copy", .fn=js_layer_copy},
        {"name", .flags=ATTR_STR_BUF, MEMBER(layer_t, name)},
        {"parent", .klass=&layer_klass, MEMBER(layer_t, parent)},
        {"volume", .klass=&volume_klass, MEMBER(layer_t, mesh)},
        {}
    },
};

static JSValue obj_attr_getter(JSContext *ctx, JSValueConst this_val,
                               int magic)
{
    void *obj, *ptr;
    const klass_t *klass;
    const attribute_t *attr;
    JSValue proto;

    proto = JS_GetPrototype(ctx, this_val);
    klass = JS_GetOpaque(proto, 1);
    obj = obj_get_ptr(this_val, klass);
    JS_FreeValue(ctx, proto);

    attr = &klass->attributes[magic];

    if (attr->flags & ATTR_STR_BUF) {
        ptr = obj + attr->member.offset;
        return JS_NewStringLen(ctx, (void*)ptr, attr->member.size);
    }

    if (attr->klass && attr->member.size) {
        ptr = obj + attr->member.offset;
        ptr = *(void**)ptr;
        if (!ptr) return JS_NULL;
        return obj_new(ctx, ptr, attr->klass, false);
    }

    return JS_NewInt32(ctx, magic);
}

static JSValue obj_attr_setter(JSContext *ctx, JSValueConst this_val,
                               JSValueConst val, int magic)
{
    const klass_t *klass;
    const attribute_t *attr;
    void *obj, *ptr;
    JSValue proto;
    const char *str;
    size_t len;

    proto = JS_GetPrototype(ctx, this_val);
    klass = JS_GetOpaque(proto, 1);
    obj = JS_GetOpaque(this_val, klass->id);
    JS_FreeValue(ctx, proto);
    attr = &klass->attributes[magic];

    if (attr->flags & ATTR_STR_BUF) {
        ptr = obj + attr->member.offset;
        str = JS_ToCStringLen(ctx, &len, val);
        snprintf(ptr, attr->member.size, "%s", str);
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
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

    // XXX: cleanup this.
    for (i = 0, attr = &klass->attributes[0]; attr->name; attr++, i++) {
        name = JS_NewAtom(ctx, attr->name);
        if (attr->getter) {
            getter = JS_NewCFunction2(ctx, (void*)attr->getter, NULL, 0,
                                      JS_CFUNC_getter, 0);
            setter = JS_NewCFunction2(ctx, (void*)attr->setter, NULL, 0,
                                      JS_CFUNC_setter, 0);
            JS_DefinePropertyGetSet(ctx, proto, name, getter, setter, 0);
        } else if (attr->fn) {
            JS_DefinePropertyValue(ctx, proto, name,
                           JS_NewCFunction(ctx, attr->fn, NULL, 0),
                           JS_DEF_CFUNC);
        } else {
            getter = JS_NewCFunction2(ctx, (void*)obj_attr_getter, NULL, 0,
                                      JS_CFUNC_getter_magic, i);
            setter = JS_NewCFunction2(ctx, (void*)obj_attr_setter, NULL, 0,
                                      JS_CFUNC_setter_magic, i);
            JS_DefinePropertyGetSet(ctx, proto, name, getter, setter, 0);
        }
    }
}

// Taken as it is from quickjs-libc.c
static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    int i;
    const char *str;
    size_t len;

    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str)
            return JS_EXCEPTION;
        fwrite(str, 1, len, stdout);
        JS_FreeCString(ctx, str);
    }
    putchar('\n');
    return JS_UNDEFINED;
}

static void js_std_add_helpers(JSContext *ctx, int argc, char **argv)
{
    JSValue global_obj, console, goxel_obj;
    global_obj = JS_GetGlobalObject(ctx);
    console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                      JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "console", console);

    init_klass(ctx, &list_klass);
    init_klass(ctx, &goxel_klass);
    init_klass(ctx, &image_klass);
    init_klass(ctx, &layer_klass);
    init_klass(ctx, &volume_klass);

    goxel_obj = obj_new(ctx, &goxel, &goxel_klass, false);
    JS_SetPropertyStr(ctx, global_obj, "goxel", goxel_obj);

    JS_FreeValue(ctx, global_obj);
}

static  void dump_exception(JSContext *ctx)
{
    JSValue exception_val;
    const char *str;
    exception_val = JS_GetException(ctx);
    str = JS_ToCString(ctx, exception_val);
    if (str) {
        fprintf(stderr, "%s\n", str);
        JS_FreeCString(ctx, str);
    } else {
        fprintf(stderr, "[exception]\n");
    }
    JS_FreeValue(ctx, exception_val);
}

int script_run_str(const char *script, const char *filename)
{
    JSRuntime *rt;
    JSContext *ctx;
    JSValue res_val;
    int flags = 0;

    rt = JS_NewRuntime();
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetRuntimeInfo(rt, filename);
    js_std_add_helpers(ctx, -1, NULL);
    res_val = JS_Eval(ctx, script, strlen(script), filename, flags);
    if (JS_IsException(res_val)) {
        dump_exception(ctx);
    }

    JS_FreeValue(ctx, res_val);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

int script_run(const char *filename, int argc, const char **argv)
{
    char *input;
    int size, ret;

    input = read_file(filename, &size);
    if (!input) {
        LOG_E("Cannot read '%d'", filename);
        return -1;
    }
    ret = script_run_str(input, filename);
    free(input);
    return ret;
}
