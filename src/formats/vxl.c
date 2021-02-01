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
#include "file_format.h"

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

static void swap_color(uint32_t v, uint8_t ret[4])
{
    uint8_t o[4];
    memcpy(o, &v, 4);
    ret[0] = o[2];
    ret[1] = o[1];
    ret[2] = o[0];
    ret[3] = o[3];
}

static int vxl_import(image_t *image, const char *path)
{
    // The algo is based on
    // https://silverspaceship.com/aosmap/aos_file_format.html
    // From Sean Barrett (the same person that wrote the code used in
    // ext_src/stb!).
    int ret = 0, size;
    int w = 512, h = 512, d = 64, x, y, z;
    uint8_t (*cube)[4] = NULL;
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

    if (!path) return -1;

    cube = calloc(w * h * d, sizeof(*cube));
    data = (void*)read_file(path, &size);
    v = data;

    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {

        for (z = 0; z < 64; z++)
            cube[AT(x, y, z)][3] = 255;

        z = 0;
        while (true) {
            number_4byte_chunks = v[0];
            top_color_start = v[1];
            top_color_end = v[2];

            for (i = z; i < top_color_start; i++)
                cube[AT(x, y, i)][3] = 0;

            color = (uint32_t*)(v + 4);
            for (z = top_color_start; z <= top_color_end; z++) {
                CHECK(z >= 0 && z < d);
                swap_color(*color++, cube[AT(x, y, z)]);
            }

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
                swap_color(*color++, cube[AT(x, y, z)]);
        }
    }

    mesh_blit(image->active_layer->mesh, (uint8_t*)cube,
              -w / 2, -h / 2, -d / 2, w, h, d, NULL);
    if (box_is_null(image->box)) {
        bbox_from_extents(image->box, vec3_zero, w / 2, h / 2, d / 2);
    }
    free(cube);
    free(data);
    return ret;
}

static int is_surface(int x, int y, int z, uint8_t map[512][512][64])
{
   if (map[x][y][z]==0) return 0;
   if (x == 0 || x == 511) return 1;
   if (y == 0 || y == 511) return 1;
   if (z == 0 || z == 63) return 1;
   if (x   >   0 && map[x-1][y][z]==0) return 1;
   if (x+1 < 512 && map[x+1][y][z]==0) return 1;
   if (y   >   0 && map[x][y-1][z]==0) return 1;
   if (y+1 < 512 && map[x][y+1][z]==0) return 1;
   if (z   >   0 && map[x][y][z-1]==0) return 1;
   if (z+1 <  64 && map[x][y][z+1]==0) return 1;
   return 0;
}

static void write_color(FILE *f, uint32_t color)
{
    uint8_t c[4];
    memcpy(c, &color, 4);
    fputc(c[2], f);
    fputc(c[1], f);
    fputc(c[0], f);
    fputc(c[3], f);
}

#define MAP_Z  64
void write_map(const char *filename,
               uint8_t map[512][512][64],
               uint32_t color[512][512][64])
{
    int i,j,k;
    FILE *f = fopen(filename, "wb");

    for (j = 0; j < 512; ++j) {
        for (i=0; i < 512; ++i) {
            k = 0;
            while (k < MAP_Z) {
                int z;
                int air_start;
                int top_colors_start;
                int top_colors_end; // exclusive
                int bottom_colors_start;
                int bottom_colors_end; // exclusive
                int top_colors_len;
                int bottom_colors_len;
                int colors;

                // find the air region
                air_start = k;
                while (k < MAP_Z && !map[i][j][k])
                    ++k;

                // find the top region
                top_colors_start = k;
                while (k < MAP_Z && is_surface(i,j,k,map))
                    ++k;
                top_colors_end = k;

                // now skip past the solid voxels
                while (k < MAP_Z && map[i][j][k] && !is_surface(i,j,k,map))
                    ++k;

                // at the end of the solid voxels, we have colored voxels.
                // in the "normal" case they're bottom colors; but it's
                // possible to have air-color-solid-color-solid-color-air,
                // which we encode as air-color-solid-0, 0-color-solid-air

                // so figure out if we have any bottom colors at this point
                bottom_colors_start = k;

                z = k;
                while (z < MAP_Z && is_surface(i,j,z,map))
                    ++z;

                if (z == MAP_Z || 0)
                    ; // in this case, the bottom colors of this span are
                      // empty, because we'l emit as top colors
                else {
                    // otherwise, these are real bottom colors so we can write
                    // them
                    while (is_surface(i,j,k,map))
                        ++k;
                }
                bottom_colors_end = k;

                // now we're ready to write a span
                top_colors_len    = top_colors_end    - top_colors_start;
                bottom_colors_len = bottom_colors_end - bottom_colors_start;

                colors = top_colors_len + bottom_colors_len;

                if (k == MAP_Z)
                    fputc(0,f); // last span
                else
                    fputc(colors+1, f);

                fputc(top_colors_start, f);
                fputc(top_colors_end-1, f);
                fputc(air_start, f);

                for (z=0; z < top_colors_len; ++z)
                    write_color(f, color[i][j][top_colors_start + z]);
                for (z=0; z < bottom_colors_len; ++z)
                    write_color(f, color[i][j][bottom_colors_start + z]);
            }
        }
    }
    fclose(f);
}

static int export_as_vxl(const image_t *image, const char *path)
{
    uint8_t (*map)[512][512][64];
    uint32_t (*color)[512][512][64];
    const mesh_t *mesh = goxel_get_layers_mesh(image);
    mesh_iterator_t iter = {0};
    uint8_t c[4];
    int x, y, z, pos[3];
    assert(path);

    map = calloc(1, sizeof(*map));
    color = calloc(1, sizeof(*color));
    for (z = 0; z < 64; z++)
    for (y = 0; y < 512; y++)
    for (x = 0; x < 512; x++) {
        pos[0] = 256 - x;
        pos[1] = y - 256;
        pos[2] = 31 - z;
        mesh_get_at(mesh, &iter, pos, c);
        if (c[3] <= 127) continue;
        (*map)[x][y][z] = 1;
        memcpy(&((*color)[x][y][z]), c, 4);
    }
    write_map(path, *map, *color);
    free(map);
    free(color);
    return 0;
}

FILE_FORMAT_REGISTER(vxl,
    .name = "vxl",
    .ext = "vxl\0*.vxl\0",
    .import_func = vxl_import,
    .export_func = export_as_vxl,
)
