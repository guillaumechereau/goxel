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
#include <errno.h>

#define VERSION 2 // Current version of the file format.

/*
 * File format, version 2:
 *
 * This is inspired by the png format, where the file consists of a list of
 * chunks with different types.
 *
 *  4 bytes magic string        : "GOX "
 *  4 bytes version             : 2
 *  List of chunks:
 *      4 bytes: type
 *      4 bytes: data length
 *      n bytes: data
 *      4 bytes: CRC
 *
 *  The layer can end with a DICT:
 *      for each entry:
 *          4 byte : key size (0 = end of dict)
 *          n bytes: key
 *          4 bytes: value size
 *          n bytes: value
 *
 *  chunks types:
 *
 *  IMG : a dict of info:
 *      - box: the image gox.
 *
 *  PREV: a png image for preview.
 *
 *  BL16: a 16^3 block saved as a 64x64 png image.
 *
 *  LAYR: a layer:
 *      4 bytes: number of blocks.
 *      for each block:
 *          4 bytes: block index
 *          4 bytes: x
 *          4 bytes: y
 *          4 bytes: z
 *          4 bytes: 0
 *      [DICT]
 *
 *  CAMR: a camera:
 *      [DICT] containing the following entries:
 *          name: string
 *          dist: float
 *          rot: quaternion
 *          ofs: offset
 *          ortho: bool
 *
 */

// We create a hash table of all the blocks, so that blocks with the same
// ids get written only once.
typedef struct {
    UT_hash_handle  hh;
    void            *v;
    uint64_t        uid;
    int             index;
} block_hash_t;

#define CHUNK_BUFF_SIZE (1 << 20) // 1 MiB max buffer size!

// XXX: should be something in goxel.h
static const shape_t *SHAPES[] = {
    &shape_sphere,
    &shape_cube,
    &shape_cylinder,
};

typedef struct {
    char     type[4];
    int      length;
    uint32_t crc;
    char     *buffer;   // Used when writing.

    int      pos;
} chunk_t;

static void write_int32(FILE *out, int32_t v)
{
    fwrite((char*)&v, 4, 1, out);
}

static int32_t read_int32(FILE *in)
{
    int32_t v;
    if (fread(&v, 4, 1, in) != 1) {
        // XXX: use a better error mechanism!
        LOG_E("Error reading file");
    }
    return v;
}

static bool chunk_read_start(chunk_t *c, FILE *in)
{
    size_t r;
    memset(c, 0, sizeof(*c));
    r = fread(c->type, 4, 1, in);
    if (r == 0) return false; // eof.
    if (r != 1) LOG_E("Error reading file");
    c->length = read_int32(in);
    return true;
}

static void chunk_read(chunk_t *c, FILE *in, char *buff, int size)
{
    c->pos += size;
    assert(c->pos <= c->length);
    if (buff) {
        // XXX: use a better error mechanism!
        if (fread(buff, size, 1, in) != 1) LOG_E("Error reading file");
    } else {
        fseek(in, size, SEEK_CUR);
    }
}

static int32_t chunk_read_int32(chunk_t *c, FILE *in)
{
    int32_t v;
    chunk_read(c, in, (char*)&v, 4);
    return v;
}

static void chunk_read_finish(chunk_t *c, FILE *in)
{
    assert(c->pos == c->length);
    read_int32(in); // TODO: check crc.
}

static bool chunk_read_dict_value(chunk_t *c, FILE *in,
                                  char *key, char *value, int *value_size) {
    int size;
    assert(c->pos <= c->length);
    if (c->pos == c->length) return 0;
    size = chunk_read_int32(c, in);
    if (size == 0) return false;
    assert(size < 256);
    chunk_read(c, in, key, size);
    key[size] = '\0';
    size = chunk_read_int32(c, in);
    chunk_read(c, in, value, size);
    value[size] = '\0';
    *value_size = size;
    return true;
}

static void chunk_write_start(chunk_t *c, FILE *out, const char *type)
{
    memset(c, 0, sizeof(*c));
    assert(strlen(type) == 4);
    memcpy(c->type, type, 4);
    c->buffer = calloc(1, CHUNK_BUFF_SIZE);
}

static void chunk_write(chunk_t *c, FILE *out, const char *data, int size)
{
    if (size == 0) return;
    assert(c->length + size < CHUNK_BUFF_SIZE);
    memcpy(c->buffer + c->length, data, size);
    c->length += size;
}

static void chunk_write_int32(chunk_t *c, FILE *out, int32_t v)
{
    chunk_write(c, out, (char*)&v, 4);
}

static void chunk_write_dict_value(chunk_t *c, FILE *out, const char *name,
                                   const void *data, int size)
{
    chunk_write_int32(c, out, (int)strlen(name));
    chunk_write(c, out, name, (int)strlen(name));
    chunk_write_int32(c, out, size);
    chunk_write(c, out, data, size);
}

static void chunk_write_finish(chunk_t *c, FILE *out)
{
    fwrite(c->type, 4, 1, out);
    write_int32(out, c->length);
    fwrite(c->buffer, c->length, 1, out);
    write_int32(out, 0);        // CRC XXX: todo.
    free(c->buffer);
    c->buffer = NULL;
}

static void chunk_write_all(FILE *out, const char *type,
                            const char *data, int size)
{
    fwrite(type, 4, 1, out);
    write_int32(out, size);
    fwrite(data, size, 1, out);
    write_int32(out, 0);        // CRC XXX: todo.
}

void save_to_file(const image_t *img, const char *path, bool with_preview)
{
    // XXX: remove all empty blocks before saving.
    LOG_I("Save to %s", path);
    block_hash_t *blocks_table = NULL, *data, *data_tmp;
    layer_t *layer;
    chunk_t c;
    int nb_blocks, index, size, bpos[3];
    uint64_t uid;
    FILE *out;
    uint8_t *png, *preview;
    camera_t *camera;
    mesh_iterator_t iter;

    img = img ?: goxel.image;
    out = fopen(path, "wb");
    if (!out) {
        LOG_E("Cannot save to %s: %s", path, strerror(errno));
        return;
    }
    fwrite("GOX ", 4, 1, out);
    write_int32(out, VERSION);

    // Write image info.
    chunk_write_start(&c, out, "IMG ");
    if (!box_is_null(img->box))
        chunk_write_dict_value(&c, out, "box", &img->box, sizeof(img->box));
    chunk_write_finish(&c, out);

    if (with_preview) {
        preview = calloc(128 * 128, 4);
        goxel_render_to_buf(preview, 128, 128, 4);
        png = img_write_to_mem(preview, 128, 128, 4, &size);
        chunk_write_all(out, "PREV", (char*)png, size);
        free(preview);
        free(png);
    }

    // Add all the blocks data into the hash table.
    index = 0;
    DL_FOREACH(img->layers, layer) {
        iter = mesh_get_iterator(layer->mesh, MESH_ITER_BLOCKS);
        while (mesh_iter(&iter, bpos)) {
            mesh_get_block_data(layer->mesh, &iter, bpos, &uid);
            HASH_FIND(hh, blocks_table, &uid, sizeof(uid), data);
            if (data) continue;
            data = calloc(1, sizeof(*data));
            data->v = mesh_get_block_data(layer->mesh, &iter, bpos, NULL);
            assert(data->v);
            data->uid = uid;
            data->index = index++;
            HASH_ADD(hh, blocks_table, uid, sizeof(data->uid), data);
        }
    }

    // Write all the blocks chunks.
    HASH_ITER(hh, blocks_table, data, data_tmp) {
        png = img_write_to_mem((uint8_t*)data->v, 64, 64, 4, &size);
        chunk_write_all(out, "BL16", (char*)png, size);
        free(png);
    }

    // Write all the layers.
    DL_FOREACH(img->layers, layer) {
        chunk_write_start(&c, out, "LAYR");
        nb_blocks = 0;
        if (!layer->base_id && !layer->shape) {
            iter = mesh_get_iterator(layer->mesh, MESH_ITER_BLOCKS);
            while (mesh_iter(&iter, bpos)) {
                nb_blocks++;
            }
        }
        chunk_write_int32(&c, out, nb_blocks);
        if (!layer->base_id && !layer->shape) {
            iter = mesh_get_iterator(layer->mesh, MESH_ITER_BLOCKS);
            while (mesh_iter(&iter, bpos)) {
                mesh_get_block_data(layer->mesh, &iter, bpos, &uid);
                HASH_FIND(hh, blocks_table, &uid, sizeof(uid), data);
                assert(data);
                chunk_write_int32(&c, out, data->index);
                chunk_write_int32(&c, out, bpos[0]);
                chunk_write_int32(&c, out, bpos[1]);
                chunk_write_int32(&c, out, bpos[2]);
                chunk_write_int32(&c, out, 0);
            }
        }
        chunk_write_dict_value(&c, out, "name", layer->name,
                               strlen(layer->name));
        chunk_write_dict_value(&c, out, "mat", &layer->mat,
                               sizeof(layer->mat));
        chunk_write_dict_value(&c, out, "id", &layer->id,
                               sizeof(layer->id));
        chunk_write_dict_value(&c, out, "base_id", &layer->base_id,
                               sizeof(layer->base_id));
        if (layer->image) {
            chunk_write_dict_value(&c, out, "img-path", layer->image->path,
                               strlen(layer->image->path));
        }
        if (!box_is_null(layer->box))
            chunk_write_dict_value(&c, out, "box", &layer->box,
                                   sizeof(layer->box));
        if (layer->shape) {
            chunk_write_dict_value(&c, out, "shape", layer->shape->id,
                               strlen(layer->shape->id));
            chunk_write_dict_value(&c, out, "color", layer->color,
                                sizeof(layer->color));
        }
        chunk_write_dict_value(&c, out, "visible", &layer->visible,
                               sizeof(layer->visible));

        chunk_write_finish(&c, out);
    }

    // Write all the cameras.
    DL_FOREACH(img->cameras, camera) {
        chunk_write_start(&c, out, "CAMR");
        chunk_write_dict_value(&c, out, "name", camera->name,
                               strlen(camera->name));
        chunk_write_dict_value(&c, out, "dist", &camera->dist,
                               sizeof(camera->dist));
        chunk_write_dict_value(&c, out, "rot", &camera->rot,
                               sizeof(camera->rot));
        chunk_write_dict_value(&c, out, "ofs", &camera->ofs,
                               sizeof(camera->ofs));
        chunk_write_dict_value(&c, out, "ortho", &camera->ortho,
                               sizeof(camera->ortho));
        if (camera == img->active_camera)
            chunk_write_dict_value(&c, out, "active", NULL, 0);

        chunk_write_finish(&c, out);
    }

    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data);
    }

    fclose(out);
}

// Iter info of a gox file, without actually reading it.
// For the moment only returns the image preview if available.
int gox_iter_infos(const char *path,
                   int (*callback)(const char *attr, int size,
                                   void *value, void *user),
                   void *user)
{
    FILE *in;
    chunk_t c;
    uint8_t *png;
    char magic[4];

    in = fopen(path, "rb");

    if (fread(magic, 4, 1, in) != 1) return -1;
    if (strncmp(magic, "GOX ", 4) != 0) return -1;
    read_int32(in);

    while (chunk_read_start(&c, in)) {
        if (strncmp(c.type, "BL16", 4) == 0) break;
        if (strncmp(c.type, "LAYR", 4) == 0) break;
        if (strncmp(c.type, "PREV", 4) == 0) {
            png = calloc(1, c.length);
            chunk_read(&c, in, (char*)png, c.length);
            callback(c.type, c.length, png, user);
            free(png);
        } else {
            // Ignore other blocks.
            chunk_read(&c, in, NULL, c.length);
        }
        chunk_read_finish(&c, in);
    }
    fclose(in);
    return 0;
}


static block_hash_t *hash_find_at(block_hash_t *hash, int index)
{
    int i;
    for (i = 0; i < index; i++) {
        hash = hash->hh.next;
    }
    return hash;
}

int load_from_file(const char *path)
{
    layer_t *layer, *layer_tmp;
    block_hash_t *blocks_table = NULL, *data, *data_tmp;
    FILE *in;
    char magic[4] = {};
    uint8_t *voxel_data;
    int nb_blocks;
    int w, h, bpp;
    uint8_t *png;
    chunk_t c;
    int i, index, version, x, y, z;
    int  dict_value_size;
    char dict_key[256];
    char dict_value[256];
    uint64_t uid = 1;
    camera_t *camera;

    in = fopen(path, "rb");
    if (!in) return -1;

    if (fread(magic, 4, 1, in) != 1) goto error;
    if (strncmp(magic, "GOX ", 4) != 0) goto error;
    version = read_int32(in);
    if (version > VERSION) {
        LOG_W("Cannot open gox file version %d", version);
        goto error;
    }

    // Remove all layers.
    // XXX: we should load the image fully before deleting the current one.
    DL_FOREACH_SAFE(goxel.image->layers, layer, layer_tmp) {
        mesh_delete(layer->mesh);
        free(layer);
    }
    goxel.image->layers = NULL;
    memset(&goxel.image->box, 0, sizeof(goxel.image->box));

    while (chunk_read_start(&c, in)) {
        if (strncmp(c.type, "BL16", 4) == 0) {
            png = calloc(1, c.length);
            chunk_read(&c, in, (char*)png, c.length);
            bpp = 4;
            voxel_data = img_read_from_mem((void*)png, c.length, &w, &h, &bpp);
            assert(w == 64 && h == 64 && bpp == 4);
            data = calloc(1, sizeof(*data));
            data->v = calloc(1, 64 * 64 * 4);
            memcpy(data->v, voxel_data, 64 * 64 * 4);
            data->uid = ++uid;
            HASH_ADD(hh, blocks_table, uid, sizeof(data->uid), data);
            free(voxel_data);
            free(png);

        } else if (strncmp(c.type, "LAYR", 4) == 0) {
            layer = image_add_layer(goxel.image, NULL);
            nb_blocks = chunk_read_int32(&c, in);   assert(nb_blocks >= 0);
            for (i = 0; i < nb_blocks; i++) {
                index = chunk_read_int32(&c, in);   assert(index >= 0);
                x = chunk_read_int32(&c, in);
                y = chunk_read_int32(&c, in);
                z = chunk_read_int32(&c, in);
                if (version == 1) { // Previous version blocks pos.
                    x -= 8; y -= 8; z -= 8;
                }
                chunk_read_int32(&c, in);
                data = hash_find_at(blocks_table, index);
                assert(data);
                mesh_blit(layer->mesh, data->v, x, y, z, 16, 16, 16, NULL);
            }
            while ((chunk_read_dict_value(&c, in, dict_key, dict_value,
                                          &dict_value_size))) {
                if (strcmp(dict_key, "name") == 0)
                    sprintf(layer->name, "%s", dict_value);
                if (strcmp(dict_key, "mat") == 0) {
                    assert(dict_value_size == sizeof(layer->mat));
                    memcpy(&layer->mat, dict_value, dict_value_size);
                }
                if (strcmp(dict_key, "img-path") == 0) {
                    layer->image = texture_new_image(dict_value, TF_NEAREST);
                }
                if (strcmp(dict_key, "id") == 0) {
                    typeof(layer->id) id;
                    memcpy(&id, dict_value, dict_value_size);
                    if (id) layer->id = id;
                }
                if (strcmp(dict_key, "base_id") == 0)
                    memcpy(&layer->base_id, dict_value, dict_value_size);
                if (strcmp(dict_key, "box") == 0)
                    memcpy(&layer->box, dict_value, dict_value_size);

                if (strcmp(dict_key, "shape") == 0) {
                    for (i = 0; i < ARRAY_SIZE(SHAPES); i++) {
                        if (strcmp(SHAPES[i]->id, dict_value) == 0) {
                            layer->shape = SHAPES[i];
                            break;
                        }
                    }
                }
                if (strcmp(dict_key, "color") == 0) {
                    memcpy(layer->color, dict_value, dict_value_size);
                }
                if (strcmp(dict_key, "visible") == 0) {
                    memcpy(&layer->visible, dict_value, dict_value_size);
                }
            }
        } else if (strncmp(c.type, "CAMR", 4) == 0) {
            camera = camera_new("unnamed");
            DL_APPEND(goxel.image->cameras, camera);
            while ((chunk_read_dict_value(&c, in, dict_key, dict_value,
                                          &dict_value_size))) {
                if (strcmp(dict_key, "name") == 0)
                    strncpy(camera->name, dict_value, sizeof(camera->name));
                if (strcmp(dict_key, "dist") == 0)
                    memcpy(&camera->dist, dict_value, dict_value_size);
                if (strcmp(dict_key, "rot") == 0)
                    memcpy(&camera->rot, dict_value, dict_value_size);
                if (strcmp(dict_key, "ofs") == 0)
                    memcpy(&camera->ofs, dict_value, dict_value_size);
                if (strcmp(dict_key, "ortho") == 0)
                    memcpy(&camera->ortho, dict_value, dict_value_size);
                if (strcmp(dict_key, "active") == 0) {
                    goxel.image->active_camera = camera;
                    camera_set(&goxel.camera, camera);
                }
            }

        } else if (strncmp(c.type, "IMG ", 4) == 0) {
            while ((chunk_read_dict_value(&c, in, dict_key, dict_value,
                                          &dict_value_size))) {
                if (strcmp(dict_key, "box") == 0)
                    memcpy(&goxel.image->box, dict_value, dict_value_size);
            }
        } else {
            // Ignore other blocks.
            chunk_read(&c, in, NULL, c.length);
        }
        chunk_read_finish(&c, in);
    }

    // Free the block hash table.  We do not delete the block data because
    // they have been used by the meshes.
    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data->v);
        free(data);
    }

    goxel.image->path = strdup(path);
    goxel.image->saved_key = image_get_key(goxel.image);
    fclose(in);

    // Update plane, snap mask and camera pos not to confuse people.
    plane_from_vectors(goxel.plane, goxel.image->box[3],
                       VEC(1, 0, 0), VEC(0, 1, 0));
    if (box_is_null(goxel.image->box)) goxel.snap_mask |= SNAP_PLANE;
    camera_fit_box(&goxel.camera, goxel.image->box);

    return 0;

error:
    fclose(in);
    return -1;
}

static void action_open(const char *path)
{

    if (!path)
        path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "gox\0*.gox\0",
                                    NULL, NULL);
    if (!path) return;
    image_delete(goxel.image);
    goxel.image = image_new();
    load_from_file(path);
}

ACTION_REGISTER(open,
    .help = "Open an image",
    .cfunc = action_open,
    .csig = "vp",
    .default_shortcut = "Ctrl O",
)

static void save_as(const char *path, bool with_preview)
{
    if (!path) {
        path = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "gox\0*.gox\0",
                                    NULL, "untitled.gox");
        if (!path) return;
    }
    if (path != goxel.image->path) {
        free(goxel.image->path);
        goxel.image->path = strdup(path);
        goxel.image->saved_key = image_get_key(goxel.image);
    }
    save_to_file(goxel.image, goxel.image->path, with_preview);
}

ACTION_REGISTER(save_as,
    .help = "Save the image as",
    .cfunc = save_as,
    .csig = "vpi",
)

static void save(const char *path, bool with_preview)
{
    save_as(path ?: goxel.image->path, with_preview);
}

ACTION_REGISTER(save,
    .help = "Save the image",
    .cfunc = save,
    .csig = "vpi",
    .default_shortcut = "Ctrl S"
)
