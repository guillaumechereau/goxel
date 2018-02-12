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


static int tool_set_action(const action_t *a, astack_t *s)
{
    if (goxel->tool_mesh) {
        mesh_delete(goxel->tool_mesh);
        goxel->tool_mesh = NULL;
        goxel_update_meshes(goxel, MESH_LAYERS);
    }
    goxel->tool = (tool_t*)a->data;
    return 0;
}

void tool_register_(const tool_t *tool)
{
    action_t action;
    action = (action_t) {
        .id = tool->action_id,
        .shortcut = tool->shortcut,
        .help = "set tool",
        .func = tool_set_action,
        .data = (void*)tool,
    };
    action_register(&action);
}


static int pick_color_gesture(gesture3d_t *gest, void *user)
{
    cursor_t *curs = &goxel->cursor;
    mesh_t *mesh = goxel->layers_mesh;
    int pi[3] = {floor(curs->pos[0]),
                 floor(curs->pos[1]),
                 floor(curs->pos[2])};
    uint8_t color[4];
    curs->snap_mask = SNAP_MESH;
    curs->snap_offset = -0.5;

    goxel_set_help_text(goxel, "Click on a voxel to pick the color");
    if (!curs->snaped) return 0;
    mesh_get_at(mesh, NULL, pi, color);
    color[3] = 255;
    goxel_set_help_text(goxel, "pick: %d %d %d",
                        color[0], color[1], color[2]);
    if (curs->flags & CURSOR_PRESSED) vec4_copy(color, goxel->painter.color);
    return 0;
}

static gesture3d_t g_pick_color_gesture = {
    .type = GESTURE_HOVER,
    .callback = pick_color_gesture,
    .buttons = CURSOR_CTRL,
};

int tool_iter(tool_t *tool, const float viewport[4])
{
    assert(tool);
    if (    (tool->flags & TOOL_REQUIRE_CAN_EDIT) &&
            !image_layer_can_edit(goxel->image, goxel->image->active_layer)) {
        goxel_set_help_text(goxel, "Cannot edit this layer");
        return 0;
    }
    tool->state = tool->iter_fn(tool, viewport);

    if (tool->flags & TOOL_ALLOW_PICK_COLOR)
        gesture3d(&g_pick_color_gesture, &goxel->cursor, NULL);

    return tool->state;
}

int tool_gui(tool_t *tool)
{
    if (!tool->gui_fn) return 0;
    return tool->gui_fn(tool);
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
    if (!box_is_null(goxel->image->box.mat))
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
    int alpha = goxel->painter.color[3];
    gui_text("Color");
    gui_color("##color", goxel->painter.color);
    if (goxel->painter.mode == MODE_PAINT) {
        if (gui_input_int("Alpha", &alpha, 0, 255))
            goxel->painter.color[3] = alpha;
    } else {
        goxel->painter.color[3] = 255;
    }
    return 0;
}

int tool_gui_symmetry(void)
{
    float w;
    int i;
    bool v;
    char *labels[] = {"X", "Y", "Z"};
    gui_text("Symmetry");
    w = gui_get_avail_width() / 3.0 - 1;
    gui_group_begin(NULL);
    for (i = 0; i < 3; i++) {
        v = (goxel->painter.symmetry >> i) & 0x1;
        if (gui_selectable(labels[i], &v, NULL, w))
            set_flag(&goxel->painter.symmetry, 1 << i, v);
        gui_same_line();
    }
    gui_group_end();
    return 0;
}
