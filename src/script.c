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

static JSClassID js_mesh_class_id;

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
    err = f->export_func(img, path);
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

int script_run(const char *filename, int argc, const char **argv)
{
    char *script;
    int ret = 0, size;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue val;

    script = read_file(filename, &size);
    if (!script) {
        fprintf(stderr, "Cannot read '%s'\n", filename);
        return -1;
    }

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    bind_mesh(ctx);
    js_std_add_helpers(ctx, argc, (char**)argv);
    val = JS_Eval(ctx, script, size, filename, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        ret = -1;
    }
    JS_FreeValue(ctx, val);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(script);
    return ret;
}
