/*
 * Goxel 3D voxels editor
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

/*
 * Qubicle project format import support.
 * Based on the reverse engineering done by Tostc:
 * https://gist.github.com/tostc/7f049207a2e5a7ccb714499702b5e2fd
 */

#include "goxel.h"
#include "file_format.h"

// For the zlib decompression.
#include "stb_image.h"

#define raise(msg, ...) do { \
        LOG_E(msg, ##__VA_ARGS__); \
        goto error; \
    } while (0)

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})

// Todo: add errors check.
#define SKIP(file, size) fseek(file, size, SEEK_CUR)

static int import_matrix(FILE *file);
static int import_model(FILE *file);

static int import_node(FILE *file)
{
    int type, name_size, r;
    layer_t *layer;

    layer = image_add_layer(goxel.image, NULL);
    type = READ(int32_t, file);
    SKIP(file, 4);
    name_size = READ(uint32_t, file);
    if (name_size >= sizeof(layer->name)) raise("Node size too long");
    memset(layer->name, 0, sizeof(layer->name));
    r = fread(layer->name, name_size, 1, file);
    if (r != 1) raise("Read file error");
    SKIP(file, 3);

    switch (type) {
    case 0:
        r = import_matrix(file);
        if (r != 0) return r;
        break;
    case 1:
        r = import_model(file);
        if (r != 0) return r;
        break;
    default:
        raise("Unknown node type: %d", type);
        break;
    }
    return 0;

error:
    return -1;
}

static int import_model(FILE *file)
{
    int i, child_count, r;

    SKIP(file, 36);
    child_count = READ(uint32_t, file);
    for (i = 0; i < child_count; i++) {
        r = import_node(file);
        if (r != 0) return r;
    }
    return 0;
}

static int AT(int x, int y, int z, int w, int h, int d)
{
    // Qubicle uses Y up, we use Z up.
    return y * (w * d) + z * w + (w - x - 1);
}

static int import_matrix_data(const char *data, int data_size,
                              uint8_t (*cube)[4], const int w, int h, int d)
{
    uint16_t size;
    int i, j, x, y, z, index = 0;
    uint8_t cmd[4], color[4];
    const char *end = data + data_size;

    while (data < end) {
        y = 0;
        memcpy(&size, data, 2);
        data += 2;
        for (i = 0;  i < size; i++) {
            memcpy(cmd, data, 4);
            data += 4;
            if (cmd[3] == 2) { // RLE
                memcpy(color, data, 4);
                data += 4;
                if (color[3]) color[3] = 255;
                for (j = 0; j < cmd[0]; j++) {
                    x = index / d;
                    z = index % d;
                    memcpy(&cube[AT(x, y, z, w, h, d)], color, 4);
                    y++;
                }
                i++;
            } else if (cmd[3] == 0) {
                y++;
            } else {
                if (cmd[3]) cmd[3] = 255;
                x = index / d;
                z = index % d;
                memcpy(&cube[AT(x, y, z, w, h, d)], cmd, 4);
                y++;
            }
        }
        index++;
    }

    return 0;
}

static int import_matrix(FILE *file)
{
    int size[3], pos[3];
    float pivot[3];
    int r, comp_data_size, data_size;
    char *comp_data, *data;
    uint8_t (*cube)[4];

    size[0] = READ(int32_t, file);
    size[1] = READ(int32_t, file);
    size[2] = READ(int32_t, file);
    pos[0] = READ(int32_t, file);
    pos[1] = READ(int32_t, file);
    pos[2] = READ(int32_t, file);
    pivot[0] = READ(float, file);
    pivot[1] = READ(float, file);
    pivot[2] = READ(float, file);
    (void)pivot;
    comp_data_size = READ(uint32_t, file);

    comp_data = malloc(comp_data_size);
    r = fread(comp_data, comp_data_size, 1, file);
    if (r != 1) return -1;

    data = stbi_zlib_decode_malloc(comp_data, comp_data_size, &data_size);

    cube = calloc(size[0] * size[1] * size[2], sizeof(*cube));
    import_matrix_data(data, data_size, cube, size[0], size[1], size[2]);

    mesh_blit(goxel.image->active_layer->mesh, (uint8_t*)cube,
              pos[0], pos[2], pos[1],
              size[0], size[2], size[1], NULL);

    free(cube);

    free(comp_data);
    free(data);

    return 0;
}

static int qubicle2_import(image_t *image, const char *path)
{
    FILE *file;
    char magic[4];
    uint32_t prog_version;
    uint32_t file_version;
    int r, i, w, h, size;

    file = fopen(path, "rb");
    r = fread(magic, 1, 4, file);
    if (r != 1 || strncmp(magic, "QBCL ", 4) != 0) raise("Invalid magic");
    prog_version = READ(uint32_t, file);
    file_version = READ(uint32_t, file);
    LOG_I("Qubicle prog version: %d, file version: %d",
          prog_version, file_version);

    // Skip the thumbnail.
    w = READ(uint32_t, file);
    h = READ(uint32_t, file);
    SKIP(file, w * h * 4);

    // Skip the meta data.
    for (i = 0; i < 7; i++) {
        size = READ(uint32_t, file);
        SKIP(file, size);
    }
    SKIP(file, 16);
    if (import_node(file)) raise("Cannot load file");

    fclose(file);
    return 0;

error:
    fclose(file);
    return -1;
}

FILE_FORMAT_REGISTER(qubicle,
    .name = "qubicle2",
    .ext = "qubicle2\0*.qbcl\0",
    .import_func = qubicle2_import,
)
