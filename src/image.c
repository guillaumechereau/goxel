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

static void print_history(const image_t *img)
{
    if (!DEBUG) return;
    const image_t *im;
    int i = 0;
    LOG_V("hist");
    DL_FOREACH2(img->history, im, history_next) {
        LOG_V("%s %d (%p)", im == img->history_current ? "*" : " ", i, im);
        i++;
    }
}

image_t *image_new(void)
{
    layer_t *layer;
    image_t *img = calloc(1, sizeof(*img));
    img->export_width = 256;
    img->export_height = 256;
    layer = calloc(1, sizeof(*img->layers));
    sprintf(layer->name, "background");
    layer->mesh = mesh_new();
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    image_history_push(img);
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
        layer = calloc(1, sizeof(*layer));
        *layer = *other_layer;
        layer->next = layer->prev = NULL;
        layer->mesh = mesh_copy(other_layer->mesh);
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
        mesh_delete(layer->mesh);
        free(layer);
    }
    free(img->path);
    free(img);
}

void image_add_layer(image_t *img)
{
    layer_t *layer;
    layer = (layer_t*)calloc(1, sizeof(*layer));
    sprintf(layer->name, "unamed");
    layer->mesh = mesh_new();
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    image_history_push(img);
    goxel_update_meshes(goxel(), true);
}

void image_delete_layer(image_t *img, layer_t *layer)
{
    DL_DELETE(img->layers, layer);
    if (layer == img->active_layer) img->active_layer = NULL;
    mesh_delete(layer->mesh);
    free(layer);
    if (img->layers == NULL) {
        layer = (layer_t*)calloc(1, sizeof(*layer));
        layer->mesh = mesh_new();
        sprintf(layer->name, "unamed");
        layer->visible = true;
        DL_APPEND(img->layers, layer);
    }
    if (!img->active_layer) img->active_layer = img->layers->prev;
    image_history_push(img);
    goxel_update_meshes(goxel(), true);
}

void image_move_layer(image_t *img, layer_t *layer, int d)
{
    assert(d == -1 || d == +1);
    layer_t *other = NULL;
    if (d == -1) {
        other = layer->next;
        SWAP(other, layer);
    } else if (layer != img->layers) {
        other = layer->prev;
    }
    if (!other || !layer) return;
    DL_DELETE(img->layers, layer);
    DL_PREPEND_ELEM(img->layers, other, layer);
    image_history_push(img);
    goxel_update_meshes(goxel(), true);
}

void image_duplicate_layer(image_t *img, layer_t *other)
{
    layer_t *layer;
    layer = (layer_t*)calloc(1, sizeof(*layer));
    sprintf(layer->name, "%s", other->name);
    layer->mesh = mesh_copy(other->mesh);
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    image_history_push(img);
    goxel_update_meshes(goxel(), true);
}

void image_merge_visible_layers(image_t *img)
{
    layer_t *layer, *last = NULL;
    DL_FOREACH(img->layers, layer) {
        if (!layer->visible) continue;
        if (last) {
            mesh_merge(layer->mesh, last->mesh);
            DL_DELETE(img->layers, last);
            mesh_delete(last->mesh);
            free(last);
        }
        last = layer;
    }
    if (last) img->active_layer = last;
    image_history_push(img);
    goxel_update_meshes(goxel(), true);
}

void image_set(image_t *img, image_t *other)
{
    layer_t *layer, *tmp, *other_layer;
    DL_FOREACH_SAFE(img->layers, layer, tmp) {
        DL_DELETE(img->layers, layer);
        mesh_delete(layer->mesh);
        free(layer);
    }
    DL_FOREACH(other->layers, other_layer) {
        layer = calloc(1, sizeof(*layer));
        *layer = *other_layer;
        layer->next = layer->prev = NULL;
        layer->mesh = mesh_copy(other_layer->mesh);
        DL_APPEND(img->layers, layer);
        if (other_layer == other->active_layer)
            img->active_layer = layer;
    }
}

void image_history_push(image_t *img)
{
    image_t *snap = image_copy(img);
    if (!img->history_current) img->history_current = img->history;
    // Discard previous undo.
    while (img->history != img->history_current)
        DL_DELETE2(img->history, img->history, history_prev, history_next);
    DL_PREPEND2(img->history, snap, history_prev, history_next);
    img->history_current = img->history;
    print_history(img);
}

void image_undo(image_t *img)
{
    if (!img->history_current->history_next) return;
    img->history_current = img->history_current->history_next;
    image_set(img, img->history_current);
    goxel_update_meshes(goxel(), true);
    print_history(img);
}

void image_redo(image_t *img)
{
    image_t *cur = img->history_current;
    if (!cur || cur == img->history) return;
    img->history_current = cur->history_prev;
    image_set(img, img->history_current);
    goxel_update_meshes(goxel(), true);
    print_history(img);
}
