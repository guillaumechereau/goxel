/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

// Magica voxel vox format support.

#include "goxel.h"
#include "file_format.h"
#include <limits.h>

static const uint32_t VOX_DEFAULT_PALETTE[256];

static inline void hexcolor(uint32_t v, uint8_t out[4])
{
    out[0] = (v >> 24) & 0xff;
    out[1] = (v >> 16) & 0xff;
    out[2] = (v >>  8) & 0xff;
    out[3] = (v >>  0) & 0xff;
}

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

#define FILE_ERROR(msg) do { LOG_E("%s", msg); goto error; } while (0);

// Import the old magica voxel file format:
//
// d, h, w, <data>, <palette>
static int vox_import_old(const char *path)
{
    FILE *file;
    int w, h, d, i;
    uint8_t *voxels;
    uint8_t (*palette)[4];
    uint8_t (*cube)[4];

    file = fopen(path, "rb");
    d = READ(uint32_t, file);
    h = READ(uint32_t, file);
    w = READ(uint32_t, file);

    voxels = calloc(w * h * d, 1);
    palette = calloc(256, sizeof(*palette));
    cube = calloc(w * h * d, sizeof(*cube));
    for (i = 0; i < w * h * d; i++) {
        voxels[i] = READ(uint8_t, file);
    }
    for (i = 0; i < 256; i++) {
        palette[i][0] = READ(uint8_t, file);
        palette[i][1] = READ(uint8_t, file);
        palette[i][2] = READ(uint8_t, file);
        palette[i][3] = 255;
    }
    memset(palette[255], 0, 4);

    for (i = 0; i < w * h * d; i++) {
        if (voxels[i] == 255) continue;
        memcpy(cube[i], palette[voxels[i]], 4);
    }

    mesh_blit(goxel.image->active_layer->mesh, (uint8_t*)cube,
              -w / 2, -h / 2, -d / 2, w, h, d, NULL);
    free(palette);
    free(voxels);
    free(cube);
    fclose(file);
    return 0;
}

typedef struct node node_t;
struct node {
    node_t *children;
    node_t *next, *prev;

    char id[4];
    node_t *parent; // Logical parent.

    int node_id;
    int nb_children;
    int *children_ids;

    union {
        struct {
            int w, h, d;
        } size;
        struct {
            uint8_t (*values)[4];
        } rgba;
        struct {
            int nb;
            uint8_t *values;
        } xyzi;
        struct {
            int id;
            int child_id;
            int nb_frames;
            int tr[3];
            bool has_rot;
            int rot[3][3];
            bool has_trans;
            int trans[3];
        } ntrn;
        struct {
            int id;
            int nb_models;
            int model_id;
        } nshp;
    };
};

static int read_string(FILE *file, char **out)
{
    int size, r;
    size = READ(int32_t, file);
    *out = calloc(size + 1, 1);
    r = fread(*out, 1, size, file);
    if (r != size) {
        return -1;
    }
    return size;
}

static void read_dict(FILE *file, void *user,
                      void (*callback)(void *user, const char *key, int size,
                                       const char *value))
{
    int nb, i, size;
    char *key, *value;
    nb = READ(int32_t, file);
    for (i = 0; i < nb; i++) {
        read_string(file, &key);
        size = read_string(file, &value);
        if (callback) callback(user, key, size, value);
        free(key);
        free(value);
    }
}

static void on_trn_dict(void *user, const char *key, int size,
                        const char *value)
{
    node_t *node = user;
    int v, x, y, z;
    if (strcmp(key, "_r") == 0) {
        node->ntrn.has_rot = true;
        v = atoi(value);
        x = (v >> 0) & 3;
        y = (v >> 2) & 3;
        z = 3 - x - y;
        node->ntrn.rot[x][0] = ((v >> 4) & 1) ? -1 : +1;
        node->ntrn.rot[y][1] = ((v >> 5) & 1) ? -1 : +1;
        node->ntrn.rot[z][2] = ((v >> 6) & 1) ? -1 : +1;
    }
    if (strcmp(key, "_t") == 0) {
        node->ntrn.has_trans = true;
        sscanf(value, "%d %d %d", &x, &y, &z);
        node->ntrn.trans[0] = x;
        node->ntrn.trans[1] = y;
        node->ntrn.trans[2] = z;
    }
}

static bool node_is(const node_t *node, const char *id)
{
    return strncmp(node->id, id, 4) == 0;
}

static void free_node(node_t *node)
{
    node_t *child, *tmp;

    if (node_is(node, "RGBA"))
        free(node->rgba.values);
    if (node_is(node, "XYZI"))
        free(node->xyzi.values);

    DL_FOREACH_SAFE(node->children, child, tmp) {
        DL_DELETE(node->children, child);
        free_node(child);
    }

    free(node->children_ids);
    free(node);
}

static node_t *read_node(FILE *file)
{
    int i, r, size, children_size;
    node_t *node, *child, *child2;
    long fpos;

    node = calloc(1, sizeof(*node));
    node->node_id = -1;
    r = fread(node->id, 1, 4, file);
    if (r != 4) goto error;

    size = READ(uint32_t, file);
    children_size = READ(uint32_t, file);

    fpos = ftell(file);

    if (strncmp(node->id, "MAIN", 4) == 0) {
        // Nothing to do.
    }
    else if (strncmp(node->id, "SIZE", 4) == 0) {
        node->size.w = READ(uint32_t, file);
        node->size.h = READ(uint32_t, file);
        node->size.d = READ(uint32_t, file);
    }
    else if (strncmp(node->id, "RGBA", 4) == 0) {
        node->rgba.values = malloc(4 * 256);
        for (i = 1; i < 256; i++) {
            node->rgba.values[i][0] = READ(uint8_t, file);
            node->rgba.values[i][1] = READ(uint8_t, file);
            node->rgba.values[i][2] = READ(uint8_t, file);
            node->rgba.values[i][3] = READ(uint8_t, file);
        }
        // Skip the last value!
        for (i = 0; i < 4; i++) READ(uint8_t, file);
    }
    else if (strncmp(node->id, "XYZI", 4) == 0) {
        node->xyzi.nb = READ(uint32_t, file);
        node->xyzi.values = calloc(node->xyzi.nb, 4);
        for (i = 0; i < node->xyzi.nb; i++) {
            node->xyzi.values[i * 4 + 0] = READ(uint8_t, file);
            node->xyzi.values[i * 4 + 1] = READ(uint8_t, file);
            node->xyzi.values[i * 4 + 2] = READ(uint8_t, file);
            node->xyzi.values[i * 4 + 3] = READ(uint8_t, file);
        }
    }
    else if (strncmp(node->id, "nTRN", 4) == 0) {
        node->node_id = READ(int32_t, file);
        read_dict(file, NULL, NULL);
        node->nb_children = 1;
        node->children_ids = calloc(1, sizeof(int));
        *node->children_ids = READ(int32_t, file);
        READ(int32_t, file);
        READ(int32_t, file);
        node->ntrn.nb_frames = READ(int32_t, file);
        for (i = 0; i < node->ntrn.nb_frames; i++) {
            read_dict(file, node, on_trn_dict);
        }
    }
    else if (strncmp(node->id, "nSHP", 4) == 0) {
        node->node_id = READ(int32_t, file);
        read_dict(file, NULL, NULL);
        node->nshp.nb_models = READ(int32_t, file);
        for (i = 0; i < node->nshp.nb_models; i++) {
            node->nshp.model_id = READ(int32_t, file);
            read_dict(file, NULL, NULL);
        }
    }
    else if (strncmp(node->id, "nGRP", 4) == 0) {
        node->node_id = READ(int32_t, file);
        read_dict(file, NULL, NULL);
        node->nb_children = READ(int32_t, file);
        node->children_ids = calloc(node->nb_children, sizeof(int));
        for (i = 0; i < node->nb_children; i++) {
            node->children_ids[i] = READ(int32_t, file);
        }
    }

    if (ftell(file) < fpos + size) {
        fseek(file, fpos + size, SEEK_SET);
    }

    while (ftell(file) < fpos + size + children_size) {
        child = read_node(file);
        if (!child) continue;
        DL_APPEND(node->children, child);
    }


    // Set the parents.
    DL_FOREACH(node->children, child) {
        for (i = 0; i < child->nb_children; i++) {
            DL_FOREACH(node->children, child2) {
                if (child2->node_id == child->children_ids[i]) {
                    child2->parent = child;
                }
            }
        }
    }

    return node;
error:
    free_node(node);
    return NULL;
}

static const node_t *tree_find_shape(const node_t *tree, int model_id)
{
    const node_t *child, *ret;
    DL_FOREACH(tree->children, child) {
        if (node_is(child, "nSHP") && child->nshp.model_id == model_id)
            return child;
        ret = tree_find_shape(child, model_id);
        if (ret) return ret;
    }
    return NULL;
}

static void node_apply_mat(const node_t *node, float mat[4][4])
{
    float rot[4][4] = MAT4_IDENTITY;
    int i, j;

    if (node->parent) {
        node_apply_mat(node->parent, mat);
    }

    if (node_is(node, "nTRN") && node->ntrn.has_trans) {
        mat4_itranslate(mat, node->ntrn.trans[0],
                             node->ntrn.trans[1],
                             node->ntrn.trans[2]);
    }
    if (node_is(node, "nTRN") && node->ntrn.has_rot) {
        for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            rot[i][j] = node->ntrn.rot[i][j];
        }
        mat4_imul(mat, rot);
    }
}

static int import_layer(image_t *image,
                        const node_t *size, const node_t *xyzi,
                        const node_t *rgba, const node_t *tree,
                        int model_id)
{
    int i, x, y, z, c, pos[3];
    layer_t *layer;
    uint8_t color[4];
    mesh_iterator_t iter = {0};
    const node_t *shape;
    float mat[4][4] = MAT4_IDENTITY;

    // Use the current layer for first shape, then create new layers.
    if (size == tree->children)
        layer = image->active_layer;
    else
        layer = image_add_layer(image, NULL);

    for (i = 0; i < xyzi->xyzi.nb; i++) {
        x = xyzi->xyzi.values[i * 4 + 0];
        y = xyzi->xyzi.values[i * 4 + 1];
        z = xyzi->xyzi.values[i * 4 + 2];
        c = xyzi->xyzi.values[i * 4 + 3];
        pos[0] = x - size->size.w / 2;
        pos[1] = y - size->size.h / 2;
        pos[2] = z - size->size.d / 2;
        if (!c) continue; // Not sure what c == 0 means.
        if (rgba)
            memcpy(color, rgba->rgba.values[c], 4);
        else
            hexcolor(VOX_DEFAULT_PALETTE[c], color);
        mesh_set_at(layer->mesh, &iter, pos, color);
    }

    // Apply the transformation.
    // XXX: would be better to properly support layer transformations!
    shape = tree_find_shape(tree, model_id);
    if (shape) {
        node_apply_mat(shape, mat);
        mesh_move(layer->mesh, mat);
    }

    return 0;
}

static int vox_import(image_t *image, const char *path)
{
    FILE *file;
    char magic[4];
    int r, i, version;
    node_t *tree, *size_n, *xyzi_n, *rgba_n;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "vox\0*.vox\0",
                                        NULL, NULL);
    if (!path) return -1;
    file = fopen(path, "rb");
    r = fread(magic, 1, 4, file);
    if (r != 4) FILE_ERROR("Cannot read file");

    if (strncmp(magic, "VOX ", 4) != 0) {
        LOG_D("Old style magica voxel file");
        fclose(file);
        return vox_import_old(path);
    }

    if (strncmp(magic, "VOX ", 4) != 0) FILE_ERROR("Wrong magic string");
    version = READ(uint32_t, file);
    if (version != 150) LOG_W("Magica voxel file version %d!", version);
    tree = read_node(file);
    if (!tree) goto error;

    // Get the palette.
    DL_FOREACH(tree->children, rgba_n) {
        if (strncmp(rgba_n->id, "RGBA", 4) == 0) break;
    }

    // Create one layer for each ('size', 'xyzi') chunks in the main chunk.
    i = 0;
    DL_FOREACH(tree->children, size_n) {
        if (strncmp(size_n->id, "SIZE", 4) != 0) continue;
        xyzi_n = size_n->next;
        if (strncmp(xyzi_n->id, "XYZI", 4) != 0) continue;
        import_layer(image, size_n, xyzi_n, rgba_n, tree, i);
        i++;
    }

    free_node(tree);

    return 0;
error:
    return -1;
}

static int get_color_index(uint8_t v[4], uint8_t (*palette)[4], bool exact)
{
    const uint8_t *c;
    int i, dist, best = -1, best_dist = 1024;
    for (i = 1; i < 255; i++) {
        c = palette[i];
        dist = abs((int)c[0] - (int)v[0]) +
               abs((int)c[1] - (int)v[1]) +
               abs((int)c[2] - (int)v[2]);
        if (dist == 0) return i;
        if (exact) continue;
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return best;
}

static int voxel_cmp(const void *a_, const void *b_)
{
    const uint8_t *a = a_;
    const uint8_t *b = b_;
    if (a[2] != b[2]) return cmp(a[2], b[2]);
    if (a[1] != b[1]) return cmp(a[1], b[1]);
    if (a[0] != b[0]) return cmp(a[0], b[0]);
    return 0;
}

static int vox_export(const image_t *image, const char *path)
{
    FILE *file;
    int children_size, nb_vox = 0, i, pos[3];
    int xmin = INT_MAX, ymin = INT_MAX, zmin = INT_MAX;
    int xmax = INT_MIN, ymax = INT_MIN, zmax = INT_MIN;
    uint8_t (*palette)[4];
    bool use_default_palette = true;
    uint8_t *voxels;
    uint8_t v[4];
    mesh_iterator_t iter;
    const mesh_t *mesh;

    mesh = goxel_get_layers_mesh(image);
    palette = calloc(256, sizeof(*palette));
    for (i = 0; i < 256; i++)
        hexcolor(VOX_DEFAULT_PALETTE[i], palette[i]);

    // Iter all the voxels to get the count and the size.
    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, &iter, pos, v);
        if (v[3] < 127) continue;
        v[3] = 255;
        use_default_palette = use_default_palette &&
                            get_color_index(v, palette, true) != -1;
        nb_vox++;
        xmin = min(xmin, pos[0]);
        ymin = min(ymin, pos[1]);
        zmin = min(zmin, pos[2]);
        xmax = max(xmax, pos[0] + 1);
        ymax = max(ymax, pos[1] + 1);
        zmax = max(zmax, pos[2] + 1);
    }
    if (!use_default_palette)
        quantization_gen_palette(mesh, 255, (void*)(palette + 1));

    children_size = 12 + 4 * 3 +      // SIZE chunk
                    12 + 4 + 4 * nb_vox + // XYZI chunk
                    (use_default_palette ? 0 : (12 + 4 * 256)); // RGBA chunk.

    file = fopen(path, "wb");
    fprintf(file, "VOX ");
    WRITE(uint32_t, 150, file);     // Version
    fprintf(file, "MAIN");
    WRITE(uint32_t, 0, file);       // Main chunck size.
    WRITE(uint32_t, children_size, file);

    fprintf(file, "SIZE");
    WRITE(uint32_t, 4 * 3, file);
    WRITE(uint32_t, 0, file);
    WRITE(uint32_t, xmax - xmin, file);
    WRITE(uint32_t, ymax - ymin, file);
    WRITE(uint32_t, zmax - zmin, file);

    fprintf(file, "XYZI");
    WRITE(uint32_t, 4 * nb_vox + 4, file);
    WRITE(uint32_t, 0, file);
    WRITE(uint32_t, nb_vox, file);

    voxels = calloc(nb_vox, 4);
    i = 0;
    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, pos)) {
        mesh_get_at(mesh, &iter, pos, v);
        if (v[3] < 127) continue;
        pos[0] -= xmin;
        pos[1] -= ymin;
        pos[2] -= zmin;
        assert(pos[0] >= 0 && pos[0] < 255);
        assert(pos[1] >= 0 && pos[1] < 255);
        assert(pos[2] >= 0 && pos[2] < 255);

        voxels[i * 4 + 0] = pos[0];
        voxels[i * 4 + 1] = pos[1];
        voxels[i * 4 + 2] = pos[2];
        voxels[i * 4 + 3] = get_color_index(v, palette, false);
        i++;
    }
    qsort(voxels, nb_vox, 4, voxel_cmp);
    for (i = 0; i < nb_vox; i++)
        fwrite(voxels + i * 4, 4, 1, file);
    free(voxels);

    if (!use_default_palette) {
        fprintf(file, "RGBA");
        WRITE(uint32_t, 4 * 256, file);
        WRITE(uint32_t, 0, file);
        for (i = 1; i < 256; i++) {
            WRITE(uint8_t, palette[i][0], file);
            WRITE(uint8_t, palette[i][1], file);
            WRITE(uint8_t, palette[i][2], file);
            WRITE(uint8_t, palette[i][3], file);
        }
        WRITE(uint32_t, 0, file);
    }

    fclose(file);
    free(palette);
    return 0;
}

FILE_FORMAT_REGISTER(vox,
    .name = "Magica Voxel",
    .ext = "vox\0*.vox\0",
    .import_func = vox_import,
    .export_func = vox_export,
)


static const uint32_t VOX_DEFAULT_PALETTE[256] = {
    0x00000000, 0xffffffff, 0xffffccff, 0xffff99ff, 0xffff66ff, 0xffff33ff,
    0xffff00ff, 0xffccffff, 0xffccccff, 0xffcc99ff, 0xffcc66ff, 0xffcc33ff,
    0xffcc00ff, 0xff99ffff, 0xff99ccff, 0xff9999ff, 0xff9966ff, 0xff9933ff,
    0xff9900ff, 0xff66ffff, 0xff66ccff, 0xff6699ff, 0xff6666ff, 0xff6633ff,
    0xff6600ff, 0xff33ffff, 0xff33ccff, 0xff3399ff, 0xff3366ff, 0xff3333ff,
    0xff3300ff, 0xff00ffff, 0xff00ccff, 0xff0099ff, 0xff0066ff, 0xff0033ff,
    0xff0000ff, 0xccffffff, 0xccffccff, 0xccff99ff, 0xccff66ff, 0xccff33ff,
    0xccff00ff, 0xccccffff, 0xccccccff, 0xcccc99ff, 0xcccc66ff, 0xcccc33ff,
    0xcccc00ff, 0xcc99ffff, 0xcc99ccff, 0xcc9999ff, 0xcc9966ff, 0xcc9933ff,
    0xcc9900ff, 0xcc66ffff, 0xcc66ccff, 0xcc6699ff, 0xcc6666ff, 0xcc6633ff,
    0xcc6600ff, 0xcc33ffff, 0xcc33ccff, 0xcc3399ff, 0xcc3366ff, 0xcc3333ff,
    0xcc3300ff, 0xcc00ffff, 0xcc00ccff, 0xcc0099ff, 0xcc0066ff, 0xcc0033ff,
    0xcc0000ff, 0x99ffffff, 0x99ffccff, 0x99ff99ff, 0x99ff66ff, 0x99ff33ff,
    0x99ff00ff, 0x99ccffff, 0x99ccccff, 0x99cc99ff, 0x99cc66ff, 0x99cc33ff,
    0x99cc00ff, 0x9999ffff, 0x9999ccff, 0x999999ff, 0x999966ff, 0x999933ff,
    0x999900ff, 0x9966ffff, 0x9966ccff, 0x996699ff, 0x996666ff, 0x996633ff,
    0x996600ff, 0x9933ffff, 0x9933ccff, 0x993399ff, 0x993366ff, 0x993333ff,
    0x993300ff, 0x9900ffff, 0x9900ccff, 0x990099ff, 0x990066ff, 0x990033ff,
    0x990000ff, 0x66ffffff, 0x66ffccff, 0x66ff99ff, 0x66ff66ff, 0x66ff33ff,
    0x66ff00ff, 0x66ccffff, 0x66ccccff, 0x66cc99ff, 0x66cc66ff, 0x66cc33ff,
    0x66cc00ff, 0x6699ffff, 0x6699ccff, 0x669999ff, 0x669966ff, 0x669933ff,
    0x669900ff, 0x6666ffff, 0x6666ccff, 0x666699ff, 0x666666ff, 0x666633ff,
    0x666600ff, 0x6633ffff, 0x6633ccff, 0x663399ff, 0x663366ff, 0x663333ff,
    0x663300ff, 0x6600ffff, 0x6600ccff, 0x660099ff, 0x660066ff, 0x660033ff,
    0x660000ff, 0x33ffffff, 0x33ffccff, 0x33ff99ff, 0x33ff66ff, 0x33ff33ff,
    0x33ff00ff, 0x33ccffff, 0x33ccccff, 0x33cc99ff, 0x33cc66ff, 0x33cc33ff,
    0x33cc00ff, 0x3399ffff, 0x3399ccff, 0x339999ff, 0x339966ff, 0x339933ff,
    0x339900ff, 0x3366ffff, 0x3366ccff, 0x336699ff, 0x336666ff, 0x336633ff,
    0x336600ff, 0x3333ffff, 0x3333ccff, 0x333399ff, 0x333366ff, 0x333333ff,
    0x333300ff, 0x3300ffff, 0x3300ccff, 0x330099ff, 0x330066ff, 0x330033ff,
    0x330000ff, 0x00ffffff, 0x00ffccff, 0x00ff99ff, 0x00ff66ff, 0x00ff33ff,
    0x00ff00ff, 0x00ccffff, 0x00ccccff, 0x00cc99ff, 0x00cc66ff, 0x00cc33ff,
    0x00cc00ff, 0x0099ffff, 0x0099ccff, 0x009999ff, 0x009966ff, 0x009933ff,
    0x009900ff, 0x0066ffff, 0x0066ccff, 0x006699ff, 0x006666ff, 0x006633ff,
    0x006600ff, 0x0033ffff, 0x0033ccff, 0x003399ff, 0x003366ff, 0x003333ff,
    0x003300ff, 0x0000ffff, 0x0000ccff, 0x000099ff, 0x000066ff, 0x000033ff,
    0xee0000ff, 0xdd0000ff, 0xbb0000ff, 0xaa0000ff, 0x880000ff, 0x770000ff,
    0x550000ff, 0x440000ff, 0x220000ff, 0x110000ff, 0x00ee00ff, 0x00dd00ff,
    0x00bb00ff, 0x00aa00ff, 0x008800ff, 0x007700ff, 0x005500ff, 0x004400ff,
    0x002200ff, 0x001100ff, 0x0000eeff, 0x0000ddff, 0x0000bbff, 0x0000aaff,
    0x000088ff, 0x000077ff, 0x000055ff, 0x000044ff, 0x000022ff, 0x000011ff,
    0xeeeeeeff, 0xddddddff, 0xbbbbbbff, 0xaaaaaaff, 0x888888ff, 0x777777ff,
    0x555555ff, 0x444444ff, 0x222222ff, 0x111111ff,
};
