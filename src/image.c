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

    img->history                                        img
        |                                                |
        v                                                v
    +--------+       +--------+       +--------+      +--------+
    |        |       |        |       |        |      |        |
    |   A    |------>|   B    |------>|   C    |----->|   D    |
    |        |       |        |       |        |      |        |
    +--------+       +--------+       +--------+      +--------+

    After an undo, we get:

    img->history                        img
        |                                |
        v                                v
    +--------+       +--------+       +--------+     +--------+
    |        |       |        |       |        |     |        |
    |   A    |------>|   B    |------>|   C    |---->|   D    |
    |        |       |        |       |        |     |        |
    +--------+       +--------+       +--------+     +--------+


*/

static layer_t *img_get_layer(const image_t *img, int id)
{
    layer_t *layer;
    if (id == 0) return NULL;
    DL_FOREACH(img->layers, layer)
        if (layer->id == id) return layer;
    assert(false);
    return NULL;
}

static int img_get_new_id(const image_t *img)
{
    int id;
    layer_t *layer;
    for (id = 1;; id++) {
        DL_FOREACH(img->layers, layer)
            if (layer->id == id) break;
        if (layer == NULL) break;
    }
    return id;
}

static layer_t *layer_new(const image_t *img, const char *name)
{
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    // XXX: potential bug here.
    strncpy(layer->name, name, sizeof(layer->name));
    layer->mesh = mesh_new();
    mat4_set_identity(layer->mat);
    layer->id = img_get_new_id(img);
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
    mat4_copy(other->box, layer->box);
    mat4_copy(other->mat, layer->mat);
    layer->id = other->id;
    layer->base_id = other->base_id;
    layer->base_mesh_key = other->base_mesh_key;
    return layer;
}

static layer_t *layer_clone(layer_t *other)
{
    int len;
    layer_t *layer;
    layer = calloc(1, sizeof(*layer));
    len = sizeof(layer->name) - 1 - strlen(" clone");
    snprintf(layer->name, sizeof(layer->name), "%.*s clone", len, other->name);
    layer->visible = other->visible;
    layer->mesh = mesh_copy(other->mesh);
    mat4_set_identity(layer->mat);
    layer->base_id = other->id;
    layer->base_mesh_key = mesh_get_key(other->mesh);
    return layer;
}

// Make sure the layer mesh is up to date.
void image_update(image_t *img)
{
    layer_t *layer, *base;
    DL_FOREACH(img->layers, layer) {
        base = img_get_layer(img, layer->base_id);
        if (base && layer->base_mesh_key != mesh_get_key(base->mesh)) {
            mesh_set(layer->mesh, base->mesh);
            mesh_move(layer->mesh, layer->mat);
            layer->base_mesh_key = mesh_get_key(base->mesh);
        }
    }
}

static void layer_delete(layer_t *layer)
{
    mesh_delete(layer->mesh);
    texture_delete(layer->image);
    free(layer);
}

static uint64_t layer_get_key(const layer_t *layer)
{
    uint64_t key;
    key = mesh_get_key(layer->mesh);
    key = crc64(key, &layer->visible, sizeof(layer->visible));
    key = crc64(key, &layer->name, sizeof(layer->name));
    key = crc64(key, &layer->box, sizeof(layer->box));
    key = crc64(key, &layer->mat, sizeof(layer->mat));
    return key;
}

image_t *image_new(void)
{
    layer_t *layer;
    image_t *img = calloc(1, sizeof(*img));
    const int aabb[2][3] = {{-16, -16, 0}, {16, 16, 32}};
    bbox_from_aabb(img->box, aabb);
    img->export_width = 1024;
    img->export_height = 1024;
    layer = layer_new(img, "background");
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    DL_APPEND2(img->history, img, history_prev, history_next);
    img->active_layer = layer;
    return img;
}

static image_t *image_snap(image_t *other)
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
    img->history_next = img->history_prev = NULL;
    assert(img->active_layer);
    return img;
}


static void image_delete_camera(image_t *img, camera_t *cam);

void image_delete(image_t *img)
{
    if (!img) return;
    image_t *hist, *snap, *snap_tmp;
    layer_t *layer, *layer_tmp;
    while (img->cameras)
        image_delete_camera(img, img->cameras);
    free(img->path);
    hist = img->history;
    DL_FOREACH_SAFE2(hist, snap, snap_tmp, history_next) {
        DL_FOREACH_SAFE(snap->layers, layer, layer_tmp) {
            DL_DELETE(snap->layers, layer);
            layer_delete(layer);
        }
        DL_DELETE2(hist, snap, history_prev, history_next);
        free(snap);
    }
}

layer_t *image_add_layer(image_t *img)
{
    layer_t *layer;
    img = img ?: goxel.image;
    layer = layer_new(img, "unnamed");
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_delete_layer(image_t *img, layer_t *layer)
{
    layer_t *other;
    img = img ?: goxel.image;
    layer = layer ?: img->active_layer;
    DL_DELETE(img->layers, layer);
    if (layer == img->active_layer) img->active_layer = NULL;

    // Unclone all layers cloned from this one.
    DL_FOREACH(goxel.image->layers, other) {
        if (other->base_id == layer->id) {
            other->base_id = 0;
        }
    }

    layer_delete(layer);
    if (img->layers == NULL) {
        layer = layer_new(img, "unnamed");
        layer->visible = true;
        DL_APPEND(img->layers, layer);
    }
    if (!img->active_layer) img->active_layer = img->layers->prev;
}

void image_move_layer(image_t *img, layer_t *layer, int d)
{
    assert(d == -1 || d == +1);
    layer_t *other = NULL;
    img = img ?: goxel.image;
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

static void image_move_layer_up(image_t *img, layer_t *layer)
{
    image_move_layer(img, layer, +1);
}

static void image_move_layer_down(image_t *img, layer_t *layer)
{
    image_move_layer(img, layer, -1);
}

layer_t *image_duplicate_layer(image_t *img, layer_t *other)
{
    layer_t *layer;
    img = img ?: goxel.image;
    other = other ?: img->active_layer;
    layer = layer_copy(other);
    layer->id = img_get_new_id(img);
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

layer_t *image_clone_layer(image_t *img, layer_t *other)
{
    layer_t *layer;
    img = img ?: goxel.image;
    other = other ?: img->active_layer;
    layer = layer_clone(other);
    layer->id = img_get_new_id(img);
    layer->visible = true;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_unclone_layer(image_t *img, layer_t *layer)
{
    img = img ?: goxel.image;
    layer = layer ?: img->active_layer;
    layer->base_id = 0;
}

void image_select_parent_layer(image_t *img, layer_t *layer)
{
    img = img ?: goxel.image;
    layer = layer ?: img->active_layer;
    img->active_layer = img_get_layer(img, layer->base_id);
}

void image_merge_visible_layers(image_t *img)
{
    layer_t *layer, *last = NULL;
    img = img ?: goxel.image;
    DL_FOREACH(img->layers, layer) {
        if (!layer->visible) continue;
        if (last) {
            mesh_merge(layer->mesh, last->mesh, MODE_OVER, NULL);
            DL_DELETE(img->layers, last);
            layer_delete(last);
        }
        last = layer;
    }
    if (last) img->active_layer = last;
}


camera_t *image_add_camera(image_t *img)
{
    camera_t *cam;
    img = img ?: goxel.image;
    cam = camera_new("unnamed");
    DL_APPEND(img->cameras, cam);
    img->active_camera = cam;
    return cam;
}

static void image_delete_camera(image_t *img, camera_t *cam)
{
    img = img ?: goxel.image;
    cam = cam ?: img->active_camera;
    if (!cam) return;
    DL_DELETE(img->cameras, cam);
    if (cam == img->active_camera) img->active_camera = NULL;
    camera_delete(cam);
}

void image_move_camera(image_t *img, camera_t *cam, int d)
{
    // XXX: make a generic algo to move objects in a list.
    assert(d == -1 || d == +1);
    camera_t *other = NULL;
    img = img ?: goxel.image;
    cam = cam ?: img->active_camera;
    if (!cam) return;
    if (d == -1) {
        other = cam->next;
        SWAP(other, cam);
    } else if (cam != img->cameras) {
        other = cam->prev;
    }
    if (!other || !cam) return;
    DL_DELETE(img->cameras, cam);
    DL_PREPEND_ELEM(img->cameras, other, cam);
}

static void image_move_camera_up(image_t *img, camera_t *cam)
{
    image_move_camera(img, cam, +1);
}

static void image_move_camera_down(image_t *img, camera_t *cam)
{
    image_move_camera(img, cam, -1);
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

#if 0 // For debugging purpose.
static void debug_print_history(image_t *img)
{
    int i = 0;
    image_t *hist;
    DL_FOREACH2(img->history, hist, history_next) {
        printf("%d%s  ", i++, hist == img ? "*" : " ");
    }
    printf("\n");
}
#else
static void debug_print_history(image_t *img) {}
#endif

void image_history_push(image_t *img)
{
    image_t *snap = image_snap(img);
    image_t *hist;
    layer_t *layer, *layer_tmp;

    // Discard previous undo.
    while ((hist = img->history_next)) {
        DL_FOREACH_SAFE(hist->layers, layer, layer_tmp) {
            DL_DELETE(hist->layers, layer);
            layer_delete(layer);
        }
        DL_DELETE2(img->history, hist, history_prev, history_next);
        free(hist);
    }

    DL_DELETE2(img->history, img,  history_prev, history_next);
    DL_APPEND2(img->history, snap, history_prev, history_next);
    DL_APPEND2(img->history, img,  history_prev, history_next);
    debug_print_history(img);
}

// XXX: not clear what this is doing.  We should try to remove it.
// It swap the content of two images without touching their pointer or
// history.
static void swap(image_t *a, image_t *b)
{
    SWAP(*a, *b);
    SWAP(a->history, b->history);
    SWAP(a->history_next, b->history_next);
    SWAP(a->history_prev, b->history_prev);
}

void image_undo(image_t *img)
{
    image_t *prev = img->history_prev;
    if (img->history == img) {
        LOG_D("No more undo");
        return;
    }
    DL_DELETE2(img->history, img, history_prev, history_next);
    DL_PREPEND_ELEM2(img->history, prev, img, history_prev, history_next);
    swap(img, prev);
    goxel_update_meshes(-1);
    debug_print_history(img);
}

void image_redo(image_t *img)
{
    image_t *next = img->history_next;
    if (!next) {
        LOG_D("No more redo");
        return;
    }
    DL_DELETE2(img->history, next, history_prev, history_next);
    DL_PREPEND_ELEM2(img->history, img, next, history_prev, history_next);
    swap(img, next);
    goxel_update_meshes(-1);
    debug_print_history(img);
}

void image_clear_layer(layer_t *layer, const float box[4][4])
{
    painter_t painter;
    layer = layer ?: goxel.image->active_layer;
    if (!box || box_is_null(box)) {
        mesh_clear(layer->mesh);
        return;
    }
    painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_SUB,
        .color = {255, 255, 255, 255},
    };
    mesh_op(layer->mesh, &painter, box);
}

bool image_layer_can_edit(const image_t *img, const layer_t *layer)
{
    return !layer->base_id && !layer->image;
}

/*
 * Function: image_get_key
 * Return a value that is garantied to change when the image change.
 */
uint64_t image_get_key(const image_t *img)
{
    uint64_t key = 0, k;
    layer_t *layer;
    camera_t *camera;
    DL_FOREACH(img->layers, layer) {
        k = layer_get_key(layer);
        key = crc64(key, &k, sizeof(k));
    }
    DL_FOREACH(img->cameras, camera) {
        k = camera_get_key(camera);
        key = crc64(key, &k, sizeof(k));
    }
    return key;
}

/*
 * Turn an image layer into a mesh of 1 voxel depth.
 */
static void image_image_layer_to_mesh(image_t *img, layer_t *layer)
{
    uint8_t *data;
    int i, j, w, h, bpp = 0, pos[3];
    uint8_t c[4];
    float p[3];
    img = img ?: goxel.image;
    layer = layer ?: img->active_layer;
    mesh_accessor_t acc;

    image_history_push(img);
    data = img_read(layer->image->path, &w, &h, &bpp);
    acc = mesh_get_accessor(layer->mesh);
    for (j = 0; j < w; j++)
    for (i = 0; i < h; i++) {
        vec3_set(p, i / (float)h - 0.5, 0.5 - j / (float)w, 0);
        mat4_mul_vec3(layer->mat, p, p);
        pos[0] = round(p[0]);
        pos[1] = round(p[1]);
        pos[2] = round(p[2]);
        memset(c, 0, 4);
        c[3] = 255;
        memcpy(c, data + (j * w + i) * bpp, bpp);
        mesh_set_at(layer->mesh, &acc, pos, c);
    }
    texture_delete(layer->image);
    layer->image = NULL;
    free(data);
}

ACTION_REGISTER(layer_clear,
    .help = "Clear the current layer",
    .cfunc = image_clear_layer,
    .csig = "vpp",
    .icon = ICON_DELETE,
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_new_layer,
    .help = "Add a new layer to the image",
    .cfunc = image_add_layer,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ADD,
)

ACTION_REGISTER(img_del_layer,
    .help = "Delete the active layer",
    .cfunc = image_delete_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_REMOVE,
)

ACTION_REGISTER(img_move_layer,
    .help = "Move the active layer",
    .cfunc = image_move_layer,
    .csig = "vppi",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_move_layer_up,
    .help = "Move the active layer up",
    .cfunc = image_move_layer_up,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_UPWARD,
)

ACTION_REGISTER(img_move_layer_down,
    .help = "Move the active layer down",
    .cfunc = image_move_layer_down,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_DOWNWARD,
)

ACTION_REGISTER(img_duplicate_layer,
    .help = "Duplicate the active layer",
    .cfunc = image_duplicate_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_clone_layer,
    .help = "Clone the active layer",
    .cfunc = image_clone_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_unclone_layer,
    .help = "Unclone the active layer",
    .cfunc = image_unclone_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_select_parent_layer,
    .help = "Select the parent of a layer",
    .cfunc = image_select_parent_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_merge_visible_layers,
    .help = "Merge all the visible layers",
    .cfunc = image_merge_visible_layers,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
)


ACTION_REGISTER(img_new_camera,
    .help = "Add a new camera to the image",
    .cfunc = image_add_camera,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ADD,
)

ACTION_REGISTER(img_del_camera,
    .help = "Delete the active camera",
    .cfunc = image_delete_camera,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_REMOVE,
)

ACTION_REGISTER(img_move_camera,
    .help = "Move the active camera",
    .cfunc = image_move_camera,
    .csig = "vppi",
    .flags = ACTION_TOUCH_IMAGE,
)

ACTION_REGISTER(img_move_camera_up,
    .help = "Move the active camera up",
    .cfunc = image_move_camera_up,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_UPWARD,
)

ACTION_REGISTER(img_move_camera_down,
    .help = "Move the active camera down",
    .cfunc = image_move_camera_down,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_DOWNWARD,
)

ACTION_REGISTER(img_image_layer_to_mesh,
    .help = "Turn an image layer into a mesh",
    .cfunc = image_image_layer_to_mesh,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
