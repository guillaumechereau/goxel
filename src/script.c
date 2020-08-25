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

    struct {
        klass_t *klass;
        void *(*add)(void *parent, void *val);
    } list;

    JSValue (*getter)(JSContext *ctx, JSValueConst this_val);
    JSCFunction *fn;
};

struct klass {
    JSClassID id;
    JSClassDef def;
    JSValue (*create)(JSContext *ctx, void *parent, const attribute_t *attr);
    JSCFunction *ctor;
    attribute_t attributes[];
};

typedef struct list_el list_el_t;
struct list_el {
    list_el_t *next, *prev;
};

typedef struct {
    void *parent;
    list_el_t **ptr;
    const attribute_t *attr;
} list_t;

#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

static klass_t image_klass;
static klass_t list_klass;
static klass_t layer_klass;
static klass_t volume_klass;

static JSValue new_obj_ref(JSContext *ctx, void *obj, const klass_t *klass)
{
    JSValue ret;
    ret = JS_NewObjectClass(ctx, klass->id);
    JS_SetOpaque(ret, obj);
    return ret;
}

static JSValue list_create(JSContext *ctx, void *parent,
                           const attribute_t *attr)
{
    list_t *list;
    list = calloc(1, sizeof(*list));
    list->parent = parent;
    list->attr = attr;
    list->ptr = parent + attr->member.offset;
    return new_obj_ref(ctx, list, &list_klass);
}

static JSValue list_new(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    list_t *list;
    list_el_t *el;

    list = JS_GetOpaque(this_val, list_klass.id);
    el = list->attr->list.add(list->parent, NULL);
    return new_obj_ref(ctx, el, list->attr->list.klass);
}

static JSValue list_length_get(JSContext *ctx, JSValueConst this_val)
{
    int length;
    list_t *list;
    list_el_t *el;
    list = JS_GetOpaque(this_val, list_klass.id);
    DL_COUNT(*list->ptr, el, length);
    return JS_NewInt32(ctx, length);
}

static klass_t list_klass = {
    .def = { "List" },
    .create = list_create,
    .attributes = {
        {"length", .getter=list_length_get},
        {"new", .fn=list_new},
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

static klass_t image_klass = {
    .def = { "Image" },
    .attributes = {
        {"layers", .klass=&list_klass, MEMBER(image_t, layers),
         .list={.klass=&layer_klass, .add=(void*)image_add_layer}},
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
    return new_obj_ref(ctx, mesh, &volume_klass);
}

static JSValue js_volume_fill(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    int aabb[2][3] = {}, pos[3];
    uint8_t c[4];
    JSValue args[2], pos_buf, aabb_buf, c_val;

    mesh = JS_GetOpaque(this_val, volume_klass.id);
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
    mesh = JS_GetOpaque(this_val, volume_klass.id);
    mesh_set_at(mesh, NULL, pos, v);
    return JS_UNDEFINED;
}

static JSValue js_volume_save(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    mesh_t *mesh;
    const char *path;

    mesh = JS_GetOpaque(this_val, volume_klass.id);
    (void)mesh;
    path = JS_ToCString(ctx, argv[0]);
    action_exec2("mesh_save", "ps", mesh, path); // XXX: should be removed.
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

static void js_volume_finalizer(JSRuntime *rt, JSValue val)
{
    mesh_t *mesh = JS_GetOpaque(val, volume_klass.id);
    mesh_delete(mesh);
}


static klass_t volume_klass = {
    .def = { "Volume", .finalizer = js_volume_finalizer },
    .ctor = volume_ctor,
    .attributes = {
        {"fill", .fn=js_volume_fill},
        {"setAt", .fn=js_volume_set_at},
        {"save", .fn=js_volume_save},
        {}
    },
};

static klass_t layer_klass = {
    .def = { "Layer" },
    .attributes = {
        {"name", .flags=ATTR_STR_BUF, MEMBER(layer_t, name)},
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
    obj = JS_GetOpaque(this_val, klass->id);
    JS_FreeValue(ctx, proto);

    attr = &klass->attributes[magic];
    if (attr->klass && attr->klass->create) {
        return attr->klass->create(ctx, obj, attr);
    }

    if (attr->flags & ATTR_STR_BUF) {
        ptr = obj + attr->member.offset;
        return JS_NewStringLen(ctx, (void*)ptr, attr->member.size);
    }

    if (attr->klass && attr->member.size) {
        ptr = obj + attr->member.offset;
        return new_obj_ref(ctx, *(void**)ptr, attr->klass);
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
            JS_DefinePropertyGetSet(ctx, proto, name, getter, JS_UNDEFINED, 0);
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

    goxel_obj = new_obj_ref(ctx, &goxel, &goxel_klass);
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

int script_run(const char *filename, int argc, const char **argv)
{
    char *input;
    int size;
    int flags = 0;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue res_val;

    rt = JS_NewRuntime();
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetRuntimeInfo(rt, filename);
    js_std_add_helpers(ctx, -1, NULL);

    input = read_file(filename, &size);
    if (!input) {
        LOG_E("Cannot read '%d'", filename);
        return -1;
    }
    res_val = JS_Eval(ctx, input, size, filename, flags);
    if (JS_IsException(res_val)) {
        dump_exception(ctx);
    }

    JS_FreeValue(ctx, res_val);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(input);
    return 0;
}
