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
    }
    mesh_blit(goxel()->image->active_layer->mesh, cube,
              -w / 2, -h / 2, -d / 2, w, h, d);
    free(cube);
}
