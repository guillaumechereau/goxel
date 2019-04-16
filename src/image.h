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

#ifndef IMAGE_H
#define IMAGE_H

#include "mesh.h"
#include "shape.h"
#include "utils/texture.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct history history_t;

typedef struct layer layer_t;
struct layer {
    layer_t     *next, *prev;
    mesh_t      *mesh;
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
    uint64_t    shape_key;
    uint8_t     color[4];
};

typedef struct image image_t;
struct image {
    layer_t *layers;
    layer_t *active_layer;
    camera_t *cameras;
    camera_t *active_camera;
    float    box[4][4];

    // For saving.
    char     *path;
    int      export_width;
    int      export_height;
    bool     export_transparent_background;
    uint64_t saved_key;     // image_get_key() value of saved file.

    image_t *history;
    image_t *history_next, *history_prev;
};

image_t *image_new(void);
void image_delete(image_t *img);
layer_t *image_add_layer(image_t *img);
void image_delete_layer(image_t *img, layer_t *layer);
void image_move_layer(image_t *img, layer_t *layer, int d);
layer_t *image_duplicate_layer(image_t *img, layer_t *layer);
void image_merge_visible_layers(image_t *img);
void image_history_push(image_t *img);
void image_undo(image_t *img);
void image_redo(image_t *img);
bool image_layer_can_edit(const image_t *img, const layer_t *layer);

/*
 * Function: image_get_key
 * Return a value that is guarantied to change when the image change.
 */
uint64_t image_get_key(const image_t *img);

#endif // IMAGE_H
