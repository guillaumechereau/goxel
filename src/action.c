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

// Global array of actions.
static action_t *g_actions = NULL;

void action_register(const action_t *action, int idx)
{
    action_t *a;
    assert(idx > 0 && idx < ACTION_COUNT);

    if (!g_actions)
        g_actions = calloc(ACTION_COUNT, sizeof(*g_actions));

    a = &g_actions[idx];
    *a = *action;
    a->idx = idx;
    assert(!a->shortcut[0]);
    if (a->default_shortcut) {
        assert(strlen(a->default_shortcut) < sizeof(a->shortcut));
        strcpy(a->shortcut, a->default_shortcut);
    }
}

action_t *action_get(int idx, bool assert_exists)
{
    action_t *action;
    assert(g_actions);
    assert(idx > 0 && idx < ACTION_COUNT);
    action = &g_actions[idx];
    if (!action->idx && assert_exists) {
        LOG_E("Cannot find action %d", idx);
        assert(false);
    }
    return action;
}

action_t *action_get_by_name(const char *name)
{
    int i;
    action_t *action;
    assert(g_actions);
    for (i = 0; i < ACTION_COUNT; i++) {
        action = &g_actions[i];
        if (!action->idx) continue;
        if (strcmp(name, action->id) == 0)
            return action;
    }
    return NULL;
}

void actions_iter(int (*f)(action_t *action, void *user), void *user)
{
    action_t *action;
    int i;
    for (i = 0; i < ACTION_COUNT; i++) {
        action = &g_actions[i];
        if (!action->idx) continue;
        if (f(action, user)) return;
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
