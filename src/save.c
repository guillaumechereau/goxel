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
 */

// We create a hash table of all the blocks data.
typedef struct {
    UT_hash_handle  hh;
    block_data_t    *v;
    int             index;
} block_hash_t;

typedef struct {
    long     length_pos; // File position where we have to write the length.
    int      length;
    uint32_t crc;

    int      pos;
    char     type[5];
} chunk_t;

static void write_int32(FILE *out, int32_t v)
{
    fwrite((char*)&v, 4, 1, out);
}

static int32_t read_int32(FILE *in)
{
    int32_t v;
    fread(&v, 4, 1, in);
    return v;
}

static bool chunk_read_start(chunk_t *c, FILE *in)
{
    memset(c, 0, sizeof(*c));
    fread(c->type, 4, 1, in);
    if (feof(in)) return false;
    c->length = read_int32(in);
    return true;
}

static void chunk_read(chunk_t *c, FILE *in, char *buff, int size)
{
    c->pos += size;
    assert(c->pos <= c->length);
    fread(buff, size, 1, in);
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

static int chunk_read_dict_value(chunk_t *c, FILE *in,
                                 char *key, char *value) {
    int size;
    assert(c->pos <= c->length);
    if (c->pos == c->length) return 0;
    size = chunk_read_int32(c, in);
    if (size == 0) return 0;
    assert(size < 256);
    chunk_read(c, in, key, size);
    key[size] = '\0';
    size = chunk_read_int32(c, in);
    chunk_read(c, in, value, size);
    value[size] = '\0';
    return size;
}

static void chunk_write_start(chunk_t *c, FILE *out, const char *type)
{
    memset(c, 0, sizeof(*c));
    assert(strlen(type) == 4);
    fwrite(type, 4, 1, out);
    c->length_pos = ftell(out);
    write_int32(out, 0); // Placeholder for the the length.
}

static void chunk_write(chunk_t *c, FILE *out, const char *data, int size)
{
    fwrite(data, size, 1, out);
    c->length += size;
}

static void chunk_write_int32(chunk_t *c, FILE *out, int32_t v)
{
    chunk_write(c, out, (char*)&v, 4);
}

static void chunk_write_dict_value(chunk_t *c, FILE *out, const char *name,
                                   char *data, int size)
{
    chunk_write_int32(c, out, strlen(name));
    chunk_write(c, out, name, strlen(name));
    chunk_write_int32(c, out, size);
    chunk_write(c, out, data, size);
}

static void chunk_write_finish(chunk_t *c, FILE *out)
{
    fseek(out, c->length_pos, SEEK_SET);
    write_int32(out, c->length);
    fseek(out, 0, SEEK_END);
    write_int32(out, 0);        // CRC XXX: todo.
}

static void chunk_write_all(FILE *out, const char *type,
                            const char *data, int size)
{
    chunk_t c;
    chunk_write_start(&c, out, type);
    chunk_write(&c, out, data, size);
    chunk_write_finish(&c, out);
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
    FILE *out;
    uint8_t *png;
    char *tmp_path = NULL;
    char *cmd;

    if (str_endswith(path, ".gz")) {
        asprintf(&tmp_path, "%s_XXXXXX", path);
        out = fdopen(mkstemp(tmp_path), "wb");
        LOG_I("Use tmp file %s", tmp_path);
    } else {
        out = fopen(path, "wb");
    }

    fwrite("GOX ", 4, 1, out);
    write_int32(out, 1);

    // Add all the blocks data into the hash table.
    index = 0;
    DL_FOREACH(goxel->image->layers, layer) {
        DL_FOREACH(layer->mesh->blocks, block) {
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
        DL_COUNT(layer->mesh->blocks, block, nb_blocks);
        chunk_write_int32(&c, out, nb_blocks);
        DL_FOREACH(layer->mesh->blocks, block) {
            HASH_FIND_PTR(blocks_table, &block->data, data);
            chunk_write_int32(&c, out, data->index);
            chunk_write_int32(&c, out, block->pos.x);
            chunk_write_int32(&c, out, block->pos.y);
            chunk_write_int32(&c, out, block->pos.z);
            chunk_write_int32(&c, out, 0);
        }
        chunk_write_dict_value(&c, out, "name", layer->name,
                               strlen(layer->name));
        chunk_write_finish(&c, out);
    }

    HASH_ITER(hh, blocks_table, data, data_tmp) {
        HASH_DEL(blocks_table, data);
        free(data);
    }

    fclose(out);
    if (tmp_path) {
        asprintf(&cmd, "gzip -c %s > %s", tmp_path, path);
        system(cmd);
        free(cmd);
        remove(tmp_path);
        free(tmp_path);
    }
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
    FILE *in;
    char magic[4];
    uint8_t *voxel_data;
    int nb_blocks;
    int w, h, bpp;
    uint8_t *png;
    chunk_t c;
    int i, index, x, y, z;
    vec3_t pos;
    int  dict_value_size;
    char dict_key[256];
    char dict_value[256];
    char *tmp_path = NULL;
    char *cmd;

    LOG_I("Load from file %s", path);
    if (str_endswith(path, ".gz")) {
        asprintf(&tmp_path, "%s_XXXXXX", path);
        fclose(fdopen(mkstemp(tmp_path), "rw"));
        asprintf(&cmd, "gzip -cd %s > %s", path, tmp_path);
        system(cmd);
        free(cmd);
        in = fopen(tmp_path, "rb");
    } else {
        in = fopen(path, "rb");
    }

    fread(magic, 4, 1, in);
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
            data->v->id = ++goxel->block_next_id;
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
                pos = vec3(x, y, z);
                mesh_add_block(layer->mesh, data->v, &pos);
            }
            while ((dict_value_size = chunk_read_dict_value(&c, in,
                                                dict_key, dict_value))) {
                if (strcmp(dict_key, "name") == 0)
                    sprintf(layer->name, "%s", dict_value);
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
    goxel_update_meshes(goxel, true);
    image_history_push(goxel->image);
    fclose(in);
    if (tmp_path) {
        remove(tmp_path);
        free(tmp_path);
    }
}
