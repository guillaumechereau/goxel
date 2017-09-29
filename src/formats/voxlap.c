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

#include "goxel.h"

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})

#define raise(msg) do { \
        LOG_E(msg); \
        ret = -1; \
        goto end; \
    } while (0)

static inline int AT(int x, int y, int z, int w, int h, int d) {
    y = h - y - 1;
    z = d - z - 1;
    return x + y * w + z * w * h;
}

static uvec4b_t swap_color(uint32_t v)
{
    uvec4b_t ret;
    ret.uint32 = v;
    SWAP(ret.r, ret.b);
    return ret;
}

static int kv6_import(const char *path)
{
    FILE *file;
    char magic[4];
    int i, r, ret = 0, w, h, d, blklen, x, y, z = 0, nb, p = 0;
    uint32_t *xoffsets = NULL;
    uint16_t *xyoffsets = NULL;
    uvec4b_t *cube = NULL, color = uvec4b_zero;
    (void)r;
    struct {
        uint32_t color;
        uint8_t zpos;
        uint8_t visface;
    } *blocks = NULL;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                        "kv6\0*.kv6\0", NULL, NULL);
    if (!path) return -1;

    file = fopen(path, "rb");
    r = fread(magic, 1, 4, file);
    if (strncmp(magic, "Kvxl ", 4) != 0) raise("Invalid magic");
    w = READ(uint32_t, file);
    h = READ(uint32_t, file);
    d = READ(uint32_t, file);
    cube = calloc(w * h * d, sizeof(*cube));

    READ(float, file);
    READ(float, file);
    READ(float, file);
    blklen = READ(uint32_t, file);
    blocks = calloc(blklen, sizeof(*blocks));
    for (i = 0; i < blklen; i++) {
        blocks[i].color = READ(uint32_t, file);
        blocks[i].zpos = READ(uint16_t, file);
        blocks[i].visface = READ(uint8_t, file);
        READ(uint8_t, file); // lighting
    }
    xoffsets = calloc(w, sizeof(*xoffsets));
    xyoffsets = calloc(w * h, sizeof(*xyoffsets));
    for (i = 0; i < w; i++)      xoffsets[i] = READ(uint32_t, file);
    for (i = 0; i < w * h; i++) xyoffsets[i] = READ(uint16_t, file);

    for (x = 0; x < w; x++)
    for (y = 0; y < h; y++) {
        nb = xyoffsets[x * h + y];
        for (i = 0; i < nb; i++, p++) {
            z = blocks[p].zpos;
            cube[AT(x, y, z, w, h, d)] = swap_color(blocks[p].color);
        }
    }

    // Fill
    p = 0;
    for (x = 0; x < w; x++)
    for (y = 0; y < h; y++) {
        nb = xyoffsets[x * h + y];
        for (i = 0; i < nb; i++, p++) {
            if (blocks[p].visface & 0x10) {
                z = blocks[p].zpos;
                color = swap_color(blocks[p].color);
                color.a = 255;
            }
            if (blocks[p].visface & 0x20) {
                for (; z < blocks[p].zpos; z++)
                    if (cube[AT(x, y, z, w, h, d)].a == 0)
                        cube[AT(x, y, z, w, h, d)] = color;
            }
        }
    }

    mesh_blit(goxel->image->active_layer->mesh, (const uint8_t*)cube,
              -w / 2, -h / 2, -d / 2, w, h, d, NULL);
    goxel_update_meshes(goxel, -1);
end:
    free(cube);
    free(blocks);
    free(xoffsets);
    free(xyoffsets);
    fclose(file);
    return ret;
}

static int kvx_import(const char *path)
{
    FILE *file;
    int i, r, ret = 0, nb, w, h, d, x, y, z, lastz = 0, len, visface;
    uint8_t color = 0;
    uvec4b_t *palette = NULL;
    uint32_t *xoffsets = NULL;
    uint16_t *xyoffsets = NULL;
    uvec4b_t *cube = NULL;
    long datpos;
    (void)r;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                        "kvx\0*.kvx\0", NULL, NULL);
    if (!path) return -1;

    file = fopen(path, "rb");
    nb = READ(uint32_t, file); (void)nb;
    w = READ(uint32_t, file);
    h = READ(uint32_t, file);
    d = READ(uint32_t, file);
    cube = calloc(w * h * d, sizeof(*cube));

    READ(uint32_t, file);
    READ(uint32_t, file);
    READ(uint32_t, file);

    xoffsets = calloc(w + 1, sizeof(*xoffsets));
    xyoffsets = calloc(w * (h + 1), sizeof(*xyoffsets));
    for (i = 0; i < w + 1; i++)        xoffsets[i] = READ(uint32_t, file);
    for (i = 0; i < w * (h + 1); i++) xyoffsets[i] = READ(uint16_t, file);

    datpos = ftell(file);

    // Read the palette at the end of the file first.
    fseek(file, -256 * 3, SEEK_END);
    palette = calloc(256, sizeof(*palette));
    for (i = 0; i < 256; i++) {
        palette[i].r = clamp(round(READ(uint8_t, file) * 255 / 63.f), 0, 255);
        palette[i].g = clamp(round(READ(uint8_t, file) * 255 / 63.f), 0, 255);
        palette[i].b = clamp(round(READ(uint8_t, file) * 255 / 63.f), 0, 255);
        palette[i].a = 255;
    }
    fseek(file, datpos, SEEK_SET);

    for (x = 0; x < w; x++)
    for (y = 0; y < h; y++) {
        if (xyoffsets[x * (h + 1) + y + 1] < xyoffsets[x * (h + 1) + y])
            raise("Invalid format");
        nb = xyoffsets[x * (h + 1) + y + 1] - xyoffsets[x * (h + 1) + y];
        while (nb > 0) {
            z = READ(uint8_t, file);
            len = READ(uint8_t, file);
            visface = READ(uint8_t, file);
            assert(z + len - 1  < d);
            for (i = 0; i < len; i++) {
                color = READ(uint8_t, file);
                cube[AT(x, y, z + i, w, h, d)] = palette[color];
            }
            nb -= len + 3;
            // Fill
            if (visface & 0x10) lastz = z + len;
            if (visface & 0x20) {
                for (i = lastz; i < z; i++)
                    if (cube[AT(x, y, i, w, h, d)].a == 0)
                        cube[AT(x, y, i, w, h, d)] = palette[color];
            }
        }
    }

    mesh_blit(goxel->image->active_layer->mesh, (uint8_t*)cube,
              -w / 2, -h / 2, -d / 2, w, h, d, NULL);
    goxel_update_meshes(goxel, -1);

end:
    free(palette);
    free(cube);
    free(xoffsets);
    free(xyoffsets);
    fclose(file);
    return ret;
}

ACTION_REGISTER(import_kv6,
    .help = "Import a slab kv6 image",
    .cfunc = kv6_import,
    .csig = "vp",
    .file_format = {
        .name = "kv6",
        .ext = "*.kv6\0"
    },
)

ACTION_REGISTER(import_kvx,
    .help = "Import a slab kvx image",
    .cfunc = kvx_import,
    .csig = "vp",
    .file_format = {
        .name = "kvx",
        .ext = "*.kvx\0"
    },
)
