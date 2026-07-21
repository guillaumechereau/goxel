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

// --- Layout persistence -----------------------------------------------------

static int panel_index_by_name(const char *name)
{
    int i;
    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++) {
        if (PANELS[i].name && strcmp(PANELS[i].name, name) == 0)
            return i;
    }
    return 0;
}

const char *gui_layout_current_panel(void)
{
    if (!goxel.gui.current_panel) return NULL;
    return PANELS[goxel.gui.current_panel].name;
}

int gui_layout_detached_panels(const char **out, int max)
{
    int i, n = 0;
    for (i = 1; i < (int)ARRAY_SIZE(PANELS) && n < max; i++) {
        if (PANELS[i].detached && PANELS[i].name)
            out[n++] = PANELS[i].name;
    }
    return n;
}

void gui_layout_set_current_panel(const char *name)
{
    goxel.gui.current_panel = panel_index_by_name(name);
}

void gui_layout_set_panel_detached(const char *name)
{
    int i = panel_index_by_name(name);
    if (i) PANELS[i].detached = true;
}

void gui_layout_reset(void)
{
    char path[1024];
    int i;

    goxel.gui.current_panel = 0;
    for (i = 0; i < (int)ARRAY_SIZE(PANELS); i++)
        PANELS[i].detached = false;
    goxel.gui.reset_layout = true;
    gui_clear_window_settings();
    snprintf(path, sizeof(path), "%s/imgui.ini", sys_get_user_dir());
    remove(path);
    // settings.ini is rewritten by save_layout_if_changed() at the end of the
    // frame, once it sees the layout changed.
}

// Save the layout to settings.ini when it changed since the last frame.
static void save_layout_if_changed(void)
{
    static int last_panel = -1;
    static uint32_t last_detached = 0;
    uint32_t detached = 0;
    int i;

    // The detached state is tracked as a bitmask, one bit per panel.
    _Static_assert(ARRAY_SIZE(PANELS) <= 32, "too many panels for the bitmask");
    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++)
        if (PANELS[i].detached) detached |= (1u << i);

    if (last_panel == -1) {
        // First frame: adopt the just-restored state without saving it.
        last_panel = goxel.gui.current_panel;
        last_detached = detached;
        return;
    }
    if (goxel.gui.current_panel == last_panel && detached == last_detached)
        return;
    last_panel = goxel.gui.current_panel;
    last_detached = detached;
    settings_save();
}

static void on_click(void) {
    if (DEFINED(GUI_SOUND))
        sound_play("click", 1.0, 1.0);
}

static void render_left_panel(void)
{
    int i;
    bool active, selected;

    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++) {
        if (!PANELS[i].name) continue;
        // A panel is "active" (button highlighted) when it is visible, either
        // docked or as a detached floating window.
        active = (goxel.gui.current_panel == i) || PANELS[i].detached;
        selected = active;
        if (gui_tab(tr(PANELS[i].name), PANELS[i].icon, &selected)) {
            on_click();
            if (active) {
                // Toggle off: close the docked panel or the detached window.
                if (goxel.gui.current_panel == i)
                    goxel.gui.current_panel = 0;
                PANELS[i].detached = false;
            } else {
                // Open docked, replacing any other docked panel.
                goxel.gui.current_panel = i;
                PANELS[i].detached = false;
            }
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
                            goxel.gui.panel_width, 0,
                            GUI_WINDOW_MOVABLE | GUI_WINDOW_PERSIST);

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
    char win_id[128];
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
        // Use a language independent window id (### suffix) so imgui.ini keeps
        // matching the window across UI language changes.
        snprintf(win_id, sizeof(win_id), "%s###%s", name,
                 PANELS[goxel.gui.current_panel].name);
        flags = gui_window_begin(
                win_id, x, y, goxel.gui.panel_width, 0, GUI_WINDOW_MOVABLE);
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
        snprintf(win_id, sizeof(win_id), "%s###%s", name, PANELS[i].name);
        gui_window_begin(win_id, 0, 0, goxel.gui.panel_width, 0,
                         GUI_WINDOW_MOVABLE | GUI_WINDOW_PERSIST);
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

    save_layout_if_changed();
    goxel.gui.reset_layout = false;
}
