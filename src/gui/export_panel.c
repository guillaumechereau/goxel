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

typedef struct {
    const char *name;
    const char *action;
    void (*gui)(void);
} format_t;

static void png_gui(void) {
    int maxsize, i;
    float view_rect[4];

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 1, maxsize))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 1, maxsize))
        goxel.image->export_height = clamp(i, 1, maxsize);
    if (gui_button("Fit screen", 1, 0)) {
        gui_get_view_rect(view_rect);
        goxel.image->export_width = view_rect[2];
        goxel.image->export_height = view_rect[3];
    }
    gui_group_end();
}

/* XXX: hardcoded list for the moment, but would be better to get it
 * automatically from the actions.  The problem is that it has to be sorted
 * somehow. */
static const format_t FORMATS[] = {
    {"Wavefront (.obj)", "export_as_obj"},
    {"Stanford (.pny)", "export_as_ply"},
    {"Png", "export_as_png", png_gui},
    {"Magica voxel (.vox)", "export_as_vox"},
    {"Qubicle (.qb)", "export_as_qubicle"},
    {"Slab (.kvx)", "export_as_kvx"},
    {"Spades (.vxl)", "export_as_vxl"},
    {"Png slices (.png)", "export_as_png_slices"},
    {"Plain text (.txt)", "export_as_txt"},

};

void gui_export_panel(void)
{
    static int format = 0;
    const char *names[ARRAY_SIZE(FORMATS)];
    int i;
    for (i = 0; i < ARRAY_SIZE(FORMATS); i++) names[i] = FORMATS[i].name;
    gui_text("Export as");
    gui_combo("Export as", &format, names, ARRAY_SIZE(FORMATS));
    if (FORMATS[format].gui) {
        FORMATS[format].gui();
    }
    if (gui_button("Export", 1, 0)) {
        action_exec2(FORMATS[format].action, "");
    }
}
