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

#ifndef GUI_CUSTOM_EXPORT_PANEL

#if 0

Keep this here as a reference until I fix file format names and order.

    {"glTF (.gltf)", "export_as_gltf"},
    {"Wavefront (.obj)", "export_as_obj"},
    {"Stanford (.pny)", "export_as_ply"},
    {"Png", "export_as_png"},
    {"Magica voxel (.vox)", "export_as_vox"},
    {"Qubicle (.qb)", "export_as_qubicle"},
    {"Slab (.kvx)", "export_as_kvx"},
    {"Spades (.vxl)", "export_as_vxl"},
    {"Png slices (.png)", "export_as_png_slices"},
    {"Plain text (.txt)", "export_as_txt"},

#endif

static const file_format_t *g_current = NULL;

static const char *make_label(const file_format_t *f, char *buf, int len)
{
    const char *ext = f->ext + strlen(f->ext) + 2;
    snprintf(buf, len, "%s (%s)", f->name, ext);
    return buf;
}

static void on_format(void *user, const file_format_t *f)
{
    char label[128];
    make_label(f, label, sizeof(label));
    if (gui_combo_item(label, f == g_current)) {
        g_current = f;
    }
}

void gui_export_panel(void)
{
    char label[128];
    gui_text("Export as");
    if (!g_current) g_current = file_formats; // First one.

    make_label(g_current, label, sizeof(label));
    if (gui_combo_begin("Export as", label)) {
        file_format_iter("w", NULL, on_format);
        gui_combo_end();
    }

    if (g_current->export_gui)
        g_current->export_gui();
    if (gui_button("Export", 1, 0))
        goxel_export_to_file(NULL, g_current->name);
}

#endif // GUI_CUSTOM_EXPORT_PANEL
