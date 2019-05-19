/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "../ext_src/lua/lauxlib.h"

typedef struct {
    UT_hash_handle  hh;
    action_t        action;
} action_hash_item_t;

// Global hash of all the actions.
static action_hash_item_t *g_actions = NULL;

void action_register(const action_t *action)
{
    action_hash_item_t *item;
    item = calloc(1, sizeof(*item));
    item->action = *action;
    assert(!*item->action.shortcut);
    if (item->action.default_shortcut) {
        assert(strlen(action->default_shortcut) < sizeof(action->shortcut));
        strcpy(item->action.shortcut, item->action.default_shortcut);
    }
    HASH_ADD_KEYPTR(hh, g_actions, action->id, strlen(action->id), item);
}

action_t *action_get(const char *id, bool assert_exists)
{
    action_hash_item_t *item;
    HASH_FIND_STR(g_actions, id, item);
    if (!item && assert_exists) {
        LOG_E("Cannot find action %s", id);
        assert(false);
    }
    return item ? &item->action : NULL;
}

void actions_iter(int (*f)(action_t *action, void *user), void *user)
{
    action_hash_item_t *item, *tmp;
    HASH_ITER(hh, g_actions, item, tmp) {
        if (f(&item->action, user)) return;
    }
}

static const void *topointer(lua_State *l, int idx)
{
    if (lua_isstring(l, idx))
        return lua_tostring(l, idx);
    if (lua_islightuserdata(l, idx))
        return lua_topointer(l, idx);
    return *(void**)lua_touserdata(l, idx);
}

// The default action function will try to call the action cfunc.
static int default_function(const action_t *a, lua_State *l)
{
    int i;
    void *p;
    assert(a->cfunc);
    assert(a->csig);

    // Fill any missing arguments with 0 values.
    for (i = lua_gettop(l); i < strlen(a->csig + 1); i++) {
        switch (a->csig[i + 1]) {
            case 'i': lua_pushnumber(l, 0); break;
            case 'p': lua_pushlightuserdata(l, NULL); break;
            default: assert(false);
        }
    }

    // XXX: clean this up.
    if (strcmp(a->csig, "v") == 0) {
        void (*func)(void) = a->cfunc;
        func();
    } else if (strcmp(a->csig, "vp") == 0) {
        void (*func)(const void *) = a->cfunc;
        func(topointer(l, 1));
    } else if (strcmp(a->csig, "vpp") == 0) {
        void (*func)(const void *, const void *) = a->cfunc;
        func(topointer(l, 1),
             topointer(l, 2));
    } else if (strcmp(a->csig, "vpi") == 0) {
        void (*func)(const void *, int) = a->cfunc;
        func(topointer(l, 1),
             lua_tointeger(l, 2));
    } else if (strcmp(a->csig, "vppp") == 0) {
        void (*func)(const void *, const void *, const void *) = a->cfunc;
        func(topointer(l, 1),
             topointer(l, 2),
             topointer(l, 3));
    } else if (strcmp(a->csig, "vppi") == 0) {
        void (*func)(const void *, const void *, int) = a->cfunc;
        func(topointer(l, 1),
             topointer(l, 2),
             lua_tointeger(l, 3));
    } else if (strcmp(a->csig, "vpii") == 0) {
        void (*func)(const void *, int, int) = a->cfunc;
        func(topointer(l, 1),
             lua_tointeger(l, 2),
             lua_tointeger(l, 3));
    } else if (strcmp(a->csig, "p") == 0) {
        void *(*func)(void) = a->cfunc;
        p = func();
        lua_pushlightuserdata(l, p);
    } else if (strcmp(a->csig, "pp") == 0) {
        void *(*func)(const void *) = a->cfunc;
        p = func(topointer(l, 1));
        lua_pushlightuserdata(l, p);
    } else {
        LOG_E("Cannot handle sig '%s'", a->csig);
        assert(false);
    }

    return a->csig[0] == 'v' ? 0 : 1;
}

int action_execv(const action_t *action, const char *sig, va_list ap)
{
    assert(action);
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;
    char c;
    bool b;
    int i, nb;
    int (*func)(const action_t *a, lua_State *l);
    lua_State *l = luaL_newstate();
    func = action->func ?: default_function;

    if (reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        image_history_push(goxel.image);
    }

    reentry++;

    while ((c = *sig++)) {
        if (c == '>') break;
        switch (c) {
            case 'i': lua_pushnumber(l, va_arg(ap, int)); break;
            case 'b': lua_pushboolean(l, va_arg(ap, int)); break;
            case 'p': lua_pushlightuserdata(l, va_arg(ap, void*)); break;
            default: assert(false);
        }
    }

    if ((action->flags & ACTION_TOGGLE) && (lua_gettop(l) == 0) && (!c)) {
        func(action, l);
        b = lua_toboolean(l, 1);
        lua_settop(l, 0);
        lua_pushboolean(l, !b);
    }
    func(action, l);

    // Get the return arguments.
    nb = c ? (int)strlen(sig) : 0;
    for (i = 0; i < nb; i++) {
        c = sig[i];
        switch (c) {
            case 'i':
                *va_arg(ap, int*) = lua_tointeger(l, -nb + i + 1);
                break;
            case 'b':
                *va_arg(ap, bool*) = lua_toboolean(l, -nb + i + 1);
                break;
            case 'p':
                *va_arg(ap, void**) = lua_touserdata(l, -nb + i + 1);
                break;
            default: assert(false);
        }
    }

    reentry--;
    lua_close(l);
    return 0;
}

int action_exec(const action_t *action, const char *sig, ...)
{
    va_list ap;
    int ret;
    va_start(ap, sig);
    ret = action_execv(action, sig, ap);
    va_end(ap);
    return ret;
}

int action_exec_lua(const action_t *action, lua_State *l)
{
    int (*func)(const action_t *a, lua_State *l);
    func = action->func ?: default_function;
    return func(action, l);
}
