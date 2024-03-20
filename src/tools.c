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

static const tool_t *g_tools[TOOL_COUNT] = {};

static void a_tool_set(void *data)
{
    if (goxel.tool_volume) {
        volume_delete(goxel.tool_volume);
        goxel.tool_volume = NULL;
    }
    goxel.tool = (tool_t*)data;
}

void tool_register_(tool_t *tool)
{
    action_t action;
    action = (action_t) {
        .id = tool->action_id,
        .default_shortcut = tool->default_shortcut,
        .cfunc_data = a_tool_set,
        .data = (void*)tool,
        .flags = ACTION_CAN_EDIT_SHORTCUT,
    };
    action_register(&action, tool->action_idx);
    g_tools[tool->id] = tool;
}

const tool_t *tool_get(int id)
{
    return g_tools[id];
}

static int pick_color_gesture(gesture3d_t *gest, void *user)
{
    cursor_t *curs = &goxel.cursor;
    const volume_t *volume = goxel_get_layers_volume(goxel.image);
    int pi[3] = {floor(curs->pos[0]),
                 floor(curs->pos[1]),
                 floor(curs->pos[2])};
    uint8_t color[4];
    curs->snap_mask = SNAP_VOLUME;
    curs->snap_offset = -0.5;

    goxel_set_help_text("Click on a voxel to pick the color");
    if (!curs->snaped) return 0;
    volume_get_at(volume, NULL, pi, color);
    color[3] = 255;
    goxel_set_help_text("pick: %d %d %d", color[0], color[1], color[2]);
    if (curs->flags & CURSOR_PRESSED) vec4_copy(color, goxel.painter.color);
    return 0;
}

static gesture3d_t g_pick_color_gesture = {
    .type = GESTURE_HOVER,
    .callback = pick_color_gesture,
    .buttons = CURSOR_CTRL,
};

int tool_iter(tool_t *tool, const painter_t *painter, const float viewport[4])
{
    assert(tool);
    if (    (tool->flags & TOOL_REQUIRE_CAN_EDIT) &&
            !image_layer_can_edit(goxel.image, goxel.image->active_layer)) {
        goxel_set_help_text("Cannot edit this layer");
        return 0;
    }
    tool->state = tool->iter_fn(tool, painter, viewport);

    if (tool->flags & TOOL_ALLOW_PICK_COLOR)
        gesture3d(&g_pick_color_gesture, &goxel.cursor, NULL);

    return tool->state;
}

int tool_gui(tool_t *tool)
{
    if (!tool->gui_fn) return 0;
    return tool->gui_fn(tool);
}

static bool mask_mode_button(const char *label, int s)
{
    bool v = goxel.mask_mode == s;
    if (gui_selectable(label, &v, NULL, 0)) {
        goxel.mask_mode = s;
        return true;
    }
    return false;
}

int tool_gui_mask_mode(void)
{
    gui_text(_(MASK));
    gui_group_begin(NULL);
    gui_row_begin(3);
    mask_mode_button(_(SET), MODE_REPLACE);
    mask_mode_button(_(ADD), MODE_OVER);
    mask_mode_button(_(SUB), MODE_SUB);
    gui_row_end();
    gui_group_end();
    return 0;
}

int tool_gui_shape(const shape_t **shape)
{
    const struct {
        int         name;
        shape_t     *shape;
        int         icon;
    } shapes[] = {
        {STR_SPHERE, &shape_sphere, ICON_SHAPE_SPHERE},
        {STR_CUBE, &shape_cube, ICON_SHAPE_CUBE},
        {STR_CYLINDER, &shape_cylinder, ICON_SHAPE_CYLINDER},
    };
    gui_icon_info_t grid[64] = {};
    shape = shape ?: &goxel.painter.shape;
    int i, ret = 0;
    int current;
    const int nb = ARRAY_SIZE(shapes);

    if (gui_section_begin(_(SHAPE), true)) {
        for (i = 0; i < nb; i++) {
            grid[i] = (gui_icon_info_t) {
                .label = tr(shapes[i].name),
                .icon = shapes[i].icon,
            };
            if (*shape == shapes[i].shape) current = i;
        }
        if (gui_icons_grid(nb, grid, &current)) {
            *shape = shapes[current].shape;
            ret = 1;
        }
    }
    gui_section_end();
    return ret;
}

int tool_gui_radius(void)
{
    int i;
    i = goxel.tool_radius * 2;
    if (gui_input_int(_(SIZE), &i, 0, 0)) {
        i = clamp(i, 1, 128);
        goxel.tool_radius = i / 2.0;
    }
    return 0;
}

int tool_gui_smoothness(void)
{
    bool s;
    s = goxel.painter.smoothness;
    if (gui_checkbox(_(ANTIALIASING), &s, NULL)) {
        goxel.painter.smoothness = s ? 1 : 0;
    }
    return 0;
}

int tool_gui_color(void)
{
    float alpha;
    gui_color_small(_(COLOR), goxel.painter.color);
    if (goxel.painter.mode == MODE_PAINT) {
        alpha = goxel.painter.color[3] / 255.;
        if (gui_input_float(_(ALPHA), &alpha, 0.1, 0, 1, "%.1f"))
            goxel.painter.color[3] = alpha * 255;
    }
    return 0;
}

int tool_gui_drag_mode(int *mode)
{
    int ret = 0;
    bool b;

    gui_group_begin(_(ADJUSTEMENTS));
    gui_row_begin(2);
    b = *mode == 0;
    if (gui_selectable(_(MOVE), &b, NULL, 0)) {
        *mode = 0;
        ret = 1;
    }
    b = *mode == 1;
    if (gui_selectable(_(RESIZE), &b, NULL, 0)) {
        *mode = 1;
        ret = 1;
    }
    gui_row_end();
    gui_group_end();
    return ret;
}
