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

// Load qubicle files.

#define READ(type, file) ({ type v; fread(&v, sizeof(v), 1, file); v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

void qubicle_import(const char *path)
{
    FILE *file;
    int version, color_format, orientation, compression, vmask, mat_count;
    int i, j, index, len, w, h, d, pos[3], x, y, z;
    char buff[256];
    uint32_t v;
    uvec4b_t *cube;
    const uint32_t CODEFLAG = 2;
    const uint32_t NEXTSLICEFLAG = 6;

    file = fopen(path, "r");
    version = READ(uint32_t, file);
    (void)version;
    color_format = READ(uint32_t, file);
    (void)color_format;
    orientation = READ(uint32_t, file);
    (void)orientation;
    compression = READ(uint32_t, file);
    (void)compression;
    vmask = READ(uint32_t, file);
    (void)vmask;
    mat_count = READ(uint32_t, file);

    for (i = 0; i < mat_count; i++) {
        len = READ(uint8_t, file);
        fread(buff, len, 1, file);
        w = READ(uint32_t, file);
        h = READ(uint32_t, file);
        d = READ(uint32_t, file);
        pos[0] = READ(int32_t, file);
        pos[1] = READ(int32_t, file);
        pos[2] = READ(int32_t, file);
        (void)pos;
        cube = calloc(w * h * d, sizeof(*cube));
        if (compression == 0) {
            for (index = 0; index < w * h * d; index++) {
                v = READ(uint32_t, file);
                cube[index].uint32 = v;
            }
        } else {
            for (z = 0; z < d; z++) {
                index = 0;
                while (true) {
                    v = READ(uint32_t, file);
                    if (v == NEXTSLICEFLAG) {
                        break; // Next z.
                    }
                    len = 1;
                    if (v == CODEFLAG) {
                        len = READ(uint32_t, file);
                        v = READ(uint32_t, file);
                    }
                    for (j = 0; j < len; j++) {
                        x = index % w;
                        y = index / w;
                        cube[x + y * w + z * w * h].uint32 = v;
                        index++;
                    }
                }
            }
        }
        mesh_blit(goxel()->image->active_layer->mesh, cube,
                  pos[0], pos[1], pos[2], w, h, d);
        free(cube);
    }
}

void qubicle_export(const mesh_t *mesh, const char *path)
{
    FILE *file;
    block_t *block;
    int i, count, x, y, z;
    char buff[8];

    count = HASH_COUNT(mesh->blocks);

    file = fopen(path, "wb");
    WRITE(uint32_t, 257, file); // version
    WRITE(uint32_t, 0, file);   // color format RGBA
    WRITE(uint32_t, 1, file);   // orientation right handed
    WRITE(uint32_t, 0, file);   // no compression
    WRITE(uint32_t, 0, file);   // vmask
    WRITE(uint32_t, count, file);

    i = 0;
    MESH_ITER_BLOCKS(mesh, block) {

        WRITE(uint8_t, 8, file);
        memset(buff, 0, 8);
        sprintf(buff, "%d", i);
        fwrite(buff, 8, 1, file);

        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(int32_t, block->pos.x - BLOCK_SIZE / 2, file);
        WRITE(int32_t, block->pos.y - BLOCK_SIZE / 2, file);
        WRITE(int32_t, block->pos.z - BLOCK_SIZE / 2, file);
        for (z = 1; z < BLOCK_SIZE - 1; z++)
        for (y = 1; y < BLOCK_SIZE - 1; y++)
        for (x = 1; x < BLOCK_SIZE - 1; x++) {
            WRITE(uint32_t, block->data->voxels[
                  x + y * BLOCK_SIZE + z * BLOCK_SIZE * BLOCK_SIZE].uint32,
                  file);
        }
        i++;
    }
    fclose(file);
}
