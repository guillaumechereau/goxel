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
    mesh_t *mesh = goxel.layers_mesh;
    int p[3];
    uint8_t v[4];
    mesh_iterator_t iter;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "text\0*.txt\0", NULL, "untitled.txt");
    if (!path) return;

    out = fopen(path, "w");
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "# One line per voxel\n");
    fprintf(out, "# X Y Z RRGGBB\n");

    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, p)) {
        mesh_get_at(mesh, &iter, p, v);
        if (v[3] < 127) continue;
        fprintf(out, "%d %d %d %02x%02x%02x\n",
                p[0], p[1], p[2], v[0], v[1], v[2]);
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

