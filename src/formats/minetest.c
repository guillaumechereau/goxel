/* Goxel 3D voxels editor
 *
 * copyright (c) 2022 Guillaume Chereau <guillaume@noctua-software.com>
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

// For the zlib decompression.
#include "stb_image.h"

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})

#define raise(msg) do { \
        LOG_E(msg); \
        goto error; \
    } while (0)

static int read_uint16(FILE *file)
{
    uint8_t data[2];
    int r;
    r = fread(data, 2, 1, file);
    if (r != 1) return 0; // XXX: need to properly check for errors.
    return (data[0] << 8) | data[1];
}

static int get_file_remaining_size(FILE *file)
{
    long cur, end;
    cur = ftell(file);
    fseek(file, 0, SEEK_END);
    end = ftell(file);
    fseek(file, cur, SEEK_SET);
    return end - cur;
}

static void get_color(const char *name, uint8_t out[4],
                      const palette_t *minetest_palette)
{
    int i;

    // First search using the current palette.
    for (i = 0; i < goxel.palette->size; i++) {
        if (strcasecmp(goxel.palette->entries[i].name, name) == 0) {
            memcpy(out, goxel.palette->entries[i].color, 4);
            return;
        }
    }

    // Try minetest palette next.
    for (i = 0; i < minetest_palette->size; i++) {
        if (strcasecmp(minetest_palette->entries[i].name, name) == 0) {
            memcpy(out, minetest_palette->entries[i].color, 4);
            return;
        }
    }

    // Fallback to white.
    LOG_I("Cannot fine color for '%s'", name);
    out[0] = 255;
    out[1] = 255;
    out[2] = 255;
    out[3] = 255;
}

static int mts_import(const file_format_t *format, image_t *image,
                      const char *path)
{
    FILE *file;
    char magic[4];
    int r, version, w, h, d, x, y, z, n_strings, len, i, size, c, pos[3];
    uint8_t color[4], (*palette)[4];
    char string[512], *buf, *data;
    const uint8_t *ptr;
    layer_t *layer;
    volume_iterator_t iter = {0};
    const palette_t *minetest_palette = NULL;

    file = fopen(path, "rb");
    r = fread(magic, 1, 4, file);
    if (r != 4 || strncmp(magic, "MTSM ", 4) != 0) raise("Invalid magic");

    version = read_uint16(file);
    w = read_uint16(file);
    h = read_uint16(file);
    d = read_uint16(file);
    LOG_I("Minetest file version %d, size = %dx%dx%d", version, w, h, d);

    // Skip the layer probability values.
    fseek(file, h, SEEK_CUR);
    n_strings = read_uint16(file);
    LOG_D("n_strings = %d", n_strings);

    // Generate the palette to use for the indices, based on the Minetest
    // palette.
    DL_FOREACH(goxel.palettes, minetest_palette) {
        if (strcmp(minetest_palette->name, "Minetest") == 0)
            break;
    }
    // assert(minetest_palette);
    palette = calloc(n_strings, sizeof(*palette));
    for (i = 0; i < n_strings; i++) {
        len = read_uint16(file);
        if (len >= sizeof(string)) raise("String name too long");
        r = fread(string, len, 1, file);
        if (r != 1) raise("Error reading file");
        string[len] = '\0';
        if (strcasecmp(string, "air") == 0) continue;
        get_color(string, palette[i], minetest_palette);
    }

    // Uncompress the data.
    size = get_file_remaining_size(file);
    buf = malloc(size);
    r = fread(buf, size, 1, file);
    if (r != 1) raise("Error reading file");
    data = stbi_zlib_decode_malloc(buf, size, &size);
    free(buf);

    layer = image_add_layer(image, NULL);

    for (z = 0; z < d; z++)
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        pos[0] = x;
        pos[1] = z;
        pos[2] = y;

        ptr = (const uint8_t*)(data + (z * w * h + y * w + x) * 2);
        c = ((int)ptr[0] << 8) | (int)ptr[1];

        if (c >= n_strings) continue;
        memcpy(color, palette[c], 4);
        volume_set_at(layer->volume, &iter, pos, color);
    }

    free(data);
    free(palette);

    return 0;

error:
    fclose(file);
    return -1;
}

FILE_FORMAT_REGISTER(kv6,
    .name = "Minetest",
    .ext = "mts\0*.mts\0",
    .import_func = mts_import,
)
