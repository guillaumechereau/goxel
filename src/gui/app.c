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

#ifndef GUI_HAS_ROTATION_BAR
#   define GUI_HAS_ROTATION_BAR 0
#endif

#ifndef GUI_HAS_HELP
#   define GUI_HAS_HELP 1
#endif

#ifndef GUI_HAS_MENU
#   define GUI_HAS_MENU 1
#endif

#ifndef YOCTO
#   define YOCTO 1
#endif

// Note: duplicated from gui.cpp!  To remove.
static const float ITEM_HEIGHT = 18;
static const float ICON_HEIGHT = 32;

void gui_menu(void);
void gui_tools_panel(void);
void gui_top_bar(void);
void gui_palette_panel(void);
void gui_layers_panel(void);
void gui_view_panel(void);
void gui_material_panel(void);
void gui_light_panel(void);
void gui_cameras_panel(void);
void gui_image_panel(void);
void gui_render_panel(void);
void gui_debug_panel(void);
void gui_export_panel(void);
bool gui_rotation_bar(void);

static const struct {
    const char *name;
    int icon;
    void (*fn)(void);
} PANELS[] = {
    {NULL},
    {"Tools", ICON_TOOLS, gui_tools_panel},
    {"Palette", ICON_PALETTE, gui_palette_panel},
    {"Layers", ICON_LAYERS, gui_layers_panel},
    {"View", ICON_VIEW, gui_view_panel},
    {"Material", ICON_MATERIAL, gui_material_panel},
    {"Light", ICON_LIGHT, gui_light_panel},
    {"Cameras", ICON_CAMERA, gui_cameras_panel},
    {"Image", ICON_IMAGE, gui_image_panel},
#if YOCTO
    {"Render", ICON_RENDER, gui_render_panel},
#endif
    {"Export", ICON_EXPORT, gui_export_panel},
#if DEBUG
    {"Debug", ICON_DEBUG, gui_debug_panel},
#endif
};


static void on_click(void) {
    if (DEFINED(GUI_SOUND))
        sound_play("click", 1.0, 1.0);
}

static void render_left_panel(void)
{
    int i;
    bool selected;

    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++) {
        selected = (goxel.gui.current_panel == i);
        if (gui_tab(PANELS[i].name, PANELS[i].icon, &selected)) {
            on_click();
            goxel.gui.current_panel = selected ? i : 0;
        }
    }
}

void gui_app(void)
{
    float x = 0, y = 0;

    goxel.show_export_viewport = false;

    if (GUI_HAS_MENU) {
        if (gui_menu_bar_begin()) {
            gui_menu();

            // Add the Help test in the top menu.
            gui_menu_begin("      ", false);
            gui_menu_begin(goxel.hint_text ?: "", false);
            gui_menu_begin("      ", false);
            gui_menu_begin(goxel.help_text ?: "", false);

            gui_menu_bar_end();
        }
        y = ITEM_HEIGHT + 2;
    }

    gui_window_begin("Top Bar", x, y, 0, 0);
    gui_top_bar();
    gui_window_end();

    y += ICON_HEIGHT + 28;
    gui_window_begin("Left Bar", x, y, 0, 0);
    render_left_panel();
    gui_window_end();

    if (goxel.gui.current_panel) {
        x += ICON_HEIGHT + 28;
        gui_window_begin("Controls", x, y, goxel.gui.panel_width, 0);

        if (gui_panel_header(PANELS[goxel.gui.current_panel].name))
            goxel.gui.current_panel = 0;
        else
            PANELS[goxel.gui.current_panel].fn();

        gui_window_end();
    }

    goxel.pathtrace = goxel.gui.current_panel &&
        PANELS[goxel.gui.current_panel].fn == gui_render_panel &&
        goxel.pathtracer.status;
}
