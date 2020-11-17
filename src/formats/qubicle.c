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
#include "file_format.h"

// Load qubicle files.

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

static void apply_orientation(int orientation, int pos[3])
{
    if (orientation == 1) {// Right
        SWAP(pos[1], pos[2]);
    } else {               // Left
        SWAP(pos[1], pos[2]);
        pos[1] = -pos[1];
    }
}

static int qubicle_import(image_t *image, const char *path)
{
    FILE *file;
    int version, color_format, orientation, compression, vmask, mat_count;
    int i, j, r, index, len, w, h, d, pos[3], vpos[3], x, y, z, bbox[2][3];
    union {
        uint8_t v[4];
        uint32_t uint32;
        struct {
            uint8_t r, g, b, a;
        };
    } v;
    const uint32_t CODEFLAG = 2;
    const uint32_t NEXTSLICEFLAG = 6;
    layer_t *layer;
    mesh_iterator_t iter = {0};

    file = fopen(path, "rb");
    version = READ(uint32_t, file);
    (void)version;
    color_format = READ(uint32_t, file);
    (void)color_format;
    orientation = READ(uint32_t, file);
    compression = READ(uint32_t, file);
    vmask = READ(uint32_t, file);
    (void)vmask;
    mat_count = READ(uint32_t, file);

    for (i = 0; i < mat_count; i++) {
        layer = image_add_layer(goxel.image, NULL);
        iter = mesh_get_accessor(layer->mesh);
        memset(layer->name, 0, sizeof(layer->name));
        len = READ(uint8_t, file);
        r = (int)fread(layer->name, len, 1, file);
        (void)r;
        w = READ(uint32_t, file);
        h = READ(uint32_t, file);
        d = READ(uint32_t, file);
        pos[0] = READ(int32_t, file);
        pos[1] = READ(int32_t, file);
        pos[2] = READ(int32_t, file);

        // Set the layer bounding box.
        vec3_set(bbox[0], pos[0], pos[1], pos[2]);
        vec3_set(bbox[1], pos[0] + w, pos[1] + h, pos[2] + d);
        apply_orientation(orientation, bbox[0]);
        apply_orientation(orientation, bbox[1]);
        if (bbox[1][1] < bbox[0][1]) {
            SWAP(bbox[1][1], bbox[0][1]);
            bbox[0][1] += 1;
            bbox[1][1] += 1;
        }
        bbox_from_aabb(layer->box, bbox);

        if (compression == 0) {
            for (index = 0; index < w * h * d; index++) {
                v.uint32 = READ(uint32_t, file);
                if (!v.a) continue;
                v.a = v.a ? 255 : 0;
                vpos[0] = pos[0] + index % w;
                vpos[1] = pos[1] + (index % (w * h)) / w;
                vpos[2] = pos[2] + index / (w * h);
                apply_orientation(orientation, vpos);
                mesh_set_at(layer->mesh, &iter, vpos, v.v);
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
                        apply_orientation(orientation, vpos);
                        mesh_set_at(layer->mesh, &iter, vpos, v.v);
                        index++;
                    }
                }
            }
        }
    }
    return 0;
}

static int qubicle_export(const image_t *img, const char *path)
{
    FILE *file;
    int i, count, x, y, z, pos[3], bbox[2][3];
    uint8_t v[4];
    layer_t *layer;
    mesh_iterator_t iter;
    mesh_t *mesh;

    count = 0;
    DL_COUNT(img->layers, layer, count);

    file = fopen(path, "wb");
    WRITE(uint32_t, 257, file); // version
    WRITE(uint32_t, 0, file);   // color format RGBA
    WRITE(uint32_t, 1, file);   // orientation right handed
    WRITE(uint32_t, 0, file);   // no compression
    WRITE(uint32_t, 0, file);   // vmask
    WRITE(uint32_t, count, file);

    i = 0;
    DL_FOREACH(img->layers, layer) {
        mesh = layer->mesh;

        if (!box_is_null(layer->box))
            bbox_to_aabb(layer->box, bbox);
        else
            if (!mesh_get_bbox(mesh, bbox, true)) continue;

        WRITE(uint8_t, strlen(layer->name), file);
        fwrite(layer->name, strlen(layer->name), 1, file);
        WRITE(uint32_t, bbox[1][0] - bbox[0][0], file);
        WRITE(uint32_t, bbox[1][2] - bbox[0][2], file);
        WRITE(uint32_t, bbox[1][1] - bbox[0][1], file);
        WRITE(int32_t, bbox[0][0], file);
        WRITE(int32_t, bbox[0][2], file);
        WRITE(int32_t, bbox[0][1], file);
        iter = mesh_get_accessor(mesh);
        for (y = bbox[0][1]; y < bbox[1][1]; y++)
        for (z = bbox[0][2]; z < bbox[1][2]; z++)
        for (x = bbox[0][0]; x < bbox[1][0]; x++) {
            pos[0] = x;
            pos[1] = y;
            pos[2] = z;
            mesh_get_at(mesh, &iter, pos, v);
            fwrite(v, 4, 1, file);
        }
        i++;
    }
    fclose(file);
    return 0;
}

FILE_FORMAT_REGISTER(qubicle,
    .name = "qubicle",
    .ext = "qubicle\0*.qb\0",
    .import_func = qubicle_import,
    .export_func = qubicle_export,
)
