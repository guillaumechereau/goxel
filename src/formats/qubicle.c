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

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

static void qubicle_import(const char *path)
{
    FILE *file;
    int version, color_format, orientation, compression, vmask, mat_count;
    int i, j, r, index, len, w, h, d, pos[3], x, y, z;
    char buff[256];
    uvec4b_t v;
    uvec4b_t *cube;
    mat4_t mat = mat4_identity;
    const uint32_t CODEFLAG = 2;
    const uint32_t NEXTSLICEFLAG = 6;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                        NULL, NULL, NULL);
    if (!path) return;

    file = fopen(path, "rb");
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
        r = (int)fread(buff, len, 1, file);
        (void)r;
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
                v.uint32 = READ(uint32_t, file);
                v.a = v.a ? 255 : 0;
                cube[index] = v;
            }
        } else {
            for (z = 0; z < d; z++) {
                index = 0;
                while (true) {
                    v.uint32 = READ(uint32_t, file);
                    if (v.uint32 == NEXTSLICEFLAG) {
                        break; // Next z.
                    }
                    len = 1;
                    if (v.uint32 == CODEFLAG) {
                        len = READ(uint32_t, file);
                        v.uint32 = READ(uint32_t, file);
                        v.a = v.a ? 255 : 0;
                    }
                    for (j = 0; j < len; j++) {
                        x = index % w;
                        y = index / w;
                        v.a = v.a ? 255 : 0;
                        cube[x + y * w + z * w * h] = v;
                        index++;
                    }
                }
            }
        }
        mesh_blit(goxel->image->active_layer->mesh, cube,
                  pos[0], pos[1], pos[2], w, h, d);
        free(cube);
    }

    // Apply a 90 deg X rotation to fix axis.
    mat4_irotate(&mat, M_PI / 2, 1, 0, 0);
    mesh_move(goxel->image->active_layer->mesh, &mat);
    goxel_update_meshes(goxel, -1);
}

void qubicle_export(const mesh_t *mesh, const char *path)
{
    FILE *file;
    block_t *block;
    int i, count, x, y, z, bpos[3];
    char buff[16];
    mesh_t *m = mesh_copy(mesh);
    mat4_t mat = mat4_identity;

    // Apply a -90 deg X rotation to fix axis.
    mat4_irotate(&mat, -M_PI / 2, 1, 0, 0);
    mesh_move(m, &mat);
    mesh = m;

    count = HASH_COUNT(mesh->blocks);

    file = fopen(path, "wb");
    WRITE(uint32_t, 257, file); // version
    WRITE(uint32_t, 0, file);   // color format RGBA
    WRITE(uint32_t, 0, file);   // orientation left handed
    WRITE(uint32_t, 0, file);   // no compression
    WRITE(uint32_t, 0, file);   // vmask
    WRITE(uint32_t, count, file);

    i = 0;
    MESH_ITER_BLOCKS(mesh, bpos, block) {

        sprintf(buff, "%d", i);
        WRITE(uint8_t, strlen(buff), file);
        fwrite(buff, strlen(buff), 1, file);

        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(uint32_t, BLOCK_SIZE - 2, file);
        WRITE(int32_t, bpos[0] - BLOCK_SIZE / 2, file);
        WRITE(int32_t, bpos[1] - BLOCK_SIZE / 2, file);
        WRITE(int32_t, bpos[2] - BLOCK_SIZE / 2, file);
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
    mesh_delete(m);
}

static void export_as_qubicle(const char *path)
{
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "qubicle\0*.qb\0", NULL, "untitled.qb");
    if (!path) return;
    qubicle_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(import_qubicle,
    .help = "Import a qubicle file",
    .cfunc = qubicle_import,
    .csig = "vp",
    .file_format = {
        .name = "qubicle",
        .ext = "*.qb\0",
    },
)

ACTION_REGISTER(export_as_qubicle,
    .help = "Save the image as a qubicle 3d file",
    .cfunc = export_as_qubicle,
    .csig = "vp",
    .file_format = {
        .name = "qubicle",
        .ext = "*.qb\0",
    },
)
