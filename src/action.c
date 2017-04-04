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
    HASH_ADD_KEYPTR(hh, g_actions, action->id, strlen(action->id), item);
}

const action_t *action_get(const char *id)
{
    action_hash_item_t *item;
    HASH_FIND_STR(g_actions, id, item);
    if (!item) LOG_W("Cannot find action %s", id);
    return item ? &item->action : NULL;
}

void actions_iter(int (*f)(const action_t *action, void *user), void *user)
{
    action_hash_item_t *item, *tmp;
    HASH_ITER(hh, g_actions, item, tmp) {
        if (f(&item->action, user)) return;
    }
}

// The default action function will try to call the action cfunc.
static int default_function(const action_t *a, astack_t *s)
{
    int i;
    void (*func)() = a->cfunc;
    assert(a->cfunc);
    assert(a->csig);

    // Fill any missing arguments with 0 values.
    for (i = stack_size(s); i < strlen(a->csig + 1); i++) {
        switch (a->csig[i + 1]) {
            case 'i': stack_push_i(s, 0); break;
            case 'p': stack_push_p(s, NULL); break;
            default: assert(false);
        }
    }

    if (strcmp(a->csig, "v") == 0) {
        func();
    } else if (strcmp(a->csig, "vp") == 0) {
        func(stack_get_p(s, 0));
    } else if (strcmp(a->csig, "vpp") == 0) {
        func(stack_get_p(s, 0),
             stack_get_p(s, 1));
    } else if (strcmp(a->csig, "vppp") == 0) {
        func(stack_get_p(s, 0),
             stack_get_p(s, 1),
             stack_get_p(s, 2));
    } else if (strcmp(a->csig, "vppi") == 0) {
        func(stack_get_p(s, 0),
             stack_get_p(s, 1),
             stack_get_i(s, 2));
    } else if (strcmp(a->csig, "vpii") == 0) {
        func(stack_get_p(s, 0),
             stack_get_i(s, 1),
             stack_get_i(s, 2));
    } else {
        LOG_E("Cannot handle sig '%s'", a->csig);
        assert(false);
    }

    return 0;
}

int action_execv(const action_t *action, const char *sig, va_list ap)
{
    assert(action);
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;
    char c;
    bool b;
    int i, nb;
    int (*func)(const action_t *a, astack_t *s);
    astack_t *s = stack_create();
    func = action->func ?: default_function;

    if (reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        tool_cancel(goxel->tool, goxel->tool_state, &goxel->tool_data);
        image_history_push(goxel->image);
    }

    reentry++;

    while ((c = *sig++)) {
        if (c == '>') break;
        switch (c) {
            case 'i': stack_push_i(s, va_arg(ap, int)); break;
            case 'b': stack_push_b(s, va_arg(ap, int)); break;
            case 'p': stack_push_p(s, va_arg(ap, void*)); break;
            default: assert(false);
        }
    }

    if ((action->flags & ACTION_TOGGLE) && (stack_size(s) == 0) && (!c)) {
        func(action, s);
        b = stack_get_b(s, 0);
        stack_clear(s);
        stack_push_b(s, !b);
    }
    func(action, s);

    // Get the return arguments.
    nb = c ? (int)strlen(sig) : 0;
    for (i = 0; i < nb; i++) {
        c = sig[i];
        switch (c) {
            case 'i': *va_arg(ap, int*) = stack_get_i(s, -nb + i); break;
            case 'b': *va_arg(ap, bool*) = stack_get_b(s, -nb + i); break;
            case 'p': *va_arg(ap, void**) = stack_get_p(s, -nb + i); break;
            default: assert(false);
        }
    }

    reentry--;
    if (reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        goxel_update_meshes(goxel, -1);
    }
    stack_delete(s);
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
