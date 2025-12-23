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
#include "xxhash.h"


static bool material_name_exists(void *user, const char *name)
{
    const image_t *img = user;
    const material_t *m;
    DL_FOREACH(img->materials, m) {
        if (strcasecmp(m->name, name) == 0) return true;
    }
    return false;
}

static bool layer_name_exists(void *user, const char *name)
{
    const image_t *img = user;
    const layer_t *layer;
    DL_FOREACH(img->layers, layer) {
        if (strcasecmp(layer->name, name) == 0) return true;
    }
    return false;
}

static bool camera_name_exists(void *user, const char *name)
{
    const image_t *img = user;
    const camera_t *cam;
    DL_FOREACH(img->cameras, cam) {
        if (strcasecmp(cam->name, name) == 0) return true;
    }
    return false;
}

static void make_uniq_name(
        char *buf, int size, const char *base, void *user,
        bool (*name_exists)(void *user, const char *name))
{
    int i = 1, n, len;
    const char *ext;

    // If base if of the form 'abc.<num>' then we turn it into 'abc'
    len = strlen(base);
    ext = strrchr(base, '.');
    if (ext) {
        if (sscanf(ext, ".%d%*c", &n) == 1) {
            len -= strlen(ext);
            i = n;
        }
    }

    for (;; i++) {
        snprintf(buf, size, "%.*s.%d", len, base, i);
        if (!name_exists(user, buf)) break;
    }
}

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

static layer_t *layer_clone(layer_t *other)
{
    int len;
    layer_t *layer;

    assert(other);
    layer = calloc(1, sizeof(*layer));
    len = sizeof(layer->name) - 1 - strlen(" clone");
    snprintf(layer->name, sizeof(layer->name), "%.*s clone", len, other->name);
    layer->visible = other->visible;
    layer->material = other->material;
    layer->volume = volume_copy(other->volume);
    mat4_set_identity(layer->mat);
    layer->base_id = other->id;
    layer->base_volume_key = volume_get_key(other->volume);
    return layer;
}

// Make sure the layer volume is up to date.
void image_update(image_t *img)
{
    painter_t painter = {};
    uint32_t key;
    layer_t *layer, *base;

    DL_FOREACH(img->layers, layer) {
        base = img_get_layer(img, layer->base_id);
        if (base && layer->base_volume_key != volume_get_key(base->volume)) {
            volume_set(layer->volume, base->volume);
            volume_move(layer->volume, layer->mat);
            layer->base_volume_key = volume_get_key(base->volume);
        }
        if (layer->shape) {
            key = XXH32(layer->mat, sizeof(layer->mat), 0);
            key = XXH32(layer->shape, sizeof(layer->shape), key);
            key = XXH32(layer->color, sizeof(layer->color), key);
            if (key != layer->shape_key) {
                painter.mode = MODE_OVER;
                painter.shape = layer->shape;
                painter.box = &goxel.image->box;
                vec4_copy(layer->color, painter.color);
                volume_clear(layer->volume);
                volume_op(layer->volume, &painter, layer->mat);
                layer->shape_key = key;
            }
        }
    }
}

image_t *image_new(void)
{
    layer_t *layer;
    image_t *img = calloc(1, sizeof(*img));
    img->ref = 1;
    const int aabb[2][3] = {{-16, -16, 0}, {16, 16, 32}};
    bbox_from_aabb(img->box, aabb);
    img->export_width = 1024;
    img->export_height = 1024;
    image_add_material(img, NULL);
    image_add_camera(img, NULL);
    layer = image_add_layer(img, NULL);
    layer->visible = true;
    layer->id = img_get_new_id(img);
    layer->material = img->active_material;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    // Prevent saving an empty image.
    img->saved_key = image_get_key(img);
    image_history_push(img);
    return img;
}

static void image_restore(image_t *img, const image_t *snap)
{
    layer_t *layer, *snap_layer;
    camera_t *camera, *snap_camera;
    material_t *material, *snap_material;
    // Note: this would be easier if we has some generic object array
    // objects.

    // First remove all the layer, cameras, and materials.
    while ((layer = img->layers)) {
        DL_DELETE(img->layers, layer);
        layer_delete(layer);
    }
    while ((camera = img->cameras)) {
        DL_DELETE(img->cameras, camera);
        camera_delete(camera);
    }
    while ((material = img->materials)) {
        DL_DELETE(img->materials, material);
        material_delete(material);
    }

    // Set copy from the other.
    img->layers = NULL;
    img->active_layer = NULL;
    DL_FOREACH(snap->layers, snap_layer) {
        layer = layer_copy(snap_layer);
        DL_APPEND(img->layers, layer);
        if (snap_layer == snap->active_layer)
            img->active_layer = layer;
    }
    assert(img->active_layer);

    img->cameras = NULL;
    img->active_camera = NULL;
    DL_FOREACH(snap->cameras, snap_camera) {
        camera = camera_copy(snap_camera);
        DL_APPEND(img->cameras, camera);
        if (snap_camera == snap->active_camera)
            img->active_camera = camera;
    }

    img->materials = NULL;
    img->active_material = NULL;
    DL_FOREACH(snap->materials, snap_material) {
        material = material_copy(snap_material);
        DL_APPEND(img->materials, material);
        if (snap_material == snap->active_material)
            img->active_material = material;
        DL_FOREACH(img->layers, layer) {
            if (layer->material == snap_material)
                layer->material = material;
        }
    }

    // Copy other attributes.
    mat4_copy(snap->box, img->box);
    mat4_copy(snap->selection_box, img->selection_box);

    volume_delete(img->selection_mask);
    img->selection_mask = NULL;
    if (snap->selection_mask)
        img->selection_mask = volume_copy(snap->selection_mask);
}

/*
 * Generate a copy of the image that can be put into the history.
 */
static image_t *image_snapshot(const image_t *other)
{
    image_t *img;
    layer_t *layer, *other_layer;
    camera_t *camera, *other_camera;
    material_t *material, *other_material;

    img = calloc(1, sizeof(*img));
    mat4_copy(other->box, img->box);
    mat4_copy(other->selection_box, img->selection_box);

    DL_FOREACH(other->layers, other_layer) {
        layer = layer_copy(other_layer);
        DL_APPEND(img->layers, layer);
        if (other_layer == other->active_layer)
            img->active_layer = layer;
    }
    assert(img->active_layer);

    DL_FOREACH(other->cameras, other_camera) {
        camera = camera_copy(other_camera);
        DL_APPEND(img->cameras, camera);
        if (other_camera == other->active_camera)
            img->active_camera = camera;
    }

    DL_FOREACH(other->materials, other_material) {
        material = material_copy(other_material);
        DL_APPEND(img->materials, material);
        if (other_material == other->active_material)
            img->active_material = material;
        DL_FOREACH(img->layers, layer) {
            if (layer->material == other_material)
                layer->material = material;
        }
    }

    if (other->selection_mask)
        img->selection_mask = volume_copy(other->selection_mask);

    return img;
}


void image_delete(image_t *img)
{
    image_t *snap, *snap_tmp;
    camera_t *cam;
    layer_t *layer;
    material_t *mat;

    if (!img) return;
    if (--img->ref > 0) return;

    while ((layer = img->layers)) {
        DL_DELETE(img->layers, layer);
        layer_delete(layer);
    }
    while ((cam = img->cameras)) {
        DL_DELETE(img->cameras, cam);
        camera_delete(cam);
    }
    while ((mat = img->materials)) {
        DL_DELETE(img->materials, mat);
        material_delete(mat);
    }

    free(img->path);
    free(img->export_path);

    DL_FOREACH_SAFE2 (img->history, snap, snap_tmp, history_next) {
        DL_DELETE2(img->history, snap, history_prev, history_next);
        assert(snap->ref <= 1);
        image_delete(snap);
    }

    free(img);
}

layer_t *image_add_layer(image_t *img, layer_t *layer)
{
    assert(img);
    if (!layer) {
        layer = layer_new(NULL);
        make_uniq_name(layer->name, sizeof(layer->name), "Layer", img,
                       layer_name_exists);
    }
    layer->visible = true;
    layer->id = img_get_new_id(img);
    layer->material = img->active_material;
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

layer_t *image_add_shape_layer(image_t *img)
{
    layer_t *layer;
    assert(img);
    layer = layer_new("shape");
    layer->visible = true;
    layer->shape = &shape_sphere;
    vec4_copy(goxel.painter.color, layer->color);
    // If the selection is on use it, otherwise center it in the image.
    if (!box_is_null(img->selection_box)) {
        mat4_copy(img->selection_box, layer->mat);
    } else {
        vec3_copy(img->box[3], layer->mat[3]);
        mat4_iscale(layer->mat, 4, 4, 4);
    }
    layer->id = img_get_new_id(img);
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_delete_layer(image_t *img, layer_t *layer)
{
    layer_t *other;
    assert(img);
    assert(layer);
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
        layer = layer_new("unnamed");
        layer->visible = true;
        layer->id = img_get_new_id(img);
        DL_APPEND(img->layers, layer);
    }
    if (!img->active_layer) img->active_layer = img->layers->prev;
}

static void image_move_layer(image_t *img, layer_t *layer, int d)
{
    layer_t *other = NULL;

    assert(img);
    assert(layer);
    assert(d == -1 || d == +1);
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
    assert(img);
    assert(other);
    layer = layer_copy(other);
    make_uniq_name(layer->name, sizeof(layer->name), other->name, img,
                   layer_name_exists);
    layer->visible = true;
    layer->id = img_get_new_id(img);
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

layer_t *image_clone_layer(image_t *img, layer_t *other)
{
    layer_t *layer;
    img = img ?: goxel.image;
    other = other ?: img->active_layer;
    assert(img && other);
    layer = layer_clone(other);
    layer->visible = true;
    layer->id = img_get_new_id(img);
    DL_APPEND(img->layers, layer);
    img->active_layer = layer;
    return layer;
}

void image_unclone_layer(image_t *img, layer_t *layer)
{
    assert(img);
    assert(layer);
    layer->base_id = 0;
    layer->shape = NULL;
}

void image_merge_visible_layers(image_t *img)
{
    layer_t *layer, *other, *last = NULL;
    assert(img);
    DL_FOREACH(img->layers, layer) {
        if (!layer->visible) continue;
        image_unclone_layer(img, layer);

        if (last) {
            // Unclone all layers cloned from this one.
            DL_FOREACH(goxel.image->layers, other) {
                if (other->base_id == last->id) {
                    other->base_id = 0;
                }
            }
            SWAP(layer->volume, last->volume);
            volume_merge(layer->volume, last->volume, layer->mode, NULL);
            DL_DELETE(img->layers, last);
            layer_delete(last);
        }
        last = layer;
    }
    if (last) img->active_layer = last;
}

void image_merge_layer_down(image_t *img, layer_t *layer)
{
    layer_t *target, *other;
    assert(img);
    assert(layer);

    if (layer == img->layers) return; // First layer.
    target = layer->prev;

    image_unclone_layer(img, layer);
    image_unclone_layer(img, target);

    // Unclone all layers cloned from this one.
    DL_FOREACH(goxel.image->layers, other) {
        if (other->base_id == layer->id) {
            other->base_id = 0;
        }
    }

    volume_merge(target->volume, layer->volume, layer->mode, NULL);
    DL_DELETE(img->layers, layer);
    layer_delete(layer);
    img->active_layer = target;
}

camera_t *image_add_camera(image_t *img, camera_t *cam)
{
    assert(img);
    if (!cam) {
        cam = camera_new(NULL);
        make_uniq_name(cam->name, sizeof(cam->name), "Camera", img,
                       camera_name_exists);
    }
    DL_APPEND(img->cameras, cam);
    img->active_camera = cam;
    return cam;
}

void image_delete_camera(image_t *img, camera_t *cam)
{
    img = img ?: goxel.image;
    cam = cam ?: img->active_camera;
    if (!cam) return;
    DL_DELETE(img->cameras, cam);
    if (cam == img->active_camera)
        img->active_camera = img->cameras;
    camera_delete(cam);
}

static void image_move_camera(image_t *img, camera_t *cam, int d)
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

material_t *image_add_material(image_t *img, material_t *mat)
{
    img = img ?: goxel.image;
    if (!mat) {
        mat = material_new(NULL);
        make_uniq_name(mat->name, sizeof(mat->name), "Material", img,
                       material_name_exists);
    }
    assert(!mat->prev);
    DL_APPEND(img->materials, mat);
    img->active_material = mat;
    return mat;
}

void image_delete_material(image_t *img, material_t *mat)
{
    layer_t *layer;

    img = img ?: goxel.image;
    mat = mat ?: img->active_material;
    if (!mat) return;
    DL_DELETE(img->materials, mat);
    if (mat == img->active_material) img->active_material = NULL;
    material_delete(mat);
    DL_FOREACH(img->layers, layer)
        if (layer->material == mat) layer->material = NULL;
}

void image_auto_resize(image_t *img)
{
    float box[4][4] = {}, layer_box[4][4];
    layer_t *layer;
    DL_FOREACH(img->layers, layer) {
        layer_get_bounding_box(layer, layer_box);
        box_union(box, layer_box, box);
    }
    mat4_copy(box, img->box);
}

static void a_image_auto_resize(void)
{
    image_auto_resize(goxel.image);
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
void debug_print_history(image_t *img)
{
    int i = 0;

    image_t *hist;
    DL_FOREACH2(img->history, hist, history_next) {
        assert(hist != img);
        printf("%d%s  ", i++, hist == img->history_pos ? "*" : " ");
    }
    printf("\n");
}
#else
static void debug_print_history(image_t *img) {}
#endif

void image_history_push(image_t *img)
{
    image_t *snap;

    // Don't do anything if the image didn't actually changed.
    if (img->history_pos) {
        if (image_get_key(img) == image_get_key(img->history_pos)) {
            return;
        }
    }

    // Discard previous undo.
    if (img->history_pos) {
        while (img->history_pos->history_next) {
            snap = img->history_pos->history_next;
            DL_DELETE2(img->history, snap, history_prev, history_next);
            debug_print_history(img);
        }
    }

    snap = image_snapshot(img);
    DL_APPEND2(img->history, snap, history_prev, history_next);
    img->history_pos = snap;
    debug_print_history(img);
}

void image_history_resize(image_t *img, int size)
{
    int i, nb = 0;
    image_t *hist;
    layer_t *layer, *layer_tmp;

    // First cound the size of the history to compute how many we are going
    // to remove.
    for (hist = img->history; hist != img; hist = hist->history_next) nb++;
    nb = max(0, nb - size);
    for (i = 0; i < nb; i++) {
        hist = img->history;

        // XXX: do that in a function!
        DL_FOREACH_SAFE(hist->layers, layer, layer_tmp) {
            assert(layer);
            DL_DELETE(hist->layers, layer);
            layer_delete(layer);
        }
        DL_DELETE2(img->history, hist, history_prev, history_next);
        free(hist);
    }
}

void image_undo(image_t *img)
{
    camera_t camera = {};
    image_t *prev;
    if (img->history_pos == img->history) {
        LOG_D("No more undo");
        debug_print_history(img);
        return;
    }
    assert(img->history_pos);
    if (img->active_camera) camera = *img->active_camera;
    prev = img->history_pos->history_prev;
    image_restore(img, prev);
    img->history_pos = prev;

    // Don't move the camera for an undo.
    if (img->active_camera &&
            strcmp(img->active_camera->name, camera.name) == 0) {
        camera_set(img->active_camera, &camera);
    }

    debug_print_history(img);
}

void image_redo(image_t *img)
{
    image_t *next;
    if (!img->history_pos || !img->history_pos->history_next) {
        LOG_D("No more redo");
        return;
    }
    next = img->history_pos->history_next;
    image_restore(img, next);
    img->history_pos = next;
    debug_print_history(img);
}

static void image_clear_layer(void)
{
    painter_t painter;
    image_t *img = goxel.image;
    layer_t *layer = img->active_layer;
    if (    box_is_null(img->selection_box) &&
            volume_is_empty(img->selection_mask)) {
        volume_clear(layer->volume);
        return;
    }

    // Use the mask in priority if it exists.
    if (!volume_is_empty(img->selection_mask)) {
        volume_merge(layer->volume, img->selection_mask, MODE_SUB, NULL);
        return;
    }

    painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_SUB,
        .color = {255, 255, 255, 255},
    };
    volume_op(layer->volume, &painter, img->selection_box);
}

bool image_layer_can_edit(const image_t *img, const layer_t *layer)
{
    return !layer->base_id && !layer->image && !layer->shape;
}

/*
 * Function: image_get_key
 * Return a value that is garantied to change when the image change.
 */
uint32_t image_get_key(const image_t *img)
{
    uint32_t key = 0, k;
    layer_t *layer;
    camera_t *camera;
    material_t *material;

    DL_FOREACH(img->layers, layer) {
        k = layer_get_key(layer);
        key = XXH32(&k, sizeof(k), key);
    }
    DL_FOREACH(img->cameras, camera) {
        k = camera_get_key(camera);
        key = XXH32(&k, sizeof(k), key);
    }
    DL_FOREACH(img->materials, material) {
        k = material_get_hash(material);
        key = XXH32(&k, sizeof(k), key);
    }
    key = XXH32(img->selection_box, sizeof(img->selection_box), key);
    k = volume_get_key(img->selection_mask);
    key = XXH32(&k, sizeof(k), key);
    return key;
}

bool image_is_empty(const image_t *img)
{
    layer_t *layer;
    DL_FOREACH(img->layers, layer) {
        if (!volume_is_empty(layer->volume)) return false;
    }
    return true;
}

/*
 * Turn an image layer into a volume of 1 voxel depth.
 */
static void image_image_layer_to_volume(image_t *img, layer_t *layer)
{
    uint8_t *data;
    int x, y, w, h, bpp = 0, pos[3];
    uint8_t c[4];
    float p[3];
    assert(img);
    assert(layer);
    volume_accessor_t acc;

    image_history_push(img);
    data = img_read(layer->image->path, &w, &h, &bpp);
    acc = volume_get_accessor(layer->volume);
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        vec3_set(p, (x / (float)w) - 0.5, - ((y + 1) / (float)h) + 0.5, 0);
        mat4_mul_vec3(layer->mat, p, p);
        pos[0] = round(p[0]);
        pos[1] = round(p[1]);
        pos[2] = round(p[2]);
        memset(c, 0, 4);
        c[3] = 255;
        memcpy(c, data + (y * w + x) * bpp, bpp);
        volume_set_at(layer->volume, &acc, pos, c);
    }
    texture_delete(layer->image);
    layer->image = NULL;
    free(data);
}

ACTION_REGISTER(ACTION_layer_clear,
    .cfunc = image_clear_layer,
    .icon = ICON_DELETE,
    .flags = ACTION_TOUCH_IMAGE,
    .default_shortcut = "Delete",
)

static void a_image_add_layer(void)
{
    image_add_layer(goxel.image, NULL);
}

ACTION_REGISTER(ACTION_img_new_layer,
    .cfunc = a_image_add_layer,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ADD,
)

static void a_image_delete_layer(void)
{
    image_delete_layer(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_del_layer,
    .cfunc = a_image_delete_layer,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_REMOVE,
)

static void a_image_move_layer_up(void)
{
    image_move_layer(goxel.image, goxel.image->active_layer, +1);
}

static void a_image_move_layer_down(void)
{
    image_move_layer(goxel.image, goxel.image->active_layer, -1);
}


ACTION_REGISTER(ACTION_img_move_layer_up,
    .cfunc = a_image_move_layer_up,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_UPWARD,
)

ACTION_REGISTER(ACTION_img_move_layer_down,
    .cfunc = a_image_move_layer_down,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_DOWNWARD,
)

static void a_image_duplicate_layer(void)
{
    image_duplicate_layer(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_duplicate_layer,
    .cfunc = a_image_duplicate_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_image_clone_layer(void)
{
    image_clone_layer(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_clone_layer,
    .help = N_("Creates a linked copy of the selected layer"),
    .cfunc = a_image_clone_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_image_unclone_layer(void)
{
    image_unclone_layer(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_unclone_layer,
    .help = N_("Unlinks the selected layer from its parent"),
    .cfunc = a_image_unclone_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_select_parent_layer(void)
{
    image_t *image = goxel.image;
    image->active_layer = img_get_layer(image, image->active_layer->base_id);
}


ACTION_REGISTER(ACTION_img_select_parent_layer,
    .cfunc = a_img_select_parent_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_merge_visible_layers(void)
{
    image_merge_visible_layers(goxel.image);
}

ACTION_REGISTER(ACTION_img_merge_visible_layers,
    .cfunc = a_img_merge_visible_layers,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_merge_layer_down(void)
{
    image_merge_layer_down(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_merge_layer_down,
    .cfunc = a_img_merge_layer_down,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_new_camera(void)
{
    image_add_camera(goxel.image, NULL);
}

ACTION_REGISTER(ACTION_img_new_camera,
    .cfunc = a_img_new_camera,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ADD,
)

static void a_img_del_camera(void)
{
    image_delete_camera(goxel.image, goxel.image->active_camera);
}

ACTION_REGISTER(ACTION_img_del_camera,
    .cfunc = a_img_del_camera,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_REMOVE,
)

static void a_img_move_camera_up(void)
{
    image_move_camera(goxel.image, goxel.image->active_camera, +1);
}

static void a_img_move_camera_down(void)
{
    image_move_camera(goxel.image, goxel.image->active_camera, -1);
}

ACTION_REGISTER(ACTION_img_move_camera_up,
    .cfunc = a_img_move_camera_up,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_UPWARD,
)

ACTION_REGISTER(ACTION_img_move_camera_down,
    .cfunc = a_img_move_camera_down,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ARROW_DOWNWARD,
)

static void a_img_image_layer_to_volume(void)
{
    image_image_layer_to_volume(goxel.image, goxel.image->active_layer);
}

ACTION_REGISTER(ACTION_img_image_layer_to_volume,
    .help = N_("Converts a layer into a volume"),
    .cfunc = a_img_image_layer_to_volume,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_new_shape_layer(void)
{
    image_add_shape_layer(goxel.image);
}

ACTION_REGISTER(ACTION_img_new_shape_layer,
    .help = N_("Creates a shape layer"),
    .cfunc = a_img_new_shape_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_img_new_material(void)
{
    image_add_material(goxel.image, NULL);
}

ACTION_REGISTER(ACTION_img_new_material,
    .cfunc = a_img_new_material,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_ADD,
)

static void a_img_del_material(void)
{
    image_delete_material(goxel.image, goxel.image->active_material);
}

ACTION_REGISTER(ACTION_img_del_material,
    .cfunc = a_img_del_material,
    .flags = ACTION_TOUCH_IMAGE,
    .icon = ICON_REMOVE,
)

ACTION_REGISTER(ACTION_img_auto_resize,
    .help = N_("Resizes the image to fit the layers"),
    .cfunc = a_image_auto_resize,
    .flags = ACTION_TOUCH_IMAGE,
)
