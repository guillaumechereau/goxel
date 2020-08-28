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

#define DL_FOREACH_REVERSE(head, el) \
    for (el = (head) ? (head)->prev : NULL; el; \
            el = (el == (head)) ? NULL : el->prev)

static void toggle_layer_only_visible(layer_t *layer)
{
    layer_t *other;
    bool others_all_invisible = true;
    DL_FOREACH(goxel.image->layers, other) {
        if (other == layer) continue;
        if (other->visible) {
            others_all_invisible = false;
            break;
        }
    }
    DL_FOREACH(goxel.image->layers, other)
        other->visible = others_all_invisible;
    layer->visible = true;
}


void gui_layers_panel(void)
{
    layer_t *layer;
    material_t *material;
    int i = 0, icon, bbox[2][3];
    bool current, visible, bounded;

    gui_group_begin(NULL);
    DL_FOREACH_REVERSE(goxel.image->layers, layer) {
        current = goxel.image->active_layer == layer;
        visible = layer->visible;
        icon = layer->base_id ? ICON_LINK : layer->shape ? ICON_SHAPE : -1;
        gui_layer_item(i, icon, &visible, &current,
                       layer->name, sizeof(layer->name));
        if (current && goxel.image->active_layer != layer) {
            goxel.image->active_layer = layer;
        }
        if (visible != layer->visible) {
            layer->visible = visible;
            if (gui_is_key_down(KEY_LEFT_SHIFT))
                toggle_layer_only_visible(layer);
        }
        i++;
    }
    gui_group_end();
    gui_action_button(ACTION_img_new_layer, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_img_del_layer, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_img_move_layer_up, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_img_move_layer_down, NULL, 0);

    gui_group_begin(NULL);
    gui_action_button(ACTION_img_duplicate_layer, "Duplicate", 1);
    gui_action_button(ACTION_img_clone_layer, "Clone", 1);
    gui_action_button(ACTION_img_merge_visible_layers, "Merge visible", 1);

    layer = goxel.image->active_layer;
    bounded = !box_is_null(layer->box);
    if (bounded && gui_button("Crop to box", 1, 0)) {
        mesh_crop(layer->mesh, layer->box);
    }
    if (!box_is_null(goxel.image->box) && gui_button("Crop to image", 1, 0)) {
        mesh_crop(layer->mesh, goxel.image->box);
    }
    if (layer->shape)
        gui_action_button(ACTION_img_unclone_layer, "To mesh", 1);

    if (gui_action_button(ACTION_img_new_shape_layer, "New Shape Layer", 1)) {
        action_exec2(ACTION_tool_set_move);
    }

    gui_group_end();

    if (layer->base_id) {
        gui_group_begin(NULL);
        gui_action_button(ACTION_img_unclone_layer, "Unclone", 1);
        gui_action_button(ACTION_img_select_parent_layer, "Select parent", 1);
        gui_group_end();
    }
    if (layer->image) {
        gui_action_button(ACTION_img_image_layer_to_mesh, "To Mesh", 1);
    }
    if (!layer->shape && gui_checkbox("Bounded", &bounded, NULL)) {
        if (bounded) {
            mesh_get_bbox(layer->mesh, bbox, true);
            if (bbox[0][0] > bbox[1][0]) memset(bbox, 0, sizeof(bbox));
            bbox_from_aabb(layer->box, bbox);
        } else {
            mat4_copy(mat4_zero, layer->box);
        }
    }
    if (bounded)
        gui_bbox(layer->box);

    if (layer->shape) {
        tool_gui_drag_mode(&goxel.tool_drag_mode);
        tool_gui_shape(&layer->shape);
        gui_color("##color", layer->color);
    }

    gui_text("Material");
    if (gui_combo_begin("##material",
                        layer->material ? layer->material->name : NULL)) {
        DL_FOREACH(goxel.image->materials, material) {
            if (gui_combo_item(material->name, material == layer->material))
                layer->material = material;
        }
        gui_combo_end();
    }
}
