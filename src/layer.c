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

#include "goxel.h"
#include "xxhash.h"

layer_t *layer_new(const char *name)
{
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    layer->ref = 1;
    if (name) strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->volume = volume_new();
    mat4_set_identity(layer->mat);
    return layer;
}

void layer_delete(layer_t *layer)
{
    if (!layer) return;
    if (--layer->ref > 0) return;
    volume_delete(layer->volume);
    texture_delete(layer->image);
    free(layer);
}

uint32_t layer_get_key(const layer_t *layer)
{
    uint32_t key;
    uint64_t volume_key;
    uint32_t mat_key;

    volume_key = volume_get_key(layer->volume);
    mat_key = material_get_hash(layer->material);

    key = 0;
    key = XXH32(&volume_key, sizeof(volume_key), key);
    key = XXH32(&layer->visible, sizeof(layer->visible), key);
    key = XXH32(&layer->name, sizeof(layer->name), key);
    key = XXH32(&layer->box, sizeof(layer->box), key);
    key = XXH32(&layer->mat, sizeof(layer->mat), key);
    key = XXH32(&layer->shape, sizeof(layer->shape), key);
    key = XXH32(&layer->color, sizeof(layer->color), key);
    key = XXH32(&mat_key, sizeof(mat_key), key);
    return key;
}

layer_t *layer_copy(layer_t *other)
{
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    layer->ref = 1;
    memcpy(layer->name, other->name, sizeof(layer->name));
    layer->visible = other->visible;
    layer->volume = volume_copy(other->volume);
    layer->image = texture_copy(other->image);
    layer->material = other->material;
    mat4_copy(other->box, layer->box);
    mat4_copy(other->mat, layer->mat);
    layer->id = other->id;
    layer->base_id = other->base_id;
    layer->base_volume_key = other->base_volume_key;
    layer->shape = other->shape;
    layer->shape_key = other->shape_key;
    memcpy(layer->color, other->color, sizeof(layer->color));
    return layer;
}

/*
 * Function: layer_get_bounding_box
 * Return the layer box if set, otherwise the bounding box of the layer
 * volume.
 */
void layer_get_bounding_box(const layer_t *layer, float box[4][4])
{
    int aabb[2][3];
    if (!box_is_null(layer->box)) {
        mat4_copy(layer->box, box);
        return;
    }
    volume_get_bbox(layer->volume, aabb, true);
    if (aabb[0][0] > aabb[1][0]) memset(aabb, 0, sizeof(aabb));
    bbox_from_aabb(box, aabb);
}
