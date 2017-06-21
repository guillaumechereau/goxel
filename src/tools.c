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

static const int STATE_MASK = 0x00ff;

enum {
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,
    // Added to a state the first time we enter into it.
    STATE_ENTER     = 0x0100,
};

static const tool_t *g_tools[TOOL_COUNT] = {};

static int tool_set_action(const action_t *a, astack_t *s)
{
    tool_cancel(goxel->tool, goxel->tool_state, &goxel->tool_data);
    goxel->tool = ((tool_t*)a->data)->id;
    return 0;
}

void tool_register_(const tool_t *tool)
{
    action_t action;
    g_tools[tool->id] = tool;
    action = (action_t) {
        .id = tool->action_id,
        .shortcut = tool->shortcut,
        .help = "set tool",
        .func = tool_set_action,
        .data = (void*)tool,
    };
    action_register(&action);
}

int tool_iter(int tool, int state, void **data,
              const vec4_t *view, bool inside)
{
    int ret;
    assert(tool >= 0 && tool < TOOL_COUNT);
    assert(g_tools[tool]->iter_fn);

    while (true) {
        ret = g_tools[tool]->iter_fn(state, data, view);
        if (ret == STATE_CANCEL) {
            tool_cancel(tool, state, data);
            ret = 0;
        }
        if (ret == STATE_END) ret = 0;
        if ((ret & STATE_MASK) != (state & STATE_MASK))
            ret |= STATE_ENTER;
        else
            ret &= ~STATE_ENTER;

        if (ret == state) break;
        state = ret;
    }

    return ret;
}

void tool_cancel(int tool, int state, void **data)
{
    if (g_tools[tool]->cancel_fn)
        g_tools[tool]->cancel_fn(state, data);
    assert(*data == NULL);
    goxel->tool_state = 0;
    goxel_update_meshes(goxel, MESH_LAYERS);
}

int tool_gui(int tool)
{
    if (!g_tools[tool]->gui_fn) return 0;
    return g_tools[tool]->gui_fn();
}


static void snap_button(const char *label, int s, float w)
{
    bool v = goxel->snap_mask & s;
    if (gui_selectable(label, &v, NULL, w))
        set_flag(&goxel->snap_mask, s, v);
}

int tool_gui_snap(void)
{
    float w, v;
    gui_text("Snap on");
    w = gui_get_avail_width() / 2.0 - 1;
    gui_group_begin(NULL);
    snap_button("Mesh", SNAP_MESH, w);
    gui_same_line();
    snap_button("Plane", SNAP_PLANE, w);
    if (!box_is_null(goxel->selection)) {
        snap_button("Sel In", SNAP_SELECTION_IN, w);
        gui_same_line();
        snap_button("Sel out", SNAP_SELECTION_OUT, w);
    }
    if (!box_is_null(goxel->image->box))
        snap_button("Image box", SNAP_IMAGE_BOX, -1);

    v = goxel->snap_offset;
    if (gui_input_float("Offset", &v, 0.1, -1, +1, "%.1f"))
        goxel->snap_offset = clamp(v, -1, +1);
    gui_group_end();
    return 0;
}

// XXX: replace this.
static void auto_grid(int nb, int i, int col)
{
    if ((i + 1) % col != 0) gui_same_line();
}

int tool_gui_mode(void)
{
    int i;
    bool v;
    struct {
        int        mode;
        const char *name;
        int        icon;
    } values[] = {
        {MODE_OVER,   "Add",  ICON_MODE_ADD},
        {MODE_SUB,    "Sub",  ICON_MODE_SUB},
        {MODE_PAINT,  "Paint", ICON_MODE_PAINT},
    };
    gui_text("Mode");

    gui_group_begin(NULL);
    for (i = 0; i < (int)ARRAY_SIZE(values); i++) {
        v = goxel->painter.mode == values[i].mode;
        if (gui_selectable_icon(values[i].name, &v, values[i].icon)) {
            goxel->painter.mode = values[i].mode;
        }
        auto_grid(ARRAY_SIZE(values), i, 4);
    }
    gui_group_end();
    return 0;
}

int tool_gui_shape(void)
{
    struct {
        const char  *name;
        shape_t     *shape;
        int         icon;
    } shapes[] = {
        {"Sphere", &shape_sphere, ICON_SHAPE_SPHERE},
        {"Cube", &shape_cube, ICON_SHAPE_CUBE},
        {"Cylinder", &shape_cylinder, ICON_SHAPE_CYLINDER},
    };
    int i;
    bool v;
    gui_text("Shape");
    gui_group_begin(NULL);
    for (i = 0; i < (int)ARRAY_SIZE(shapes); i++) {
        v = goxel->painter.shape == shapes[i].shape;
        if (gui_selectable_icon(shapes[i].name, &v, shapes[i].icon)) {
            goxel->painter.shape = shapes[i].shape;
        }
        auto_grid(ARRAY_SIZE(shapes), i, 4);
    }
    gui_group_end();
    return 0;
}

int tool_gui_radius(void)
{
    int i;
    i = goxel->tool_radius * 2;
    if (gui_input_int("Size", &i, 1, 128)) {
        i = clamp(i, 1, 128);
        goxel->tool_radius = i / 2.0;
    }
    return 0;
}

int tool_gui_smoothness(void)
{
    bool s;
    s = goxel->painter.smoothness;
    if (gui_checkbox("Antialiased", &s, NULL)) {
        goxel->painter.smoothness = s ? 1 : 0;
    }
    return 0;
}

int tool_gui_color(void)
{
    gui_text("Color");
    gui_color(&goxel->painter.color);
    return 0;
}
