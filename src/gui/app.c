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
void gui_snap_panel(void);
void gui_symmetry_panel(void);
bool gui_rotation_bar(void);

enum {
    PANEL_NULL,
    PANEL_TOOLS,
    PANEL_PALETTE,
    PANEL_LAYERS,
    PANEL_SNAP,
    PANEL_SYMMETRY,
    PANEL_VIEW,
    PANEL_MATERIAL,
    PANEL_LIGHT,
    PANEL_CAMERAS,
    PANEL_IMAGE,
    PANEL_RENDER,
    PANEL_EXPORT,
    PANEL_DEBUG,
};

static struct {
    int name;
    int icon;
    void (*fn)(void);
    bool detached;
} PANELS[] = {
    [PANEL_TOOLS]       = {STR_TOOLS, ICON_TOOLS, gui_tools_panel},
    [PANEL_PALETTE]     = {STR_PALETTE, ICON_PALETTE, gui_palette_panel},
    [PANEL_LAYERS]      = {STR_LAYERS, ICON_LAYERS, gui_layers_panel},
    [PANEL_SNAP]        = {STR_SNAP, ICON_SNAP, gui_snap_panel},
    [PANEL_SYMMETRY]    = {STR_SYMMETRY, ICON_SYMMETRY, gui_symmetry_panel},
    [PANEL_VIEW]        = {STR_VIEW, ICON_VIEW, gui_view_panel},
    [PANEL_MATERIAL]    = {STR_MATERIALS, ICON_MATERIAL, gui_material_panel},
    [PANEL_LIGHT]       = {STR_LIGHT, ICON_LIGHT, gui_light_panel},
    [PANEL_CAMERAS]     = {STR_CAMERAS, ICON_CAMERA, gui_cameras_panel},
    [PANEL_IMAGE]       = {STR_IMAGE, ICON_IMAGE, gui_image_panel},
#if YOCTO
    [PANEL_RENDER]      = {STR_RENDER, ICON_RENDER, gui_render_panel},
#endif
    [PANEL_EXPORT]      = {STR_EXPORT, ICON_EXPORT, gui_export_panel},
#if DEBUG
    [PANEL_DEBUG]       = {STR_DEBUG, ICON_DEBUG, gui_debug_panel},
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
        if (gui_tab(tr(PANELS[i].name), PANELS[i].icon, &selected)) {
            on_click();
            goxel.gui.current_panel = selected ? i : 0;
        }
    }
}

void gui_app(void)
{
    float x = 0, y = 0;
    const char *name;
    const float spacing = 8;
    int flags;
    int i;
    filter_t *filter;

    goxel.show_export_viewport = false;

    if (GUI_HAS_MENU) {
        if (gui_menu_bar_begin()) {
            gui_menu();

            // Add the Help test in the top menu.
            gui_menu_begin("      ", false);
            gui_menu_begin(goxel.hint_text ?: "", false);
            gui_menu_begin("      ", false);
            gui_menu_begin(goxel.help_text ?: "", false);
            goxel_set_help_text(NULL);
            goxel_set_hint_text(NULL);
            gui_menu_bar_end();
        }
        y = ITEM_HEIGHT + 2;
    }

    gui_window_begin("Top Bar", x, y, 0, 0, 0);
    gui_top_bar();
    y += gui_window_end().h + spacing;

    gui_window_begin("Left Bar", x, y, 0, 0, 0);
    render_left_panel();
    x += gui_window_end().w + spacing;

    if (goxel.gui.current_panel) {
        name = tr(PANELS[goxel.gui.current_panel].name);
        flags = gui_window_begin(
                name, x, y, goxel.gui.panel_width, 0, GUI_WINDOW_MOVABLE);
        if (gui_panel_header(name))
            goxel.gui.current_panel = 0;
        else
            PANELS[goxel.gui.current_panel].fn();
        gui_window_end();

        if (flags & GUI_WINDOW_MOVED) {
            PANELS[goxel.gui.current_panel].detached = true;
            goxel.gui.current_panel = 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(PANELS); i++) {
        if (!PANELS[i].detached) continue;
        name = tr(PANELS[i].name);
        gui_window_begin(name, 0, 0, goxel.gui.panel_width, 0,
                         GUI_WINDOW_MOVABLE);
        if (gui_panel_header(name)) {
            PANELS[i].detached = false;
        }
        PANELS[i].fn();
        gui_window_end();
    }

    filter = goxel.gui.current_filter;
    if (filter) {
        // XXX: we should have a way to center the filter window.
        gui_window_begin(filter->name, 100, 100, goxel.gui.panel_width, 0,
                         GUI_WINDOW_MOVABLE);
        if (gui_panel_header(filter->name)) {
            if (goxel.gui.current_filter->on_close) {
                goxel.gui.current_filter->on_close(goxel.gui.current_filter);
            }
            goxel.gui.current_filter = NULL;
        }
        filter->gui_fn(filter);
        gui_window_end();
    }

    goxel.pathtrace = goxel.pathtracer.status &&
        (goxel.gui.current_panel == PANEL_RENDER ||
         PANELS[PANEL_RENDER].detached);

    gui_view_cube(goxel.gui.viewport[2] - 128, ITEM_HEIGHT + 2, 128, 128);
}
