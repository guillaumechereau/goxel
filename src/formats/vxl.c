/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

// Support for Ace of Spades map files (vxl)

#include "goxel.h"

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})

#define raise(msg) do { \
        LOG_E(msg); \
        ret = -1; \
        goto end; \
    } while (0)

static inline int AT(int x, int y, int z) {
    x = 511 - x;
    z = 63 - z;
    return x + y * 512 + z * 512 * 512;
}

static uvec4b_t swap_color(uint32_t v)
{
    uvec4b_t ret;
    ret.uint32 = v;
    SWAP(ret.r, ret.b);
    ret.a = 255;
    return ret;
}

static int vxl_import(const char *path)
{
    // The algo is based on
    // https://silverspaceship.com/aosmap/aos_file_format.html
    // From Sean Barrett (the same person that wrote the code used in
    // ext_src/stb!).
    int ret = 0, size;
    int w = 512, h = 512, d = 64, x, y, z;
    uvec4b_t *cube = NULL;
    uint8_t *data, *v;

    uint32_t *color;
    int i;
    int number_4byte_chunks;
    int top_color_start;
    int top_color_end;
    int bottom_color_start;
    int bottom_color_end; // exclusive
    int len_top;
    int len_bottom;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                        "vxl\0*.vxl\0", NULL, NULL);
    if (!path) return -1;

    cube = calloc(w * h * d, sizeof(*cube));
    data = (void*)read_file(path, &size);
    v = data;

    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {

        for (z = 0; z < 64; z++)
            cube[AT(x, y, z)].a = 255;

        z = 0;
        while (true) {
            number_4byte_chunks = v[0];
            top_color_start = v[1];
            top_color_end = v[2];

            for (i = z; i < top_color_start; i++)
                cube[AT(x, y, i)].a = 0;

            color = (uint32_t*)(v + 4);
            for (z = top_color_start; z <= top_color_end; z++)
                cube[AT(x, y, z)] = swap_color(*color++);

            len_bottom = top_color_end - top_color_start + 1;

            // check for end of data marker
            if (number_4byte_chunks == 0) {
                // infer ACTUAL number of 4-byte chunks from the length of the
                // color data
                v += 4 * (len_bottom + 1);
                break;
            }

            // infer the number of bottom colors in next span from chunk length
            len_top = (number_4byte_chunks-1) - len_bottom;

            // now skip the v pointer past the data to the beginning of the
            // next span
            v += v[0] * 4;

            bottom_color_end   = v[3]; // aka air start
            bottom_color_start = bottom_color_end - len_top;

            for(z = bottom_color_start; z < bottom_color_end; z++)
                cube[AT(x, y, z)] = swap_color(*color++);
        }
    }

    mesh_blit(goxel->image->active_layer->mesh, cube,
              -w / 2, -h / 2, -d / 2, w, h, d);
    goxel_update_meshes(goxel, -1);
    if (box_is_null(goxel->image->box))
        goxel->image->box = bbox_from_extents(vec3_zero, w / 2, h / 2, d / 2);
    free(cube);
    free(data);
    return ret;
}

ACTION_REGISTER(import_vxl,
    .help = "Import a Ace of Spades map file",
    .cfunc = vxl_import,
    .csig = "vp",
    .file_format = {
        .name = "vxl",
        .ext = "*.vxl\0"
    },
)
