/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

static void export_as_txt(const char *path)
{
    FILE *out;
    mesh_t *mesh = goxel->layers_mesh;
    int x, y, z;
    uvec4b_t v;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "text\0*.txt\0", NULL, "untitled.txt");
    if (!path) return;

    out = fopen(path, "w");
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "# One line per voxel\n");
    fprintf(out, "# X Y Z RRGGBB\n");

    MESH_ITER_VOXELS(mesh, x, y, z, v) {
        if (v.a < 127) continue;
        fprintf(out, "%d %d %d %2x%2x%2x\n", x, y, z, v.r, v.g, v.b);
    }
    fclose(out);
}

ACTION_REGISTER(export_as_txt,
    .help = "Export the image as a txt file",
    .cfunc = export_as_txt,
    .csig = "vp",
    .file_format = {
        .name = "text",
        .ext = "*.txt\0",
    },
)

