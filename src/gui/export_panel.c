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
} format_t;

/* XXX: hardcoded list for the moment, but would be better to get it
 * automatically from the actions.  The problem is that it has to be sorted
 * somehow. */
static const format_t FORMATS[] = {
    {"Wavefront (.obj)", "export_as_obj"},
    {"Stanford (.pny)", "export_as_ply"},
    {"Png", "export_as_png"},
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
    action_t *action;

    for (i = 0; i < ARRAY_SIZE(FORMATS); i++) names[i] = FORMATS[i].name;
    gui_text("Export as");
    gui_combo("Export as", &format, names, ARRAY_SIZE(FORMATS));
    action = action_get(FORMATS[format].action, true);

    if (action->file_format.export_gui)
        action->file_format.export_gui();
    if (gui_button("Export", 1, 0))
        action_exec(action, "");
}
