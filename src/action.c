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

static long get_arg_value(arg_t arg, const arg_t *args)
{
    const arg_t *argp;
    if (args) {
        for (argp = args; argp->name; argp++) {
            if (strcmp(argp->name, arg.name) == 0)
                return argp->value;
        }
    }
    // Default values for some types.
    if (arg.type == TYPE_GOXEL)
        return (long)goxel();
    if (arg.type == TYPE_IMAGE)
        return (long)goxel()->image;
    if (arg.type == TYPE_LAYER)
        return (long)goxel()->image->active_layer;
    if (arg.type == TYPE_BOX)
        return (long)&goxel()->selection;
    if (str_equ(arg.name, "width"))
        return (int)goxel()->image->export_width;
    if (str_equ(arg.name, "height"))
        return (int)goxel()->image->export_height;
    return 0;
}

void *action_exec(const action_t *action, const arg_t *args)
{
    void *ret = NULL;
    int i, nb_args;
    long vals[4];
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;
    // XXX: not sure this is actually legal in C.  func will be called with
    // a variable number or arguments, that might be of any registered types!
    void *(*func)() = action->func;
    bool is_void;

    assert(action);
    nb_args = action->sig.nb_args;
    assert(nb_args <= ARRAY_SIZE(vals));
    is_void = action->sig.ret == TYPE_VOID;

    // For the moment all action cancel the current tool, for simplicity.
    tool_cancel(goxel(), goxel()->tool, goxel()->tool_state);

    if (reentry == 0 && !(action->flags & ACTION_NO_CHANGE)) {
        image_history_push(goxel()->image);
    }

    reentry++;

    for (i = 0; i < nb_args; i++)
        vals[i] = get_arg_value(action->sig.args[i], args);

    if (is_void && nb_args == 0)
        func();
    else if (is_void && nb_args == 1)
        func(vals[0]);
    else if (is_void && nb_args == 2)
        func(vals[0], vals[1]);
    else if (is_void && nb_args == 3)
        func(vals[0], vals[1], vals[2]);
    else if (is_void && nb_args == 4)
        func(vals[0], vals[1], vals[2], vals[3]);
    else if (!is_void && nb_args == 0)
        ret = func();
    else if (!is_void && nb_args == 1)
        ret = func(vals[0]);
    else if (!is_void && nb_args == 2)
        ret = func(vals[0], vals[1]);
    else if (!is_void && nb_args == 3)
        ret = func(vals[0], vals[1], vals[2]);
    else if (!is_void && nb_args == 4)
        ret = func(vals[0], vals[1], vals[2], vals[3]);
    else
        LOG_E("Cannot handle signature for action %s", action->id);

    reentry--;
    if (reentry == 0 && !(action->flags & ACTION_NO_CHANGE)) {
        goxel_update_meshes(goxel(), true);
    }
    return ret;
}
