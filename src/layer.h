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
