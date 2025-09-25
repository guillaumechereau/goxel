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

#include "filters.h"
#include "goxel.h"

#include "../ext_src/stb/stb_ds.h"

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

#define INITIAL_FILTER_OFFSET 10
#define RELATIVE_FILTER_OFFSET 40

void gui_edit_panel(void);
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
    PANEL_EDIT,
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
    const char *name;
    int icon;
    void (*fn)(void);
    bool detached;
} PANELS[] = {
    [PANEL_TOOLS]       = {N_("Tools"), ICON_TOOLS, gui_tools_panel},
    [PANEL_PALETTE]     = {N_("Palette"), ICON_PALETTE, gui_palette_panel},
    [PANEL_EDIT]        = {N_("Edit"), ICON_HAMMER, gui_edit_panel},
    [PANEL_LAYERS]      = {N_("Layers"), ICON_LAYERS, gui_layers_panel},
    [PANEL_SNAP]        = {N_("Snap"), ICON_SNAP, gui_snap_panel},
    [PANEL_SYMMETRY]    = {N_("Symmetry"), ICON_SYMMETRY, gui_symmetry_panel},
    [PANEL_VIEW]        = {N_("View"), ICON_VIEW, gui_view_panel},
    [PANEL_MATERIAL]    = {N_("Materials"), ICON_MATERIAL, gui_material_panel},
    [PANEL_LIGHT]       = {N_("Light"), ICON_LIGHT, gui_light_panel},
    [PANEL_CAMERAS]     = {N_("Cameras"), ICON_CAMERA, gui_cameras_panel},
    [PANEL_IMAGE]       = {N_("Image"), ICON_IMAGE, gui_image_panel},
#if YOCTO
    [PANEL_RENDER]      = {N_("Render"), ICON_RENDER, gui_render_panel},
#endif
    [PANEL_EXPORT]      = {N_("Export"), ICON_EXPORT, gui_export_panel},
#if DEBUG
    [PANEL_DEBUG]       = {N_("Debug"), ICON_DEBUG, gui_debug_panel},
#endif
};

typedef struct filter_layout_state filter_layout_state_t;

struct filter_layout_state {
    int next_x;
    int next_y;
};

static void on_click(void) {
    if (DEFINED(GUI_SOUND))
        sound_play("click", 1.0, 1.0);
}

void gui_open_export_panel(void)
{
    goxel.gui.current_panel = PANEL_EXPORT;
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

// Compute the order to render the hints.
static int hint_render_priority(const hint_t *hint)
{
    if (hint->flags & HINT_COORDINATES) return 100;
    if (strstr(hint->title, GLYPH_MOUSE_LMB)) return 1;
    if (strstr(hint->title, GLYPH_MOUSE_MMB)) return 2;
    if (strstr(hint->title, GLYPH_MOUSE_RMB)) return 3;
    return 10;
}

// Not too sure about this.
static int hints_cmp(const void *a_, const void *b_)
{
    const hint_t *a = a_;
    const hint_t *b = b_;
    int ret;
    ret = cmp(hint_render_priority(a), hint_render_priority(b));
    if (ret) return ret;
    return -strcmp(a->title, b->title);
}

static void render_hints(const hint_t *hints)
{
    const float size = 150; // Size in pixel per hint.
    int i;
    float pos = gui_get_current_pos_x() + 0.5 * size;
    for (i = 0; i < arrlen(hints); i++) {
        gui_set_current_pos_x(pos);
        if (hints[i].title[0]) {
            gui_text(hints[i].title);
        }
        gui_text(hints[i].msg);
        pos += size;
        if (hints[i].flags & HINT_LARGE) pos += 0.5 * size;
    }
}

static void gui_filter_window(void *arg, filter_t *filter)
{
    filter_layout_state_t *state = arg;

    if (filter->is_open) {
        gui_window_begin(filter->name, state->next_x, state->next_y,
                            goxel.gui.panel_width, 0, GUI_WINDOW_MOVABLE);

        if (gui_panel_header(filter->name)) {
            if (filter->on_close) {
                filter->on_close(filter);
            }
            filter->is_open = false;
        }
        filter->gui_fn(filter);

        gui_window_end();
    }

    state->next_x += RELATIVE_FILTER_OFFSET;
    state->next_y += RELATIVE_FILTER_OFFSET;
}

void gui_app(void)
{
    float x = 0, y = 0;
    const char *name;
    const float spacing = 8;
    int flags;
    int i;
    filter_layout_state_t filter_layout_state;
    const float item_height = gui_get_item_height();

    goxel.show_export_viewport = false;

    if (GUI_HAS_MENU) {
        if (gui_menu_bar_begin()) {
            gui_menu();

            // Add the Help test in the top menu.
            qsort(goxel.hints, arrlen(goxel.hints), sizeof(hint_t), hints_cmp);
            render_hints(goxel.hints);
            gui_menu_bar_end();
        }
        y = item_height + 2;
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

    filter_layout_state.next_x = x + goxel.gui.panel_width +
                                    INITIAL_FILTER_OFFSET;
    filter_layout_state.next_y = y;
    filters_iter_all(&filter_layout_state, gui_filter_window);

    goxel.pathtrace = goxel.pathtracer.status &&
        (goxel.gui.current_panel == PANEL_RENDER ||
         PANELS[PANEL_RENDER].detached);

    gui_view_cube(goxel.gui.viewport[2] - 128, item_height + 2, 128, 128);
}
