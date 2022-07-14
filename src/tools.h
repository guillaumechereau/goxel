#ifndef TOOLS_H
#define TOOLS_H

#include "shape.h"
#include "mesh_utils.h"

enum {
    TOOL_NONE = 0,
    TOOL_BRUSH,
    TOOL_SHAPE,
    TOOL_LINE,
    TOOL_LASER,
    TOOL_SET_PLANE,
    TOOL_MOVE,
    TOOL_PICK_COLOR,
    TOOL_SELECTION,
    TOOL_PROCEDURAL,
    TOOL_EXTRUDE,
    TOOL_FUZZY_SELECT,

    TOOL_COUNT
};

enum {
    // Tools flags.
    TOOL_REQUIRE_CAN_EDIT = 1 << 0, // Set to tools that can edit the layer.
    TOOL_REQUIRE_CAN_MOVE = 1 << 1, // Set to tools that can move the layer.
    TOOL_ALLOW_PICK_COLOR = 1 << 2, // Ctrl switches to pick color tool.
};

// Tools
typedef struct tool tool_t;
struct tool {
    int id;
    const char *action_id;
    int action_idx;
    int (*iter_fn)(tool_t *tool, const painter_t *painter,
                   const float viewport[4]);
    int (*gui_fn)(tool_t *tool);
    const char *default_shortcut;
    int state; // XXX: to be removed I guess.
    int flags;
    const char *name;
};

#define TOOL_REGISTER(id_, name_, klass_, ...) \
    static klass_ GOX_tool_##id_ = {\
            .tool = { \
                .action_idx = ACTION_tool_set_##name_, \
                .id = id_, .action_id = "tool_set_" #name_, __VA_ARGS__ \
            } \
        }; \
    static void GOX_register_tool_##tool_(void) __attribute__((constructor)); \
    static void GOX_register_tool_##tool_(void) { \
        tool_register_(&GOX_tool_##id_.tool); \
    }

void tool_register_(tool_t *tool);
const tool_t *tool_get(int id);

int tool_iter(tool_t *tool, const painter_t *painter, const float viewport[4]);
int tool_gui(tool_t *tool);

int tool_gui_snap(void);
int tool_gui_shape(const shape_t **shape);
int tool_gui_radius(void);
int tool_gui_smoothness(void);
int tool_gui_color(void);
int tool_gui_symmetry(void);
int tool_gui_drag_mode(int *mode);

#endif // TOOLS_H
