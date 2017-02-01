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
    const action_t  *action;
} action_hash_item_t;

// Global hash of all the actions.
static action_hash_item_t *g_actions = NULL;

void action_register(const action_t *action)
{
    action_hash_item_t *item;
    item = calloc(1, sizeof(*item));
    item->action = action;
    HASH_ADD_KEYPTR(hh, g_actions, action->id, strlen(action->id), item);
}

const action_t *action_get(const char *id)
{
    action_hash_item_t *item;
    HASH_FIND_STR(g_actions, id, item);
    if (!item) LOG_W("Cannot find action %s", id);
    return item ? item->action : NULL;
}

void actions_iter(int (*f)(const action_t *action))
{
    action_hash_item_t *item, *tmp;
    HASH_ITER(hh, g_actions, item, tmp) {
        f(item->action);
    }
}

int action_execv(const action_t *action, const char *sig, va_list ap)
{
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;
    int i;
    astack_t *stack = stack_create();
    // XXX: not sure this is actually legal in C.  func will be called with
    // a variable number or arguments, that might be of any registered types!
    void (*func)() = action->func;

    assert(action);

    // For the moment all action cancel the current tool, for simplicity.
    tool_cancel(goxel, goxel->tool, goxel->tool_state);

    if (reentry == 0 && !(action->flags & ACTION_NO_CHANGE)) {
        image_history_push(goxel->image);
    }

    reentry++;

    for (i = 0; i < strlen(action->sig); i++) {
        if (i < strlen(sig)) {
            assert(sig[i] == action->sig[i]);
            switch (action->sig[i]) {
                case 'i': stack_push_i(stack, va_arg(ap, int)); break;
                case 'p': stack_push_p(stack, va_arg(ap, void*)); break;
                default: assert(false);
            }
        } else {
            switch (action->sig[i]) {
                case 'i': stack_push_i(stack, 0); break;
                case 'p': stack_push_p(stack, NULL); break;
                default: assert(false);
            }
        }
    }
    if (strcmp(action->sig, "") == 0) {
        func();
    } else if (strcmp(action->sig, "p") == 0) {
        func(stack_get_p(stack, 0));
    } else if (strcmp(action->sig, "pp") == 0) {
        func(stack_get_p(stack, 0),
             stack_get_p(stack, 1));
    } else if (strcmp(action->sig, "ppp") == 0) {
        func(stack_get_p(stack, 0),
             stack_get_p(stack, 1),
             stack_get_p(stack, 2));
    } else if (strcmp(action->sig, "ppi") == 0) {
        func(stack_get_p(stack, 0),
             stack_get_p(stack, 1),
             stack_get_i(stack, 2));
    } else if (strcmp(action->sig, "pii") == 0) {
        func(stack_get_p(stack, 0),
             stack_get_i(stack, 1),
             stack_get_i(stack, 2));
    } else {
        LOG_E("Cannot handle sig '%s'", action->sig);
        assert(false);
    }

    reentry--;
    if (reentry == 0 && !(action->flags & ACTION_NO_CHANGE)) {
        goxel_update_meshes(goxel, -1);
    }
    stack_delete(stack);
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
