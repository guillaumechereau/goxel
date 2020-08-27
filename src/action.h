/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef ACTION_H
#define ACTION_H

#include <stdarg.h>
#include <stdbool.h>

#include "luagoxel.h"

// #### Action #################

// We support some basic reflexion of functions.  We do this by registering the
// functions with the ACTION_REGISTER macro.  Once a function has been
// registered, it is possible to query it (action_get) and call it
// (action_exec).
// Check the end of image.c to see some examples.  The idea is that this will
// make it much easier to add meta information to functions, like
// documentation, shortcuts.  Also in theory this should allow to add a
// scripting engine on top of goxel quite easily.

// XXX: this is still pretty experimental.  This might change in the future.

typedef struct image image_t;


enum {
    ACTION_TOUCH_IMAGE          = 1 << 0,  // Push the undo history.
    ACTION_CAN_EDIT_SHORTCUT    = 1 << 2,
};

// Represent an action.
typedef struct action action_t;
struct action {
    const char      *id;            // Globally unique id.
    const char      *help;          // Help text.
    int             flags;
    const char      *default_shortcut;
    char            shortcut[8];    // Can be changed at runtime.
    int             icon;           // Optional icon id.
    int             (*func)(const action_t *a, lua_State *l);
    void            *data;

    // cfunc and csig can be used to directly call any function.
    union {
        void            (*cfunc)(void);
        void            (*cfunc_data)(void *data);
    };

    // Used for export / import actions.
    struct {
        const char  *name;
        const char  *ext;
        void        (*export_gui)(void);
        int         (*export_func)(const image_t *img, const char *path);
        int         (*import_func)(image_t *img, const char *path);
    } file_format;
};

void action_register(const action_t *action);
action_t *action_get(const char *id, bool assert_exists);
int action_exec(const action_t *action, const char *sig, ...);
int action_execv(const action_t *action, const char *sig, va_list ap);
void actions_iter(int (*f)(action_t *action, void *user), void *user);

// Convenience macro to call action_exec directly from an action id.
#define action_exec2(id, sig, ...) \
    action_exec(action_get(id, true), sig, ##__VA_ARGS__)


// Convenience macro to register an action from anywere in a c file.
#define ACTION_REGISTER(id_, ...) \
    static const action_t GOX_action_##id_ = {.id = #id_, __VA_ARGS__}; \
    static void GOX_register_action_##id_(void) __attribute__((constructor)); \
    static void GOX_register_action_##id_(void) { \
        action_register(&GOX_action_##id_); \
    }

#endif // ACTION_H
