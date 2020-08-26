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

#include "camera.h"
#include "layer.h"
#include "material.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct history history_t;

typedef struct image image_t;
struct image {

    layer_t *layers;
    layer_t *active_layer;

    camera_t *cameras;
    camera_t *active_camera;

    material_t *materials;
    material_t *active_material;

    float    box[4][4];

    // For saving.
    // XXX: I think those should be persistend data of export code instead.
    char     *path;
    bool     export_custom_size;
    int      export_width;
    int      export_height;
    bool     export_transparent_background;
    uint32_t saved_key;     // image_get_key() value of saved file.

    image_t *history;
    image_t *history_next, *history_prev;
};

image_t *image_new(void);
void image_delete(image_t *img);
layer_t *image_add_layer(image_t *img, layer_t *layer);
void image_delete_layer(image_t *img, layer_t *layer);
layer_t *image_duplicate_layer(image_t *img, layer_t *layer);
void image_merge_visible_layers(image_t *img);

void image_history_push(image_t *img);
void image_undo(image_t *img);
void image_redo(image_t *img);
void image_history_resize(image_t *img, int size);

bool image_layer_can_edit(const image_t *img, const layer_t *layer);

material_t *image_add_material(image_t *img, material_t *mat);
void image_delete_material(image_t *img, material_t *mat);

camera_t *image_add_camera(image_t *img, camera_t *cam);
void image_delete_camera(image_t *img, camera_t *cam);

/*
 * Function: image_get_key
 * Return a value that is guarantied to change when the image change.
 */
uint32_t image_get_key(const image_t *img);

#endif // IMAGE_H
