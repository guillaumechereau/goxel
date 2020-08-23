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

#if 0

#include "../ext_src/lua/lauxlib.h"
#include "../ext_src/lua/lua.h"
#include "../ext_src/lua/lualib.h"


static int l_goxel_call(lua_State *l)
{
    action_t *a;
    const char *id = lua_tostring(l, 1);
    lua_remove(l, 1);
    a = action_get(id, false);
    if (!a) luaL_error(l, "Cannot find action %s", id);
    return action_exec_lua(a, l);
}

static void add_globals(lua_State *l)
{
    const char *script;
    lua_pushcfunction(l, l_goxel_call);
    lua_setglobal(l, "goxel_call");
    script = assets_get("asset://data/other/script_header.lua", NULL);
    assert(script);
    luaL_loadstring(l, script);
    lua_pcall(l, 0, 0, 0);
}

/*
 * Function: script_run
 * Run a lua script from a file.
 */
int script_run(const char *filename, int argc, const char **argv)
{
    lua_State *l;
    char *script;
    int ret = 0, i;

    script = read_file(filename, 0);
    if (!script) {
        LOG_E("Cannot read '%d'", filename);
        return -1;
    }
    l = luaL_newstate();
    luaL_openlibs(l);
    add_globals(l);

    // Put the arguments.
    lua_newtable(l);
    for (i = 0; i < argc; i++) {
        lua_pushnumber(l, i + 1);
        lua_pushstring(l, argv[i]);
        lua_rawset(l, -3);
    }
    lua_setglobal(l, "arg");

    luaL_loadstring(l, script);
    if (lua_pcall(l, 0, 0, 0)) {
        fprintf(stderr, "ERROR:\n%s\n", lua_tostring(l, -1));
        ret = -1;
    }
    lua_close(l);
    free(script);
    return ret;
}

#endif

#include "../ext_src/quickjs/quickjs.h"

struct {

} klass_t;

static JSClassID gox_obj_class_id;

static void gox_obj_finalizer(JSRuntime *rt, JSValue val)
{
}

static JSClassDef gox_obj_class = {
    "Obj",
    .finalizer = gox_obj_finalizer,
}; 

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

static void js_std_add_helpers(JSContext *ctx, int argc, char **argv)
{
    JSValue global_obj, console, goxel_obj, proto;
    global_obj = JS_GetGlobalObject(ctx);
    console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                      JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "console", console);

    JS_NewClassID(&gox_obj_class_id);
    JS_NewClass(JS_GetRuntime(ctx), gox_obj_class_id, &gox_obj_class);
    proto = JS_NewObject(ctx);
    JS_SetClassProto(ctx, gox_obj_class_id, proto);

    goxel_obj = JS_NewObjectClass(ctx, gox_obj_class_id);
    JS_SetOpaque(goxel_obj, &goxel);
    JS_SetPropertyStr(ctx, global_obj, "goxel", goxel_obj);

    JSValue getter;
    JSValue name = JS_NewString(ctx, "image");
    getter = JS_NewCFunctionData(ctx, gox_obj_attr_getter, 0, 0, 1, &name);
    JS_DefinePropertyGetSet(ctx, goxel_obj, JS_NewAtom(ctx, "image"),
                            getter, JS_UNDEFINED, 0);

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
