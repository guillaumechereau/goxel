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

static bool sig_equ(action_sig_t a, action_sig_t b)
{
    int i;
    if (a.ret != b.ret || a.nb_args != b.nb_args) return false;
    for (i = 0; i < a.nb_args; i++)
        if (a.args[i].type != b.args[i].type) return false;
    return true;
}

#define SIG_EQU(a, ...) ({  \
        action_sig_t b = SIG(__VA_ARGS__); \
        sig_equ(a, b); \
    })

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
    return 0;
}

void *action_exec(const action_t *action, const arg_t *args)
{
    void *ret = NULL;
    long vals[3];
    // So that we do not add undo snapshot when an action calls an other one.
    static int reentry = 0;

    assert(action);
    reentry++;
    // For the moment we handle all the different signatures manually.
    // Is there a way to make it automatic?
    if (SIG_EQU(action->sig, TYPE_VOID, ARG(NULL, TYPE_IMAGE))) {
        void (*func)(image_t *) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        func((image_t*)vals[0]);
    } else if (SIG_EQU(action->sig, TYPE_LAYER, ARG(NULL, TYPE_IMAGE))) {
        layer_t *(*func)(image_t *) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        ret = func((image_t*)vals[0]);
    } else if (SIG_EQU(action->sig, TYPE_VOID, ARG(NULL, TYPE_IMAGE),
                                               ARG(NULL, TYPE_LAYER))) {
        void (*func)(image_t *, layer_t *) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        vals[1] = get_arg_value(action->sig.args[1], args);
        func((image_t*)vals[0], (layer_t*)vals[1]);
    } else if (SIG_EQU(action->sig, TYPE_VOID, ARG(NULL, TYPE_IMAGE),
                                               ARG(NULL, TYPE_LAYER),
                                               ARG(NULL, TYPE_INT))) {
        void (*func)(image_t *, layer_t *, int) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        vals[1] = get_arg_value(action->sig.args[1], args);
        vals[2] = get_arg_value(action->sig.args[2], args);
        func((image_t*)vals[0], (layer_t*)vals[1], vals[2]);
    } else if (SIG_EQU(action->sig, TYPE_VOID, ARG(NULL, TYPE_GOXEL),
                                               ARG(NULL, TYPE_STRING),
                                               ARG(NULL, TYPE_FILE_PATH))) {
        void (*func)(goxel_t *, const char *, const char *) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        vals[1] = get_arg_value(action->sig.args[1], args);
        vals[2] = get_arg_value(action->sig.args[2], args);
        func((goxel_t*)vals[0], (const char *)vals[1], (const char *)vals[2]);
    } else if (SIG_EQU(action->sig, TYPE_VOID, ARG(NULL, TYPE_GOXEL),
                                               ARG(NULL, TYPE_FILE_PATH))) {
        void (*func)(goxel_t *, const char *) = action->func;
        vals[0] = get_arg_value(action->sig.args[0], args);
        vals[1] = get_arg_value(action->sig.args[1], args);
        func((goxel_t*)vals[0], (const char *)vals[1]);
    } else {
        LOG_W("Cannot handle signature for action %s", action->id);
    }

    reentry--;
    if (reentry == 0 && !(action->flags & ACTION_NO_CHANGE)) {
        image_history_push(goxel()->image);
        goxel_update_meshes(goxel(), true);
    }
    return ret;
}
