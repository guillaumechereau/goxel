/* Goxel 3D voxels editor
 *
 * copyright (c) 2020 Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * This file contains the list of all the actions, in the form of an
 * enum of values ACTION_<name>.
 */

#ifndef ACTIONS_H
#define ACTIONS_H

enum {
    ACTION_NULL = 0,

    ACTION_layer_clear,
    ACTION_img_new_layer,
    ACTION_img_del_layer,
    ACTION_img_move_layer_up,
    ACTION_img_move_layer_down,
    ACTION_img_duplicate_layer,
    ACTION_img_clone_layer,
    ACTION_img_unclone_layer,
    ACTION_img_select_parent_layer,
    ACTION_img_merge_visible_layers,
    ACTION_img_merge_layer_down,
    ACTION_img_new_camera,
    ACTION_img_del_camera,
    ACTION_img_move_camera_up,
    ACTION_img_move_camera_down,
    ACTION_img_image_layer_to_volume,
    ACTION_img_new_shape_layer,
    ACTION_img_new_material,
    ACTION_img_del_material,
    ACTION_img_auto_resize,

    ACTION_cut_as_new_layer,
    ACTION_reset_selection,
    ACTION_fill_selection_box,
    ACTION_paint_selection,
    ACTION_add_selection,
    ACTION_sub_selection,
    ACTION_copy,
    ACTION_paste,
    ACTION_view_left,
    ACTION_view_right,
    ACTION_view_top,
    ACTION_view_toggle_ortho,
    ACTION_view_default,
    ACTION_view_front,
    ACTION_quit,
    ACTION_undo,
    ACTION_redo,
    ACTION_toggle_mode,
    ACTION_set_mode_add,
    ACTION_set_mode_sub,
    ACTION_set_mode_paint,
    ACTION_export_render_buf_to_photos,
    ACTION_open,
    ACTION_save_as,
    ACTION_save,
    ACTION_overwrite_export,
    ACTION_reset,

    ACTION_tool_set_brush,
    ACTION_tool_set_laser,
    ACTION_tool_set_shape,
    ACTION_tool_set_pick_color,
    ACTION_tool_set_extrude,
    ACTION_tool_set_plane,
    ACTION_tool_set_selection,
    ACTION_tool_set_fuzzy_select,
    ACTION_tool_set_rect_select,
    ACTION_tool_set_line,
    ACTION_tool_set_move,

    ACTION_export_to_photos,

    ACTION_COUNT
};

#undef X

#endif // ACTIONS_H
