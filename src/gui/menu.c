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
#include "script.h"

#include "../../ext_src/stb/stb_ds.h"

int gui_settings_popup(void *data);
int gui_about_popup(void *data);
int gui_about_scripts_popup(void *data);

static void import_image_plane(void)
{
    const char *path;
    const char *filters[] = {"*.png", "*.jpg", "*.jpeg", NULL};
    path = sys_open_file_dialog("Open", NULL, filters, "png, jpeg");
    if (!path) return;
    goxel_import_image_plane(path);
}

static file_format_t *g_import_format = NULL;

static int import_gui(void *data)
{
    g_import_format->import_gui(g_import_format);
    if (gui_button("OK", 0, 0)) {
        goxel_import_file(NULL, g_import_format->name);
        return 1;
    }
    return 0;
}

static void import_menu_callback(void *user, file_format_t *f)
{
    if (!gui_menu_item(0, f->name, true)) return;
    if (f->import_gui) {
        g_import_format = f;
        gui_open_popup("Import", 0, NULL, import_gui);
        return;
    }
    goxel_import_file(NULL, f->name);
}

static void export_menu_callback(void *user, file_format_t *f)
{
    if (gui_menu_item(0, f->name, true))
        goxel_export_to_file(NULL, f->name);
}

static void on_script(void *user, const char *name)
{
    if (gui_menu_item(0, name, true))
        script_execute(name);
}

void gui_menu(void)
{
    camera_t *cam;
    int i;

    if (gui_menu_begin(_(FILE), true)) {
        gui_menu_item(ACTION_reset, _(NEW), true);
        gui_menu_item(ACTION_save, _(SAVE),
                image_get_key(goxel.image) != goxel.image->saved_key);
        gui_menu_item(ACTION_save_as, _(SAVE_AS), true);
        gui_menu_item(ACTION_open, _(OPEN), true);
        if (gui_menu_begin("Open Recent", true)) {
            for (i = 0; i < arrlen(goxel.recent_files); i++) {
                if (gui_menu_item(0, goxel.recent_files[i], true)) {
                    goxel_open_file(goxel.recent_files[i]);
                }
            }
            gui_menu_end();
        };
        if (gui_menu_begin(_(IMPORT), true)) {
            if (gui_menu_item(0, _(2D_IMAGE), true))
                import_image_plane();
            file_format_iter("r", NULL, import_menu_callback);
            gui_menu_end();
        }
        gui_menu_item(ACTION_overwrite_export, _(OVERWRITE_EXPORT), goxel.image->export_path != NULL);
        if (gui_menu_begin(_(EXPORT), true)) {
            file_format_iter("w", NULL, export_menu_callback);
            gui_menu_end();
        }
        gui_menu_item(ACTION_quit, _(QUIT), true);
        gui_menu_end();
    }
    if (gui_menu_begin(_(EDIT), true)) {
        gui_menu_item(ACTION_layer_clear, _(CLEAR), true);
        gui_menu_item(ACTION_undo, _(UNDO), true);
        gui_menu_item(ACTION_redo, _(REDO), true);
        gui_menu_item(ACTION_copy, _(COPY), true);
        gui_menu_item(ACTION_paste, _(PASTE), true);
        if (gui_menu_item(0, _(SETTINGS), true))
            gui_open_popup(_(SETTINGS), GUI_POPUP_FULL | GUI_POPUP_RESIZE,
                           NULL, gui_settings_popup);
        gui_menu_end();
    }
    if (gui_menu_begin(_(VIEW), true)) {
        cam = goxel.image->active_camera;
        gui_menu_item(ACTION_view_left, _(LEFT), true);
        gui_menu_item(ACTION_view_right, _(RIGHT), true);
        gui_menu_item(ACTION_view_front, _(FRONT), true);
        gui_menu_item(ACTION_view_top, _(TOP), true);
        gui_menu_item(ACTION_view_toggle_ortho,
                cam->ortho ? _(PERSPECTIVE) : _(ORTHOGRAPHIC), true);
        gui_menu_item(ACTION_view_default, _(RESET), true);
        gui_menu_end();
    }
    if (gui_menu_begin("Scripts", true)) {
        if (gui_menu_item(0, "About Scripts", true))
            gui_open_popup("Scripts", 0, NULL, gui_about_scripts_popup);
        script_iter_all(NULL, on_script);
        gui_menu_end();
    }
    if (gui_menu_begin("Help", true)) {
        if (gui_menu_item(0, "About", true))
            gui_open_popup("About", GUI_POPUP_RESIZE, NULL, gui_about_popup);
        gui_menu_end();
    }
}
