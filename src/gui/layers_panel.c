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

static const char *get_mode_name(int mode)
{
    switch (mode) {
        case MODE_OVER: return "Add";
        case MODE_SUB: return "Subtract";
        case MODE_INTERSECT: return "Intersect";
        default: return "";
    }
}

void gui_layers_panel(void)
{
    layer_t *layer;
    material_t *material;
    int i = 0, bbox[2][3];
    int icons_count, icons[8];
    bool current, visible, bounded;
    char buf[256];
    const int MODES[] = {MODE_OVER, MODE_SUB, MODE_INTERSECT};

    gui_group_begin(NULL);
    DL_FOREACH_REVERSE(goxel.image->layers, layer) {
        current = goxel.image->active_layer == layer;
        visible = layer->visible;
        icons_count = 0;
        if (layer->base_id) icons[icons_count++] = ICON_LINK;
        if (layer->shape) icons[icons_count++] = ICON_SHAPE;
        if (layer->mode == MODE_SUB) icons[icons_count++] = ICON_SUBTRACT;
        if (layer->mode == MODE_INTERSECT)
            icons[icons_count++] = ICON_INTERSECT;
        gui_layer_item(i, icons_count, icons, &visible, &current,
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

    gui_row_begin(0);
    gui_action_button(ACTION_img_new_layer, NULL, 0);
    gui_action_button(ACTION_img_del_layer, NULL, 0);
    gui_action_button(ACTION_img_move_layer_up, NULL, 0);
    gui_action_button(ACTION_img_move_layer_down, NULL, 0);
    gui_row_end();

    gui_group_begin(NULL);
    gui_action_button(ACTION_img_duplicate_layer, _(DUPLICATE), 1);
    gui_action_button(ACTION_img_clone_layer, _(CLONE), 1);
    gui_action_button(ACTION_img_merge_layer_down, _(MERGE_DOWN), 1);
    gui_action_button(ACTION_img_merge_visible_layers, _(MERGE_VISIBLE), 1);

    layer = goxel.image->active_layer;
    bounded = !box_is_null(layer->box);
    if (bounded && gui_button(_(CROP), 1, 0)) {
        volume_crop(layer->volume, layer->box);
    }
    if (!box_is_null(goxel.image->box)) {
        snprintf(buf, sizeof(buf), "%s: %s", _(CROP), _(IMAGE));
        if (gui_button(buf, 1, 0))
            volume_crop(layer->volume, goxel.image->box);
    }
    if (layer->shape) {
        snprintf(buf, sizeof(buf), "▶ %s", _(VOLUME));
        gui_action_button(ACTION_img_unclone_layer, buf, 1);
    }

    snprintf(buf, sizeof(buf), "%s: %s", _(ADD), _(SHAPE));
    if (gui_action_button(ACTION_img_new_shape_layer, buf, 1)) {
        action_exec2(ACTION_tool_set_move);
    }

    gui_group_end();

    if (layer->base_id) {
        gui_group_begin(NULL);
        gui_action_button(ACTION_img_unclone_layer, _(UNLINK), 1);
        gui_action_button(ACTION_img_select_parent_layer, "Select parent", 1);
        gui_group_end();
    }
    if (layer->image) {
        snprintf(buf, sizeof(buf), "-> %s", _(VOLUME));
        gui_action_button(ACTION_img_image_layer_to_volume, buf, 1);
    }
    if (!layer->shape && gui_checkbox(_(BOUNDED), &bounded, NULL)) {
        if (bounded) {
            volume_get_bbox(layer->volume, bbox, true);
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

    gui_text(_(MATERIAL));
    if (gui_combo_begin("##material",
                        layer->material ? layer->material->name : NULL)) {
        DL_FOREACH(goxel.image->materials, material) {
            if (gui_combo_item(material->name, material == layer->material))
                layer->material = material;
        }
        gui_combo_end();
    }

    gui_text("Mode");
    if (gui_combo_begin("##mode", get_mode_name(layer->mode))) {
        for (i = 0; i < ARRAY_SIZE(MODES); i++) {
            current = MODES[i] == layer->mode;
            if (gui_combo_item(get_mode_name(MODES[i]), current)) {
                layer->mode = MODES[i];
            }
        }
        gui_combo_end();
    }
}
