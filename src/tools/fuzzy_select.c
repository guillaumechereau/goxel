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

#include "goxel.h"

typedef struct {
    tool_t tool;
    mesh_t *selection;
    int threshold;
    struct {
        gesture3d_t click;
    } gestures;
} tool_fuzzy_select_t;

static int select_cond(void *user, const mesh_t *mesh,
                       const int base_pos[3],
                       const int new_pos[3],
                       mesh_accessor_t *mesh_accessor)
{
    tool_fuzzy_select_t *tool = (void*)user;
    uint8_t v0[4], v1[4];
    int d;

    mesh_get_at(mesh, mesh_accessor, base_pos, v0);
    mesh_get_at(mesh, mesh_accessor, new_pos, v1);
    if (!v0[3] || !v1[3]) return 0;

    d = max3(abs(v0[0] - v1[0]), abs(v0[1] - v1[1]), abs(v0[2] - v1[2]));
    return d <= tool->threshold ? 255 : 0;
}

static int on_click(gesture3d_t *gest, void *user)
{
    mesh_t *mesh = goxel.image->active_layer->mesh;
    int pi[3];
    cursor_t *curs = gest->cursor;
    tool_fuzzy_select_t *tool = (void*)user;

    pi[0] = floor(curs->pos[0]);
    pi[1] = floor(curs->pos[1]);
    pi[2] = floor(curs->pos[2]);
    if (!tool->selection) tool->selection = mesh_new();
    mesh_clear(tool->selection);
    mesh_select(mesh, pi, select_cond, tool, tool->selection);
    return 0;
}


static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    cursor_t *curs = &goxel.cursor;
    tool_fuzzy_select_t *tool = (void*)tool_;

    curs->snap_offset = -0.5;
    curs->snap_mask &= ~SNAP_ROUNDED;

    if (!tool->gestures.click.type) {
        tool->gestures.click = (gesture3d_t) {
            .type = GESTURE_CLICK,
            .callback = on_click,
        };
    }
    gesture3d(&tool->gestures.click, curs, tool);

    if (tool->selection) {
        render_mesh(&goxel.rend, tool->selection, NULL, EFFECT_GRID_ONLY);
    }

    return 0;
}

static layer_t *cut_as_new_layer(image_t *img, layer_t *layer,
                                 const mesh_t *mask)
{
    layer_t *new_layer;

    new_layer = image_duplicate_layer(img, layer);
    mesh_merge(new_layer->mesh, mask, MODE_INTERSECT, NULL);
    mesh_merge(layer->mesh, mask, MODE_SUB, NULL);
    return new_layer;
}

static int gui(tool_t *tool_)
{
    tool_fuzzy_select_t *tool = (void*)tool_;
    bool use_color = tool->threshold < 255;

    if (gui_checkbox("Use color", &use_color, "Stop at different color")) {
        tool->threshold = use_color ? 0 : 255;
    }
    if (use_color) {
        gui_input_int("Threshold", &tool->threshold, 1, 254);
    }

    if (!tool->selection || mesh_is_empty(tool->selection))
        return 0;

    mesh_t *mesh = goxel.image->active_layer->mesh;

    gui_group_begin(NULL);
    if (gui_button("Clear", 1, 0)) {
        image_history_push(goxel.image);
        mesh_merge(mesh, tool->selection, MODE_SUB, NULL);
    }
    if (gui_button("Fill", 1, 0)) {
        image_history_push(goxel.image);
        mesh_merge(mesh, tool->selection, MODE_OVER, goxel.painter.color);
    }
    if (gui_button("Cut as new layer", 1, 0)) {
        image_history_push(goxel.image);
        cut_as_new_layer(goxel.image, goxel.image->active_layer,
                         tool->selection);
    }
    gui_group_end();
    return 0;
}

TOOL_REGISTER(TOOL_FUZZY_SELECT, fuzzy_select, tool_fuzzy_select_t,
              .name = "Fuzzy Select",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT,
)
