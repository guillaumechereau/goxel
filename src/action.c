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

int action_exec(const action_t *action)
{
    assert(action);
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;

    if (reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        image_history_push(goxel.image);
    }

    reentry++;

    if (action->data)
        action->cfunc_data(action->data);
    else
        action->cfunc();

    reentry--;
    return 0;
}
