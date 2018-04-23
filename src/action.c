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
#include <unistd.h>

typedef struct {
    UT_hash_handle  hh;
    action_t        action;
} action_hash_item_t;

// Global hash of all the actions.
static action_hash_item_t *g_actions = NULL;

// Incremented each time an action is called, and decremented each time one
// finish so that we know when action are being called within other actions.
static int g_reentry = 0;

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

action_t *action_get(const char *id)
{
    action_hash_item_t *item;
    HASH_FIND_STR(g_actions, id, item);
    if (!item) LOG_W("Cannot find action %s", id);
    return item ? &item->action : NULL;
}

void actions_iter(int (*f)(action_t *action, void *user), void *user)
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
    } else if (strcmp(a->csig, "vpi") == 0) {
        func(stack_get_p(s, 0),
             stack_get_i(s, 1));
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


static void *thread_func(void *args)
{
    astack_t *s = args;
    int (*func)(const action_t *a, astack_t *s);
    action_t *action;

    pthread_mutex_lock(&goxel->mutex);

    action = stack_get_p(s, -1);
    stack_pop(s);

    func = action->func ?: default_function;
    func(action, s);
    goxel_set_progress_popup(NULL, 0);

    g_reentry--;
    if (g_reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        goxel_update_meshes(-1);
    }
    stack_delete(s);

    pthread_mutex_unlock(&goxel->mutex);
    return NULL;
}

int action_execv(const action_t *action, const char *sig, va_list ap)
{
    assert(action);
    // So that we do not add undo snapshot when an action calls an other one.
    char c;
    bool b;
    int i, nb;
    int (*func)(const action_t *a, astack_t *s);
    pthread_t thread;
    astack_t *s;

    func = action->func ?: default_function;
    s = stack_create();

    if (g_reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        image_history_push(goxel->image);
    }

    g_reentry++;

    while ((c = *sig++)) {
        if (c == '>') break;
        switch (c) {
            case 'i': stack_push_i(s, va_arg(ap, int)); break;
            case 'b': stack_push_b(s, va_arg(ap, int)); break;
            case 'p': stack_push_p(s, va_arg(ap, void*)); break;
            default: assert(false);
        }
    }

    if (g_reentry == 1 && (action->flags & ACTION_THREADED)) {
        assert(action->csig[0] == 'v');
        stack_push_p(s, (void*)action);
        pthread_create(&thread, NULL, thread_func, s);
        return 0;
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

    g_reentry--;
    if (g_reentry == 0 && (action->flags & ACTION_TOUCH_IMAGE)) {
        goxel_update_meshes(-1);
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

/*
 * Function: action_progress
 * To be used by threaded tasks to update the progress dialog.
 */
int action_progress(const char *title, int total, int current)
{
    static double last_time = 0;
    double time = sys_get_time();
    goxel_set_progress_popup(title, (float)current / total);
    // Unlock and relock the global mutex to let the main loop iterate.
    pthread_mutex_unlock(&goxel->mutex);
    // If we have been running too long, sleep a little bit to let the main
    // thread some time to wake up.
    if (time - last_time < 0.016) usleep(10000);
    last_time = time;
    pthread_mutex_lock(&goxel->mutex);
    return 0;
}
