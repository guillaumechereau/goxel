/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

/* History
    the images undo history is stored in a linked list.  Every time we call
    image_history_push, we add the current image snapshot in the list.

    For example, if we did three operations, A, B, C, and now the image is
    in the D state, the history list looks like this:

    img->history     img->history_current -> NULL      img
        |                                               |
        v                                               v
    +--------+       +--------+       +--------+     +--------+
    |        |       |        |       |        |     |        |
    |   A    |------>|   B    |------>|   C    |     |   D    |
    |        |       |        |       |        |     |        |
    +--------+       +--------+       +--------+     +--------+

    After an undo, we get:

    img->history                        img->history_current
        |                               img
        v                                v
    +--------+       +--------+       +--------+     +--------+
    |        |       |        |       |        |     |        |
    |   A    |------>|   B    |------>|   C    |---->|   D    |
    |        |       |        |       |        |     |        |
    +--------+       +--------+       +--------+     +--------+


*/

static layer_t *layer_new(const char *name)
{
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    // XXX: potential bug here.
    strncpy(layer->name, name, sizeof(layer->name));
    layer->mesh = mesh_new();
    layer->mat = mat4_identity;
    return layer;
}

static layer_t *layer_copy(layer_t *other)
{
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    memcpy(layer->name, other->name, sizeof(layer->name));
    layer->visible = other->visible;
    layer->mesh = mesh_copy(other->mesh);
    layer->image = texture_copy(other->image);
    layer->mat = other->mat;
    return layer;
}

static void layer_delete(layer_t *layer)
{
    mesh_delete(layer->mesh);
    texture_delete(layer->image);
    free(layer);
}

image_t *image_new(void)
{
    layer_t *layer;
    image_t *img = calloc(1, sizeof(*img));
    img->export_width = 256;
    img->export_height = 256;
    layer = layer_new("background");
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return img;
}

image_t *image_copy(image_t *other)
{
    image_t *img;
    layer_t *layer, *other_layer;
    img = calloc(1, sizeof(*img));
    *img = *other;
    img->layers = NULL;
    img->active_layer = NULL;
    DL_FOREACH(other->layers, other_layer) {
        layer = layer_copy(other_layer);
        DL_APPEND(img->layers, layer);
        if (other_layer == other->active_layer)
            img->active_layer = layer;
    }
    assert(img->active_layer);
    return img;
}

void image_delete(image_t *img)
{
    layer_t *layer, *tmp;
    DL_FOREACH_SAFE(img->layers, layer, tmp) {
        DL_DELETE(img->layers, layer);
        layer_delete(layer);
    }
    free(img->path);
    free(img);
}

layer_t *image_add_layer(image_t *img)
{
    layer_t *layer;
    img = img ?: goxel->image;
    layer = layer_new("unamed");
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_delete_layer(image_t *img, layer_t *layer)
{
    img = img ?: goxel->image;
    layer = layer ?: img->active_layer;
    DL_DELETE(img->layers, layer);
    if (layer == img->active_layer) img->active_layer = NULL;
    layer_delete(layer);
    if (img->layers == NULL) {
        layer = layer_new("unamed");
        layer->visible = true;
        DL_APPEND(img->layers, layer);
    }
    if (!img->active_layer) img->active_layer = img->layers->prev;
}

void image_move_layer(image_t *img, layer_t *layer, int d)
{
    assert(d == -1 || d == +1);
    layer_t *other = NULL;
    img = img ?: goxel->image;
    layer = layer ?: img->active_layer;
    if (d == -1) {
        other = layer->next;
        SWAP(other, layer);
    } else if (layer != img->layers) {
        other = layer->prev;
    }
    if (!other || !layer) return;
    DL_DELETE(img->layers, layer);
    DL_PREPEND_ELEM(img->layers, other, layer);
}

layer_t *image_duplicate_layer(image_t *img, layer_t *other)
{
    layer_t *layer;
    img = img ?: goxel->image;
    other = other ?: img->active_layer;
    layer = layer_copy(other);
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_merge_visible_layers(image_t *img)
{
    layer_t *layer, *last = NULL;
    img = img ?: goxel->image;
    DL_FOREACH(img->layers, layer) {
        if (!layer->visible) continue;
        if (last) {
            mesh_merge(layer->mesh, last->mesh, MODE_ADD);
            DL_DELETE(img->layers, last);
            layer_delete(last);
        }
        last = layer;
    }
    if (last) img->active_layer = last;
}

void image_set(image_t *img, image_t *other)
{
    layer_t *layer, *tmp, *other_layer;
    DL_FOREACH_SAFE(img->layers, layer, tmp) {
        DL_DELETE(img->layers, layer);
        layer_delete(layer);
    }
    DL_FOREACH(other->layers, other_layer) {
        layer = layer_copy(other_layer);
        DL_APPEND(img->layers, layer);
        if (other_layer == other->active_layer)
            img->active_layer = layer;
    }
}

void image_history_push(image_t *img)
{
    image_t *snap = image_copy(img);
    image_t *tmp;

    // Discard previous undo.
    // XXX: also need to delete the images!!
    while (img->history_current) {
        tmp = img->history_current;
        img->history_current = tmp->history_next;
        DL_DELETE2(img->history, tmp, history_prev, history_next);
    }

    DL_APPEND2(img->history, snap, history_prev, history_next);
    img->history_current = NULL;
}

void image_undo(image_t *img)
{
    if (img->history_current == img->history) {
        LOG_D("No more undo");
        return;
    }
    if (!img->history_current) {
        image_t *snap = image_copy(img);
        DL_APPEND2(img->history, snap, history_prev, history_next);
        img->history_current = img->history->history_prev;
    }

    image_set(img, img->history_current->history_prev);
    img->history_current = img->history_current->history_prev;
    goxel_update_meshes(goxel, -1);
}

void image_redo(image_t *img)
{
    if (!img->history_current || !img->history_current->history_next) {
        LOG_D("No more redo");
        return;
    }
    img->history_current = img->history_current->history_next;
    image_set(img, img->history_current);
    goxel_update_meshes(goxel, -1);
}

void image_clear_layer(layer_t *layer, const box_t *box)
{
    painter_t painter;
    layer = layer ?: goxel->image->active_layer;
    box = box ?: &goxel->selection;
    if (box_is_null(*box)) {
        mesh_clear(layer->mesh);
        return;
    }
    painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_SUB,
    };
    mesh_op(layer->mesh, &painter, box);
}

ACTION_REGISTER(layer_clear,
    .help = "Clear the current layer",
    .func = image_clear_layer,
    .sig = "pp",
)

ACTION_REGISTER(img_new_layer,
    .help = "Add a new layer to the image",
    .func = image_add_layer,
    .sig = "p",
)

ACTION_REGISTER(img_del_layer,
    .help = "Delete the active layer",
    .func = image_delete_layer,
    .sig = "pp",
)

ACTION_REGISTER(img_move_layer,
    .help = "Move the active layer",
    .func = image_move_layer,
    .sig = "ppi",
)

ACTION_REGISTER(img_duplicate_layer,
    .help = "Duplicate the active layer",
    .func = image_duplicate_layer,
    .sig = "pp",
)

ACTION_REGISTER(img_merge_visible_layers,
    .help = "Merge all the visible layers",
    .func = image_merge_visible_layers,
    .sig = "p",
)
