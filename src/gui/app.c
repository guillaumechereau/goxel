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

#ifndef YOCTO
#   define YOCTO 1
#endif


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
    int i, current_i = 0;
    bool selected;

    goxel.gui.panel_width = GUI_PANEL_WIDTH_NORMAL;

    gui_div_begin();
    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++) {
        selected = (goxel.gui.current_panel == PANELS[i].fn);
        if (selected) current_i = i;
        if (gui_tab(PANELS[i].name, PANELS[i].icon, &selected)) {
            on_click();
            goxel.gui.current_panel = selected ? PANELS[i].fn : NULL;
            current_i = goxel.gui.current_panel ? i : 0;
        }
    }
    gui_div_end();

    if (goxel.gui.current_panel) {
        gui_same_line();
        gui_div_begin();
        goxel.show_export_viewport = false;
        gui_push_id("panel");
        gui_push_id(PANELS[current_i].name);
        if (gui_panel_header(PANELS[current_i].name))
            goxel.gui.current_panel = NULL;
        else
            goxel.gui.current_panel();
        gui_pop_id();
        gui_pop_id();
        gui_div_end();
    }
}

static void render_view(void *user, const float viewport[4])
{
    bool render_mode;
    assert(sizeof(goxel.gui.viewport[0]) == sizeof(viewport[0]));
    memcpy(goxel.gui.viewport, viewport, sizeof(goxel.gui.viewport));
    render_mode = goxel.gui.current_panel == gui_render_panel &&
                  goxel.pathtracer.status;
    goxel_render_view(viewport, render_mode);
}

void gui_app(void)
{
    inputs_t inputs;
    bool has_mouse, has_keyboard;
    const theme_t *theme = theme_get();
    float menu_height = theme->sizes.icons_height * 0.7;
    float bottom_size = theme->sizes.item_height +
                        2 * theme->sizes.item_padding_h;

    gui_canvas(0, 0,
               &inputs, &has_mouse, &has_keyboard,
               NULL, render_view);

    if (GUI_HAS_ROTATION_BAR) {
        if (gui_rotation_bar())
            has_mouse = false;
    }

    gui_window_begin("top_bar", 0, menu_height, 0, 0);
    gui_top_bar();
    gui_window_end();

    gui_window_begin("left_panel", 0,
            theme->sizes.icons_height + menu_height +
            theme->sizes.item_padding_h * 2,
            goxel.gui.current_panel ? goxel.gui.panel_width : 0, 0);
    render_left_panel();
    gui_window_end();

    if (GUI_HAS_HELP) {
        gui_window_begin("bottom_bar", 0, -bottom_size, -1, -bottom_size);
        gui_text("%s", goxel.hint_text ?: "");
        gui_same_line();
        gui_spacing(180);
        gui_text("%s", goxel.help_text ?: "");
        gui_window_end();
    }

    // Call mouse_in_view with inputs in the view referential.
    if (has_mouse)
        goxel_mouse_in_view(goxel.gui.viewport, &inputs, has_keyboard);


}

