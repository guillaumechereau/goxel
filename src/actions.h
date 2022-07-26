/*
 * This file contains the list of all the actions, in the form of an
 * enum of values ACTION_<name>.
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#define X(name) ACTION_##name

enum {
    ACTION_NULL = 0,

    X(layer_clear),
    X(img_new_layer),
    X(img_del_layer),
    X(img_move_layer_up),
    X(img_move_layer_down),
    X(img_duplicate_layer),
    X(img_clone_layer),
    X(img_hide_all_layers),
    X(img_show_all_layers),
    X(img_unclone_layer),
    X(img_select_parent_layer),
    X(img_merge_visible_layers),
    X(img_new_camera),
    X(img_del_camera),
    X(img_move_camera_up),
    X(img_move_camera_down),
    X(img_image_layer_to_mesh),
    X(img_new_shape_layer),
    X(img_new_material),
    X(img_del_material),
    X(img_auto_resize),

    X(cut_as_new_layer),
    X(reset_selection),
    X(fill_selection),
    X(copy),
    X(past),
    X(view_left),
    X(view_right),
    X(view_top),
    X(view_default),
    X(view_front),
    X(quit),
    X(undo),
    X(redo),
    X(toggle_mode),
    X(export_render_buf_to_photos),
    X(open),
    X(save_as),
    X(save),
    X(reset),

    X(tool_set_brush),
    X(tool_set_laser),
    X(tool_set_shape),
    X(tool_set_pick_color),
    X(tool_set_extrude),
    X(tool_set_plane),
    X(tool_set_selection),
    X(tool_set_fuzzy_select),
    X(tool_set_line),
    X(tool_set_move),

    X(export_to_photos),
    X(open_run_lua_plugin),

    ACTION_COUNT
};

#undef X

#endif // ACTIONS_H
