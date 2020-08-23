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

typedef struct klass klass_t;

typedef struct {
    const char *name;
    int type;
    klass_t *klass;
    struct {
        int offset;
        int size;
    } member;
} attribute_t;

struct klass {
    JSClassID id;
    JSClassDef def;
    attribute_t attributes[];
};

#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

static klass_t image_klass;

static klass_t list_klass = {
    .def = { "List" },
    .attributes = {
        {}
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
        {"layers", .klass=&list_klass, MEMBER(image_t, layers)},
        {}
    },
};

static JSValue new_obj_ref(JSContext *ctx, void *obj, const klass_t *klass)
{
    JSValue ret;
    ret = JS_NewObjectClass(ctx, klass->id);
    JS_SetOpaque(ret, obj);
    return ret;
}

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
    if (attr->klass && attr->member.size) {
        ptr = obj + attr->member.offset;
        return new_obj_ref(ctx, ptr, attr->klass);
    }

    return JS_NewInt32(ctx, magic);
}

static void init_klass(JSContext *ctx, klass_t *klass)
{
    JSValue proto, getter;
    attribute_t *attr;
    int i;

    JS_NewClassID(&klass->id);
    JS_NewClass(JS_GetRuntime(ctx), klass->id, &klass->def);
    proto = JS_NewObject(ctx);
    JS_SetOpaque(proto, klass);
    JS_SetClassProto(ctx, klass->id, proto);

    for (i = 0, attr = &klass->attributes[0]; attr->name; attr++, i++) {
        getter = JS_NewCFunction2(ctx, (void*)obj_attr_getter, NULL, 0,
                                  JS_CFUNC_getter_magic, 0);
        JS_DefinePropertyGetSet(ctx, proto, JS_NewAtom(ctx, attr->name),
                                getter, JS_UNDEFINED, i);
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

/*
static JSValue gox_obj_attr_getter(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int magic,
                                   JSValue *func_data)
{
    const char *name;
    name = JS_ToCString(ctx, *func_data);
    if (strcmp(name, "image")) {
    }
    return JS_UNDEFINED;
}
*/

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
