/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

static bool have_volume_to_paste()
{
    const char *clipboard_text;
    int size[3];

    clipboard_text = sys_callbacks.get_clipboard_text(sys_callbacks.user);
    if (clipboard_text == NULL) {
        return false;
    }

    if (volume_parse_string_header(clipboard_text, size) == NULL) {
        return false;
    }

    return true;
}

void gui_edit_panel(void)
{
    image_t *img = goxel.image;

    gui_group_begin(NULL);
    gui_enabled_begin(!box_is_null(img->selection_box));
    gui_action_button(ACTION_fill_selection_box, _("Fill"), 1.0);
    gui_group_end();
    gui_enabled_end();

    gui_group_begin(NULL);
    gui_enabled_begin(!volume_is_empty(goxel.image->selection_mask));
    gui_action_button(ACTION_layer_clear, _("Clear"), 1.0);
    gui_action_button(ACTION_paint_selection, _("Paint"), 1.0);
    gui_action_button(ACTION_cut_as_new_layer, _("Cut as new layer"), 1.0);
    gui_enabled_end();
    gui_group_end();

    gui_group_begin(NULL);
    gui_enabled_begin(!volume_is_empty(goxel.image->selection_mask));
    gui_action_button(ACTION_copy, _("Copy"), 1.0);
    gui_enabled_end();
    gui_enabled_begin(have_volume_to_paste());
    gui_action_button(ACTION_paste, _("Paste"), 1.0);
    gui_enabled_end();
    gui_group_end();
}
