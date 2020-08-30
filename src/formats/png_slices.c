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
#include "file_format.h"

static int export_as_png_slices(const image_t *image, const char *path)
{
    float box[4][4];
    const mesh_t *mesh;
    int x, y, z, w, h, d, pos[3], start_pos[3];
    uint8_t c[4];
    uint8_t *img;
    mesh_iterator_t iter = {0};

    mesh = goxel_get_layers_mesh(image);
    mat4_copy(image->box, box);
    if (box_is_null(box)) mesh_get_box(mesh, true, box);
    w = box[0][0] * 2;
    h = box[1][1] * 2;
    d = box[2][2] * 2;
    start_pos[0] = box[3][0] - box[0][0];
    start_pos[1] = box[3][1] - box[1][1];
    start_pos[2] = box[3][2] - box[2][2];
    img = calloc(w * h * d, 4);
    for (z = 0; z < d; z++)
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        pos[0] = x + start_pos[0];
        pos[1] = y + start_pos[1];
        pos[2] = z + start_pos[2];
        mesh_get_at(mesh, &iter, pos, c);
        img[(y * w * d + z * w + x) * 4 + 0] = c[0];
        img[(y * w * d + z * w + x) * 4 + 1] = c[1];
        img[(y * w * d + z * w + x) * 4 + 2] = c[2];
        img[(y * w * d + z * w + x) * 4 + 3] = c[3];
    }
    img_write(img, w * d, h, 4, path);
    free(img);
    return 0;
}

FILE_FORMAT_REGISTER(png_slices,
    .name = "png slices",
    .ext = "png\0*.png\0",
    .export_func = export_as_png_slices,
)
