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
#   define GUI_PANEL_WIDTH_NORMAL 190
#endif

#ifndef GUI_PANEL_WIDTH_LARGE
#   define GUI_PANEL_WIDTH_LARGE 400
#endif

void gui_release(void);
void gui_release_graphics(void);

void gui_iter(const inputs_t *inputs);
void gui_render(void);

void gui_request_panel_width(float width);

bool gui_panel_header(const char *label);

void gui_canvas(float w, float h,
                inputs_t *inputs, bool *has_mouse, bool *has_keyboard,
                void *user,
                void (*render)(void *user, const float viewport[4]));

// Gui widgets:
bool gui_collapsing_header(const char *label, bool default_opened);
void gui_text(const char *label, ...);
void gui_text_wrapped(const char *label, ...);
bool gui_button(const char *label, float w, int icon);
bool gui_button_right(const char *label, int icon);
void gui_group_begin(const char *label);
void gui_group_end(void);

void gui_div_begin(void);
void gui_div_end(void);

void gui_child_begin(const char *id, float w, float h);
void gui_child_end(void);

bool gui_tab(const char *label, int icon, bool *v);
bool gui_checkbox(const char *label, bool *v, const char *hint);
bool gui_checkbox_flag(const char *label, int *v, int flag, const char *hint);
bool gui_input_int(const char *label, int *v, int minv, int maxv);
bool gui_input_float(const char *label, float *v, float step,
                     float minv, float maxv, const char *format);
bool gui_angle(const char *id, float *v, int vmin, int vmax);
bool gui_bbox(float box[4][4]);
bool gui_quat(const char *label, float q[4]);
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


float gui_get_avail_width(void);
void gui_same_line(void);
void gui_enabled_begin(bool enabled);
void gui_enabled_end(void);
void gui_spacing(int w);

// Add an icon in top left corner of last item.
void gui_floating_icon(int icon);
// Add a text at the bottom of last item.
void gui_bottom_text(const char *txt);

void gui_alert(const char *title, const char *msg);

void gui_columns(int count);
void gui_next_column(void);
void gui_separator(void);

void gui_push_id(const char *id);
void gui_pop_id(void);

bool gui_layer_item(int i, int icon, bool *visible, bool *edit,
                    char *name, int len);

bool gui_is_key_down(int key);
bool gui_palette_entry(const uint8_t color[4], uint8_t target[4]);

bool gui_need_full_version(void);

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
void gui_popup_body_begin(void);
void gui_popup_body_end(void);

bool gui_menu_begin(const char *label);
void gui_menu_end(void);
bool gui_menu_item(int action, const char *label, bool enabled);

void gui_scrollable_begin(int width);
void gui_scrollable_end(void);

void gui_tooltip(const char *str);

void gui_choice_begin(const char *label, int *value, bool small);
bool gui_choice(const char *label, int idx, int icon);
void gui_choice_end(void);

#endif // GUI_H
