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

/*
 * Section: Gui
 *
 * XXX: add doc and cleanup this.
 */

#ifndef GUI_H
#define GUI_H

#include "inputs.h"

#ifndef GUI_PANEL_WIDTH_NORMAL
#   define GUI_PANEL_WIDTH_NORMAL 230
#endif

#ifndef GUI_PANEL_WIDTH_LARGE
#   define GUI_PANEL_WIDTH_LARGE 400
#endif

typedef struct {
    float h;
    float w;
} gui_window_ret_t;

void gui_window_begin(const char *label, float x, float y, float w, float h,
                      bool *moved);

gui_window_ret_t gui_window_end(void);

bool gui_want_capture_mouse(void);

void gui_release(void);
void gui_release_graphics(void);

void gui_render(inputs_t *inputs);

void gui_request_panel_width(float width);

bool gui_panel_header(const char *label);

// Gui widgets:
bool gui_collapsing_header(const char *label, bool default_opened);
void gui_text(const char *label, ...);
void gui_text_wrapped(const char *label, ...);
bool gui_button(const char *label, float w, int icon);
bool gui_button_right(const char *label, int icon);

// Group just lower the spacing between items.
void gui_group_begin(const char *label);
void gui_group_end(void);

// Section have a frame.

enum {
    GUI_SECTION_COLLAPSABLE             = 1 << 0,
    GUI_SECTION_COLLAPSABLE_CLOSED      = 1 << 1,
};

bool gui_section_begin(const char *label, int flags);
void gui_section_end(void);

// Auto layout the inner items in a row.
void gui_row_begin(int nb);
void gui_row_end(void);

/* Render a grid of same sized icons, where one can be selected.
 * For tools grid, shapes... */
typedef struct gui_icon_info
{
    const char *label;
    const char *sublabel;
    int icon;
    uint8_t color[4];
} gui_icon_info_t;

bool gui_icons_grid(int nb, const gui_icon_info_t *icons, int *current);

bool gui_tab(const char *label, int icon, bool *v);
bool gui_checkbox(const char *label, bool *v, const char *hint);
bool gui_checkbox_flag(const char *label, int *v, int flag, const char *hint);
bool gui_input_int(const char *label, int *v, int minv, int maxv);
bool gui_input_float(const char *label, float *v, float step,
                     float minv, float maxv, const char *format);
bool gui_angle(const char *id, float *v, int vmin, int vmax);
bool gui_bbox(float box[4][4]);
bool gui_rotation_mat4(float m[4][4]);
bool gui_rotation_mat4_axis(float m[4][4]);
bool gui_action_button(int id, const char *label, float size);
bool gui_selectable(const char *name, bool *v, const char *tooltip, float w);
bool gui_selectable_toggle(const char *name, int *v, int set_v,
                           const char *tooltip, float w);
bool gui_selectable_icon(const char *name, bool *v, int icon);
bool gui_color(const char *label, uint8_t color[4]);
bool gui_color_small(const char *label, uint8_t color[4]);
bool gui_color_small_f3(const char *label, float color[3]);
bool gui_input_text(const char *label, char *buf, int size);
bool gui_input_text_multiline(const char *label, char *buf, int size,
                              float width, float height);
void gui_input_text_multiline_highlight(int line);

bool gui_combo(const char *label, int *v, const char **names, int nb);
bool gui_combo_begin(const char *label, const char *preview);
bool gui_combo_item(const char *label, bool selected);
void gui_combo_end(void);

void gui_enabled_begin(bool enabled);
void gui_enabled_end(void);
void gui_dummy(int w, int h);
void gui_spacing(int w);

void gui_alert(const char *title, const char *msg);

void gui_columns(int count);
void gui_next_column(void);
void gui_separator(void);

void gui_push_id(const char *id);
void gui_pop_id(void);

bool gui_layer_item(int idx, int icons_count, const int *icons,
                    bool *visible, bool *edit,
                    char *name, int len);

bool gui_is_key_down(int key);

void gui_query_quit(void);

enum {
    GUI_POPUP_FULL      = 1 << 0,
    GUI_POPUP_RESIZE    = 1 << 1,
};


/*
 * Function: gui_open_popup
 * Open a modal popup.
 *
 * Parameters:
 *    title - The title of the popup.
 *    flags - Union of <GUI_POPUP_FLAGS> values.
 *    data  - Data passed to the popup.  It will be automatically released
 *            by the gui.
 *    func  - The popup function, that render the popup gui.  Should return
 *            a non zero value to close the popup.
 */
void gui_open_popup(const char *title, int flags, void *data,
                    int (*func)(void *data));
void gui_on_popup_closed(void (*func)(int v));
void gui_popup_bottom_begin(void);
void gui_popup_bottom_end(void);

bool gui_menu_bar_begin(void);
void gui_menu_bar_end(void);
bool gui_menu_begin(const char *label, bool enabled);
void gui_menu_end(void);
bool gui_menu_item(int action, const char *label, bool enabled);

void gui_tooltip(const char *str);

bool gui_view_cube(float x, float y, float w, float h);

#endif // GUI_H
