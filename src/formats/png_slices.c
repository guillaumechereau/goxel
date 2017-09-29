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

static void export_as_png_slices(const char *path)
{
    box_t box;
    mesh_t *mesh;
    int x, y, z, w, h, d, pos[3], start_pos[3];
    uint8_t c[4];
    uint8_t *img;
    mesh_iterator_t iter = {0};

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                   "png\0*.png\0", NULL, "untitled.png");
    if (!path) return;
    mesh = goxel->layers_mesh;
    box = goxel->image->box;
    if (box_is_null(box)) box = mesh_get_box(mesh, true);
    w = box.w.x * 2;
    h = box.h.y * 2;
    d = box.d.z * 2;
    vec3i_set(start_pos, box.p.x - box.w.x,
                         box.p.y - box.h.y,
                         box.p.z - box.d.z);
    img = calloc(w * h * d, 4);
    for (z = 0; z < d; z++)
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        pos[0] = x + start_pos[0];
        pos[1] = y + start_pos[1];
        pos[2] = z + start_pos[2];
        mesh_get_at(mesh, pos, &iter, c);
        img[(y * w * d + z * w + x) * 4 + 0] = c[0];
        img[(y * w * d + z * w + x) * 4 + 1] = c[1];
        img[(y * w * d + z * w + x) * 4 + 2] = c[2];
        img[(y * w * d + z * w + x) * 4 + 3] = c[3];
    }
    img_write(img, w * d, h, 4, path);

    free(img);
}

ACTION_REGISTER(export_as_png_slices,
    .help = "Export the image as a png slices file",
    .cfunc = export_as_png_slices,
    .csig = "vp",
    .file_format = {
        .name = "png slices",
        .ext = "*.png\0",
    },
)
