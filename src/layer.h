/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef LAYER_H
#define LAYER_H

#include "material.h"
#include "mesh.h"
#include "shape.h"
#include "utils/texture.h"

typedef struct layer layer_t;

struct layer {
    layer_t     *next, *prev;
    mesh_t      *mesh;
    const material_t  *material;
    int         id;         // Uniq id in the image (for clones).
    bool        visible;
    char        name[256];  // 256 chars max.
    float       box[4][4];  // Bounding box.
    float       mat[4][4];
    // For 2d image layers.
    texture_t   *image;
    // For clone layers:
    int         base_id;
    uint64_t    base_mesh_key;
    // For shape layers.
    const shape_t *shape;
    uint32_t    shape_key;
    uint8_t     color[4];
};

layer_t *layer_new(const char *name);
void layer_delete(layer_t *layer);
uint32_t layer_get_key(const layer_t *layer);
layer_t *layer_copy(layer_t *other);

/*
 * Function: layer_get_bounding_box
 * Return the layer box if set, otherwise the bounding box of the layer
 * mesh.
 */
void layer_get_bounding_box(const layer_t *layer, float box[4][4]);

#endif // LAYER_H
