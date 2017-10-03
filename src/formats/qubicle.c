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
    int i, j, r, index, len, w, h, d, pos[3], vpos[3], x, y, z;
    char buff[256];
    union {
        uint8_t v[4];
        uint32_t uint32;
        struct {
            uint8_t r, g, b, a;
        };
    } v;
    mat4_t mat = mat4_identity;
    const uint32_t CODEFLAG = 2;
    const uint32_t NEXTSLICEFLAG = 6;
    mesh_t *mesh = goxel->image->active_layer->mesh;
    mesh_iterator_t iter = {0};

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
        if (compression == 0) {
            for (index = 0; index < w * h * d; index++) {
                v.uint32 = READ(uint32_t, file);
                if (!v.a) continue;
                v.a = v.a ? 255 : 0;
                vpos[0] = pos[0] + index % w;
                vpos[1] = pos[1] + (index % (w * h)) / w;
                vpos[2] = pos[2] + index / (w * h);
                mesh_set_at(mesh, vpos, v.v, &iter);
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
                        vpos[0] = pos[0] + x;
                        vpos[1] = pos[1] + y;
                        vpos[2] = pos[2] + z;
                        mesh_set_at(mesh, vpos, v.v, &iter);
                        index++;
                    }
                }
            }
        }
    }

    // Apply a 90 deg X rotation to fix axis.
    mat4_irotate(&mat, M_PI / 2, 1, 0, 0);
    mesh_move(goxel->image->active_layer->mesh, &mat);
    goxel_update_meshes(goxel, -1);
}

void qubicle_export(const mesh_t *mesh, const char *path)
{
    FILE *file;
    int i, count, x, y, z, bpos[3], vpos[3];
    uint8_t v[4];
    char buff[16];
    mesh_t *m = mesh_copy(mesh);
    mat4_t mat = mat4_identity;
    mesh_iterator_t iter;

    // Apply a -90 deg X rotation to fix axis.
    mat4_irotate(&mat, -M_PI / 2, 1, 0, 0);
    mesh_move(m, &mat);
    mesh = m;

    count = 0;
    iter = mesh_get_iterator(mesh);
    while (mesh_iter_blocks(mesh, &iter, NULL, NULL, NULL, NULL)) count++;

    file = fopen(path, "wb");
    WRITE(uint32_t, 257, file); // version
    WRITE(uint32_t, 0, file);   // color format RGBA
    WRITE(uint32_t, 0, file);   // orientation left handed
    WRITE(uint32_t, 0, file);   // no compression
    WRITE(uint32_t, 0, file);   // vmask
    WRITE(uint32_t, count, file);

    i = 0;
    iter = mesh_get_iterator(mesh);
    while (mesh_iter_blocks(mesh, &iter, bpos, NULL, NULL, NULL)) {
        sprintf(buff, "%d", i);
        WRITE(uint8_t, strlen(buff), file);
        fwrite(buff, strlen(buff), 1, file);

        WRITE(uint32_t, BLOCK_SIZE, file);
        WRITE(uint32_t, BLOCK_SIZE, file);
        WRITE(uint32_t, BLOCK_SIZE, file);
        WRITE(int32_t, bpos[0] - BLOCK_SIZE / 2, file);
        WRITE(int32_t, bpos[1] - BLOCK_SIZE / 2, file);
        WRITE(int32_t, bpos[2] - BLOCK_SIZE / 2, file);
        for (z = 0; z < BLOCK_SIZE; z++)
        for (y = 0; y < BLOCK_SIZE; y++)
        for (x = 0; x < BLOCK_SIZE; x++) {
            vpos[0] = bpos[0] - BLOCK_SIZE / 2 + x;
            vpos[1] = bpos[1] - BLOCK_SIZE / 2 + y;
            vpos[2] = bpos[2] - BLOCK_SIZE / 2 + z;
            mesh_get_at(mesh, vpos, &iter, v);
            fwrite(v, 4, 1, file);
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
