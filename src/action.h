#ifndef ACTION_H
#define ACTION_H

#include <stdarg.h>
#include <stdbool.h>

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

typedef struct astack astack_t;

astack_t *stack_create(void);
void      stack_delete(astack_t *s);

void  stack_clear(astack_t *s);
int   stack_size(const astack_t *s);
char  stack_type(const astack_t *s, int i);
void  stack_push_i(astack_t *s, int i);
void  stack_push_p(astack_t *s, void *p);
void  stack_push_b(astack_t *s, bool b);
int   stack_get_i(const astack_t *s, int i);
void *stack_get_p(const astack_t *s, int i);
bool  stack_get_b(const astack_t *s, int i);
void  stack_pop(astack_t *s);

enum {
    ACTION_TOUCH_IMAGE          = 1 << 0,  // Push the undo history.
    // Toggle actions accept and return a boolean value.
    ACTION_TOGGLE               = 1 << 1,
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
    int             (*func)(const action_t *a, astack_t *s);
    void            *data;

    // cfunc and csig can be used to directly call any function.
    void            *cfunc;
    const char      *csig;

    // Used for export / import actions.
    struct {
        const char  *name;
        const char  *ext;
    } file_format;
};

void action_register(const action_t *action);
action_t *action_get(const char *id);
int action_exec(const action_t *action, const char *sig, ...);
int action_execv(const action_t *action, const char *sig, va_list ap);
void actions_iter(int (*f)(action_t *action, void *user), void *user);

// Convenience macro to call action_exec directly from an action id.
#define action_exec2(id, sig, ...) \
    action_exec(action_get(id), sig, ##__VA_ARGS__)


// Convenience macro to register an action from anywere in a c file.
#define ACTION_REGISTER(id_, ...) \
    static const action_t GOX_action_##id_ = {.id = #id_, __VA_ARGS__}; \
    static void GOX_register_action_##id_(void) __attribute__((constructor)); \
    static void GOX_register_action_##id_(void) { \
        action_register(&GOX_action_##id_); \
    }

#endif // ACTION_H
