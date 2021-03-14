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

enum {
    GUI_CLOSE_IMAGE_POPUP_AWAITING = 0,
    GUI_CLOSE_IMAGE_POPUP_CANCEL,
    GUI_CLOSE_IMAGE_POPUP_NO,
    GUI_CLOSE_IMAGE_POPUP_YES,
};

// Expects data of type void(*)(void) pointing to function to call after user
// makes choice
int gui_close_image_popup(void *data)
{
    gui_text("Current document has been modified.");
    gui_text("Do you want to save it?");

    int res = GUI_CLOSE_IMAGE_POPUP_AWAITING;
    if (gui_button("Yes", 0, 0))
        res = GUI_CLOSE_IMAGE_POPUP_YES;

    gui_same_line();
    if (gui_button("Cancel", 0, 0))
        res = GUI_CLOSE_IMAGE_POPUP_CANCEL;

    gui_same_line();
    if (gui_button_right("No", 0))
        res = GUI_CLOSE_IMAGE_POPUP_NO;

    return res;
}

void gui_close_image_popup_closed(int result, void *data)
{
    if (result == GUI_CLOSE_IMAGE_POPUP_NO)
        ((void(*)(void))data)();
    else if (result == GUI_CLOSE_IMAGE_POPUP_YES) {
        const action_t *save = action_get(ACTION_save, true);
        action_exec(save);
        
        ((void(*)(void))data)();
    }
}

