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

#define CHECK(x) do { \
        if (!(x)) { \
            ret = -1; \
            goto end; \
        } \
    } while(0)


static inline int AT(int x, int y, int z, int d) {
    x = 511 - x;
    z = d - 1 - z;
    return x + y * 512 + z * 512 * 512;
}

static int vxl_import(const file_format_t *format, image_t *image,
                      const char *path)
{
    // See https://silverspaceship.com/aosmap/aos_file_format.html
    // for a description of the AOS file format. Note that this import
    // code is different from the one given on that page, however we use
    // some of the same variable names. The importer on silverspaceship.com
    // contains a bug somewhere, this was known back when AOS was at its
    // peak in 2012.

    int width = 512, height = 64, depth = 512;

    int ret = 0;

    // The variable i here points to the location in the data array we
    // are reading from
    int i = 0;
    int x = 0; // Cursor location x
    int y = 0; // Cursor location y
    int columnI = 0; // Which vertical column we are currently on
    int columnCount = width * depth; // The total number of columns in the map

    int size;
    uint8_t *data = (uint8_t*)read_file(path, &size);

    uint8_t (*cube)[4] = NULL;
    cube = calloc(width * height * depth, sizeof(*cube));

    // The general strategy for this loader is to consume data from the input
    // binary until we've processed all columns in the map.
    // We will move the cursor (x, y, zz) as we read the voxel data. The cursor
    // indicates the current location we are modifying
    while (columnI < columnCount) {
        // i = span start byte
        int N = data[i]; // length of span data (N * 4 bytes including span header)
        int S = data[i + 1]; // Starting height of top colored run
        int E = data[i + 2]; // Ending height of top colored run
        int K = E - S + 1;
        int M, Z, zz, runLength;

        if (N == 0) {
            Z = 0;
            M = 64;
        } else {
            Z = (N - 1) - K;
            // A of the next span
            M = data[i + N * 4 + 3];
        }

        int colorI = 0;
        // Execute the following loop twice:
        // Once for the top run of colors, the second for the bottom run of colors
        for (int p = 0; p < 2; p++) {
            // Get top run of colors
            if (p == 0) {
                zz = S;
                runLength = K;
            } else {
                // Get bottom run of colors
                zz = M - Z;
                runLength = Z;
            }

            for (int j = 0; j < runLength; j++) {
                uint8_t blue = data[i + 4 + colorI * 4];
                uint8_t green = data[i + 5 + colorI * 4];
                uint8_t red = data[i + 6 + colorI * 4];

                int idx = AT(x, y, zz, height);
                cube[idx][0] = red;
                cube[idx][1] = green;
                cube[idx][2] = blue;
                cube[idx][3] = 255;

                zz++;
                colorI++;
            }
        }

        // Now deal with solid non-surface voxels
        // No color data is provided for non-surface voxels
        zz = E + 1;
        runLength = M - Z - zz;
        for (int j = 0; j < runLength; j++) {
            // All other channels should already be 0 due to calloc
            // Set to brown color. In AOS non-surface blocks became
            // brown when exposed to the air
            int idx = AT(x, y, zz, height);
            cube[idx][0] = 91;
            cube[idx][1] = 64;
            cube[idx][2] = 64;
            cube[idx][3] = 255;
            zz++;
        }

        if (N == 0) {
            // We're done with this column of data, move the cursor
            // to the next column and increment our column counter
            columnI++;
            x++;
            if (x >= width) {
                x = 0;
                y++;
            }

            i += 4 * (1 + K);
        } else {
            i += N * 4;
        }
    }

    volume_blit(image->active_layer->volume, (uint8_t*)cube,
          -width / 2, -depth / 2, -height / 2, width, depth, height, NULL);
    if (box_is_null(image->box)) {
        bbox_from_extents(image->box, vec3_zero, width / 2, depth / 2, height / 2);
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

static int export_as_vxl(const file_format_t *format, const image_t *image,
                         const char *path)
{
    uint8_t (*map)[512][512][64];
    uint32_t (*color)[512][512][64];
    const volume_t *volume = goxel_get_layers_volume(image);
    volume_iterator_t iter = {0};
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
        volume_get_at(volume, &iter, pos, c);
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
    .exts = {"*.vxl"},
    .exts_desc = "vxl",
    .import_func = vxl_import,
    .export_func = export_as_vxl,
)
