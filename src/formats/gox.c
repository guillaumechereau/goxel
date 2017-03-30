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

#ifndef NO_ZLIB
#   include <zlib.h>
#else

// If we don't have zlib, we simulate the gz functions using normal file
// operations.
typedef FILE *gzFile;
static gzFile gzopen(const char *path, const char *mode) {
    return fopen(path, mode);
}
static void gzclose(gzFile file) {fclose(file);}
static size_t gzread(gzFile file, void *buff, size_t size) {
    return fread(buff, size, 1, file);
}
static size_t gzwrite(gzFile file, const void *buff, size_t size) {
    return fwrite(buff, size, 1, file);
}
static int gzeof(gzFile file) {return feof(file);}

#endif

/*
 * File format, version 1:
 *
 * This is inspired by the png format, where the file consists of a list of
 * chunks with different types.
 *
 *  4 bytes magic string        : "GOX "
 *  4 bytes version             : 1
 *  List of chunks:
 *      4 bytes: data length
 *      4 bytes: type
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
 *
 */

// We create a hash table of all the blocks data.
typedef struct {
    UT_hash_handle  hh;
    block_data_t    *v;
    int             index;
} block_hash_t;

#define CHUNK_BUFF_SIZE (1 << 20) // 1 MiB max buffer size!

typedef struct {
    char     type[4];
    int      length;
    uint32_t crc;
    char     *buffer;   // Used when writing.

    int      pos;
} chunk_t;

static void write_int32(gzFile out, int32_t v)
{
    gzwrite(out, (char*)&v, 4);
}

static int32_t read_int32(gzFile in)
{
    int32_t v;
    gzread(in, &v, 4);
    return v;
}

static bool chunk_read_start(chunk_t *c, gzFile in)
{
    memset(c, 0, sizeof(*c));
    gzread(in, c->type, 4);
    if (gzeof(in)) return false;
    c->length = read_int32(in);
    return true;
}

static void chunk_read(chunk_t *c, gzFile in, char *buff, int size)
{
    c->pos += size;
    assert(c->pos <= c->length);
    gzread(in, buff, size);
}

static int32_t chunk_read_int32(chunk_t *c, gzFile in)
{
    int32_t v;
    chunk_read(c, in, (char*)&v, 4);
    return v;
}

static void chunk_read_finish(chunk_t *c, gzFile in)
{
    assert(c->pos == c->length);
    read_int32(in); // TODO: check crc.
}

static bool chunk_read_dict_value(chunk_t *c, gzFile in,
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

static void chunk_write_start(chunk_t *c, gzFile out, const char *type)
{
    memset(c, 0, sizeof(*c));
    assert(strlen(type) == 4);
    memcpy(c->type, type, 4);
    c->buffer = calloc(1, CHUNK_BUFF_SIZE);
}

static void chunk_write(chunk_t *c, gzFile out, const char *data, int size)
{
    if (size == 0) return;
    assert(c->length + size < CHUNK_BUFF_SIZE);
    memcpy(c->buffer + c->length, data, size);
    c->length += size;
}

static void chunk_write_int32(chunk_t *c, gzFile out, int32_t v)
{
    chunk_write(c, out, (char*)&v, 4);
}

static void chunk_write_dict_value(chunk_t *c, gzFile out, const char *name,
                                   const void *data, int size)
{
    chunk_write_int32(c, out, (int)strlen(name));
    chunk_write(c, out, name, (int)strlen(name));
    chunk_write_int32(c, out, size);
    chunk_write(c, out, data, size);
}

static void chunk_write_finish(chunk_t *c, gzFile out)
{
    gzwrite(out, c->type, 4);
    write_int32(out, c->length);
    gzwrite(out, c->buffer, c->length);
    write_int32(out, 0);        // CRC XXX: todo.
    free(c->buffer);
    c->buffer = NULL;
}

static void chunk_write_all(gzFile out, const char *type,
                            const char *data, int size)
{
    gzwrite(out, type, 4);
    write_int32(out, size);
    gzwrite(out, data, size);
    write_int32(out, 0);        // CRC XXX: todo.
}

void save_to_file(goxel_t *goxel, const char *path)
{
    // XXX: remove all empty blocks before saving.
    LOG_I("Save to %s", path);
    block_hash_t *blocks_table = NULL, *data, *data_tmp;
    layer_t *layer;
    block_t *block;
    chunk_t c;
    int nb_blocks, index, size;
    gzFile out;
    uint8_t *png;
    camera_t *camera;

    out = gzopen(path, str_endswith(path, ".gz") ? "wb" : "wbT");
    gzwrite(out, "GOX ", 4);
    write_int32(out, 1);

    // Add all the blocks data into the hash table.
    index = 0;
    DL_FOREACH(goxel->image->layers, layer) {
        MESH_ITER_BLOCKS(layer->mesh, block) {
            HASH_FIND_PTR(blocks_table, &block->data, data);
            if (data) continue;
            data = calloc(1, sizeof(*data));
            data->v = block->data;
            data->index = index++;
            HASH_ADD_PTR(blocks_table, v, data);
        }
    }

    // Write all the blocks chunks.
    HASH_ITER(hh, blocks_table, data, data_tmp) {
        png = img_write_to_mem((uint8_t*)data->v->voxels, 64, 64, 4, &size);
        chunk_write_all(out, "BL16", (char*)png, size);
        free(png);
    }

    // Write all the layers.
    DL_FOREACH(goxel->image->layers, layer) {
        chunk_write_start(&c, out, "LAYR");
        nb_blocks = 0;
        nb_blocks = HASH_COUNT(layer->mesh->blocks);
        chunk_write_int32(&c, out, nb_blocks);
        MESH_ITER_BLOCKS(layer->mesh, block) {
            HASH_FIND_PTR(blocks_table, &block->data, data);
            chunk_write_int32(&c, out, data->index);
            chunk_write_int32(&c, out, block->pos.x);
            chunk_write_int32(&c, out, block->pos.y);
            chunk_write_int32(&c, out, block->pos.z);
            chunk_write_int32(&c, out, 0);
        }
        chunk_write_dict_value(&c, out, "name", layer->name,
                               strlen(layer->name));
        chunk_write_dict_value(&c, out, "mat", &layer->mat,
                               sizeof(layer->mat));
        if (layer->image) {
            chunk_write_dict_value(&c, out, "img-path", layer->image->path,
                               strlen(layer->image->path));
        }

        chunk_write_finish(&c, out);
    }

    // Write all the cameras.
    DL_FOREACH(goxel->image->cameras, camera) {
        chunk_write_start(&c, out, "CAMR");
        chunk_write_dict_value(&c, out, "name", camera->name,
                               strlen(camera->name));
        chunk_write_dict_value(&c, out, "dist", &camera->dist,
                               sizeof(camera->dist));
        chunk_write_dict_value(&c, out, "rot", &camera->rot,
                               sizeof(camera->rot));
        chunk_write_dict_value(&c, out, "ofs", &camera->ofs,
                               sizeof(camera->ofs));
        if (camera == goxel->image->active_camera)
            chunk_write_dict_value(&c, out, "active", NULL, 0);

        chunk_write_finish(&c, out);
    }

    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data);
    }

    gzclose(out);
}

static block_hash_t *hash_find_at(block_hash_t *hash, int index)
{
    int i;
    for (i = 0; i < index; i++) {
        hash = hash->hh.next;
    }
    return hash;
}

void load_from_file(goxel_t *goxel, const char *path)
{
    layer_t *layer, *layer_tmp;
    block_hash_t *blocks_table = NULL, *data, *data_tmp;
    gzFile in;
    char magic[4];
    uint8_t *voxel_data;
    int nb_blocks;
    int w, h, bpp;
    uint8_t *png;
    chunk_t c;
    int i, index, x, y, z;
    vec3i_t pos;
    int  dict_value_size;
    char dict_key[256];
    char dict_value[256];
    camera_t *camera;

    image_history_push(goxel->image);

    LOG_I("Load from file %s", path);
    in = gzopen(path, "rb");

    gzread(in, magic, 4);
    assert(strncmp(magic, "GOX ", 4) == 0);
    read_int32(in);

    // Remove all layers.
    // XXX: we should load the image fully before deleting the current one.
    DL_FOREACH_SAFE(goxel->image->layers, layer, layer_tmp) {
        mesh_delete(layer->mesh);
        free(layer);
    }
    goxel->image->layers = NULL;

    while (chunk_read_start(&c, in)) {
        if (strncmp(c.type, "BL16", 4) == 0) {
            png = calloc(1, c.length);
            chunk_read(&c, in, (char*)png, c.length);
            bpp = 4;
            voxel_data = img_read_from_mem((void*)png, c.length, &w, &h, &bpp);
            assert(w == 64 && h == 64 && bpp == 4);
            data = calloc(1, sizeof(*data));
            data->v = calloc(1, sizeof(*data->v));
            memcpy(data->v->voxels, voxel_data, sizeof(data->v->voxels));
            data->v->id = ++goxel->next_uid;
            HASH_ADD_PTR(blocks_table, v, data);
            free(voxel_data);
            free(png);

        } else if (strncmp(c.type, "LAYR", 4) == 0) {
            layer = calloc(1, sizeof(*layer));
            layer->mesh = mesh_new();
            layer->visible = true;
            DL_APPEND(goxel->image->layers, layer);
            goxel->image->active_layer = layer;

            nb_blocks = chunk_read_int32(&c, in);   assert(nb_blocks >= 0);
            for (i = 0; i < nb_blocks; i++) {
                index = chunk_read_int32(&c, in);   assert(index >= 0);
                x = chunk_read_int32(&c, in);
                y = chunk_read_int32(&c, in);
                z = chunk_read_int32(&c, in);
                chunk_read_int32(&c, in);
                data = hash_find_at(blocks_table, index);
                assert(data);
                pos = vec3i(x, y, z);
                mesh_add_block(layer->mesh, data->v, &pos);
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
                    layer->image = texture_new_image(dict_value);
                }
            }
        } else if (strncmp(c.type, "CAMR", 4) == 0) {
            camera = camera_new("unamed");
            DL_APPEND(goxel->image->cameras, camera);
            while ((chunk_read_dict_value(&c, in, dict_key, dict_value,
                                          &dict_value_size))) {
                if (strcmp(dict_key, "name") == 0)
                    sprintf(camera->name, "%s", dict_value);
                if (strcmp(dict_key, "dist") == 0)
                    memcpy(&camera->dist, dict_value, dict_value_size);
                if (strcmp(dict_key, "rot") == 0)
                    memcpy(&camera->rot, dict_value, dict_value_size);
                if (strcmp(dict_key, "ofs") == 0)
                    memcpy(&camera->ofs, dict_value, dict_value_size);
                if (strcmp(dict_key, "active") == 0) {
                    goxel->image->active_camera = camera;
                    camera_set(&goxel->camera, camera);
                }
            }

        } else assert(false);
        chunk_read_finish(&c, in);
    }

    // Free the block hash table.  We do not delete the block data because
    // they have been used by the meshes.
    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data);
    }

    goxel->image->path = strdup(path);
    goxel_update_meshes(goxel, -1);
    gzclose(in);
}

static void action_open(const char *path)
{

    if (!path)
        path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "gox\0*.gox\0",
                                    NULL, NULL);
    if (!path) return;
    load_from_file(goxel, path);
}

ACTION_REGISTER(open,
    .help = "Open an image",
    .cfunc = action_open,
    .csig = "vp",
    .shortcut = "Ctrl O",
)

static void save_as(const char *path)
{
    if (!path) {
        path = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "gox\0*.gox\0",
                                    NULL, "untitled.gox");
        if (!path) return;
    }
    if (path != goxel->image->path) {
        free(goxel->image->path);
        goxel->image->path = strdup(path);
    }
    save_to_file(goxel, goxel->image->path);
}

ACTION_REGISTER(save_as,
    .help = "Save the image as",
    .cfunc = save_as,
    .csig = "vp",
)

static void save(const char *path)
{
    save_as(path ?: goxel->image->path);
}

ACTION_REGISTER(save,
    .help = "Save the image",
    .cfunc = save,
    .csig = "vp",
    .shortcut = "Ctrl S"
)
