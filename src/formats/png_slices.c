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
    int x, y, z, w, h, d;
    uvec4b_t c;
    uint8_t *img;
    vec3i_t pos, start_pos;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                   "png\0*.png\0", NULL, "untitled.png");
    if (!path) return;
    mesh = goxel->layers_mesh;
    box = goxel->image->box;
    if (box_is_null(box)) box = mesh_get_box(mesh, true);
    w = box.w.x * 2;
    h = box.h.y * 2;
    d = box.d.z * 2;
    start_pos = vec3i(box.p.x - box.w.x, box.p.y - box.h.y, box.p.z - box.d.z);

    img = calloc(w * h * d, 4);
    for (z = 0; z < d; z++)
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        pos = vec3i(x + start_pos.x, y + start_pos.y, z + start_pos.z);
        c = mesh_get_at(mesh, &pos);
        img[(y * w * d + z * w + x) * 4 + 0] = c.r;
        img[(y * w * d + z * w + x) * 4 + 1] = c.g;
        img[(y * w * d + z * w + x) * 4 + 2] = c.b;
        img[(y * w * d + z * w + x) * 4 + 3] = c.a;
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
