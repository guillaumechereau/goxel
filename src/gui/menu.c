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

#include "file_format.h"

int gui_settings_popup(void *data);
int gui_about_popup(void *data);

static void import_image_plane(void)
{
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
            "png\0*.png\0jpg\0*.jpg;*.jpeg\0", NULL, NULL);
    if (!path) return;
    goxel_import_image_plane(path);
}

static void import_menu_callback(void *user, const file_format_t *f)
{
    if (gui_menu_item(0, f->name, true))
        goxel_import_file(NULL, f->name);
}

static void export_menu_callback(void *user, const file_format_t *f)
{
    if (gui_menu_item(0, f->name, true))
        goxel_export_to_file(NULL, f->name);
}

void gui_menu(void)
{
    if (gui_menu_begin("File")) {
        gui_menu_item(ACTION_reset, "New", true);
        gui_menu_item(ACTION_save, "Save",
                image_get_key(goxel.image) != goxel.image->saved_key);
        gui_menu_item(ACTION_save_as, "Save as", true);
        gui_menu_item(ACTION_open, "Open", true);
        if (gui_menu_begin("Import...")) {
            if (gui_menu_item(0, "image plane", true))
                import_image_plane();
            file_format_iter("r", NULL, import_menu_callback);
            gui_menu_end();
        }
        if (gui_menu_begin("Export As..")) {
            file_format_iter("w", NULL, export_menu_callback);
            gui_menu_end();
        }
        gui_menu_item(ACTION_quit, "Quit", true);
        gui_menu_end();
    }
    if (gui_menu_begin("Edit")) {
        gui_menu_item(ACTION_layer_clear, "Clear", true);
        gui_menu_item(ACTION_undo, "Undo", true);
        gui_menu_item(ACTION_redo, "Redo", true);
        gui_menu_item(ACTION_copy, "Copy", true);
        gui_menu_item(ACTION_past, "Paste", true);
        if (gui_menu_item(0, "Settings", true))
            gui_open_popup("Settings", GUI_POPUP_FULL | GUI_POPUP_RESIZE,
                           NULL, gui_settings_popup);
        gui_menu_end();
    }
    if (gui_menu_begin("View")) {
        gui_menu_item(ACTION_view_left, "Left", true);
        gui_menu_item(ACTION_view_right, "Right", true);
        gui_menu_item(ACTION_view_front, "Front", true);
        gui_menu_item(ACTION_view_top, "Top", true);
        gui_menu_item(ACTION_view_default, "Default", true);
        gui_menu_end();
    }
    if (gui_menu_begin("Help")) {
        if (gui_menu_item(0, "About", true))
            gui_open_popup("About", 0, NULL, gui_about_popup);
        gui_menu_end();
    }
}
