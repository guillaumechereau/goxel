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
#include <limits.h>

static const uint32_t VOX_DEFAULT_PALETTE[256];

#define READ(type, file) \
    ({ type v; int r = fread(&v, sizeof(v), 1, file); (void)r; v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

typedef struct {
    int         w, h, d;
    uvec4b_t    *palette;
    int         nb;
    uint8_t     *voxels;
} context_t;

static void read_chunk(FILE *file, context_t *ctx)
{
    char id[4], r;
    int size, children_size, fpos, i;

    r = fread(id, 1, 4, file);
    (void)r;
    size = READ(uint32_t, file);
    children_size = READ(uint32_t, file);

    if (strncmp(id, "SIZE", 4) == 0) {
        assert(size = 4 * 3);
        ctx->w = READ(uint32_t, file);
        ctx->h = READ(uint32_t, file);
        ctx->d = READ(uint32_t, file);
    } else if (strncmp(id, "RGBA", 4) == 0) {
        ctx->palette = malloc(4 * 256);
        for (i = 0; i < 256; i++) {
            ctx->palette[i].r = READ(uint8_t, file);
            ctx->palette[i].g = READ(uint8_t, file);
            ctx->palette[i].b = READ(uint8_t, file);
            ctx->palette[i].a = READ(uint8_t, file);
        }
    } else if (strncmp(id, "XYZI", 4) == 0) {
        ctx->nb = READ(uint32_t, file);
        ctx->voxels = calloc(ctx->nb, 4);
        for (i = 0; i < ctx->nb; i++) {
            ctx->voxels[i * 4 + 0] = READ(uint8_t, file);
            ctx->voxels[i * 4 + 1] = READ(uint8_t, file);
            ctx->voxels[i * 4 + 2] = READ(uint8_t, file);
            ctx->voxels[i * 4 + 3] = READ(uint8_t, file);
        }

    } else {
        fseek(file, size, SEEK_CUR);
    }

    fpos = ftell(file);
    while (ftell(file) < fpos + children_size) {
        read_chunk(file, ctx);
    }
}

void vox_import(const char *path)
{
    FILE *file;
    char magic[4];
    vec3_t pos;
    int version, r, i, x, y, z, c;
    mesh_t      *mesh;
    uvec4b_t color;
    context_t ctx = {};

    mesh = goxel()->image->active_layer->mesh;
    file = fopen(path, "r");
    r = fread(magic, 1, 4, file);
    (void)r;
    assert(strncmp(magic, "VOX ", 4) == 0);
    version = READ(uint32_t, file);
    (void)version;
    read_chunk(file, &ctx);

    assert(ctx.voxels);
    for (i = 0; i < ctx.nb; i++) {
        x = ctx.voxels[i * 4 + 0];
        y = ctx.voxels[i * 4 + 1];
        z = ctx.voxels[i * 4 + 2];
        c = ctx.voxels[i * 4 + 3];
        pos = vec3(x + 0.5 - ctx.w / 2, y + 0.5 - ctx.h / 2, z + 0.5);
        if (!c) continue; // Not sure what c == 0 means.
        color = ctx.palette ? ctx.palette[c - 1] :
                              HEXCOLOR(VOX_DEFAULT_PALETTE[c]);
        mesh_set_at(mesh, &pos, color);
    }
    free(ctx.voxels);
    free(ctx.palette);
    mesh_remove_empty_blocks(mesh);
    goxel_update_meshes(goxel(), true);
}

static int get_color_index(uvec4b_t v, const uvec4b_t *palette, bool exact)
{
    uvec4b_t c;
    int i, dist, best = -1, best_dist = 1024;
    for (i = 0; i < 255; i++) {
        c = palette[i];
        dist = abs((int)c.r - (int)v.r) +
               abs((int)c.g - (int)v.g) +
               abs((int)c.b - (int)v.b);
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
    if (a[2] != b[2]) return sign(a[2] - b[2]);
    if (a[1] != b[1]) return sign(a[1] - b[1]);
    if (a[0] != b[0]) return sign(a[0] - b[0]);
    return 0;
}

static void vox_export(const mesh_t *mesh, const char *path)
{
    FILE *file;
    int children_size, nb_vox = 0, i, x, y, z, vx, vy, vz;
    int xmin = INT_MAX, ymin = INT_MAX, zmin = INT_MAX;
    int xmax = INT_MIN, ymax = INT_MIN, zmax = INT_MIN;
    uvec4b_t *palette;
    bool use_default_palette = true;
    uint8_t *voxels;
    block_t *block;
    uvec4b_t v;

    palette = calloc(256, sizeof(*palette));
    for (i = 0; i < 256; i++)
        palette[i] = HEXCOLOR(VOX_DEFAULT_PALETTE[i]);

    // Iter all the voxels to get the count and the size.
    MESH_ITER_VOXELS(mesh, block, x, y, z, v) {
        if (v.a < 127) continue;
        v.a = 255;
        use_default_palette = use_default_palette &&
                            get_color_index(v, palette, true) != -1;
        nb_vox++;
        vx = round(x + block->pos.x - BLOCK_SIZE / 2);
        vy = round(y + block->pos.y - BLOCK_SIZE / 2);
        vz = round(z + block->pos.z - BLOCK_SIZE / 2);
        xmin = min(xmin, vx);
        ymin = min(ymin, vy);
        zmin = min(zmin, vz);
        xmax = max(xmax, vx + 1);
        ymax = max(ymax, vy + 1);
        zmax = max(zmax, vz + 1);
    }
    if (!use_default_palette)
        quantization_gen_palette(mesh, 256, palette);

    children_size = 12 + 4 * 3 +      // SIZE chunk
                    12 + 4 * nb_vox + // XYZI chunk
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
    WRITE(uint32_t, 4 * nb_vox, file);
    WRITE(uint32_t, 0, file);
    WRITE(uint32_t, nb_vox, file);

    voxels = calloc(nb_vox, 4);
    i = 0;
    MESH_ITER_VOXELS(mesh, block, x, y, z, v) {
        if (v.a < 127) continue;
        vx = round(x + block->pos.x - BLOCK_SIZE / 2) - xmin;
        vy = round(y + block->pos.y - BLOCK_SIZE / 2) - ymin;
        vz = round(z + block->pos.z - BLOCK_SIZE / 2) - zmin;
        assert(vx >= 0 && vx < 255);
        assert(vy >= 0 && vy < 255);
        assert(vz >= 0 && vz < 255);

        voxels[i * 4 + 0] = vx;
        voxels[i * 4 + 1] = vy;
        voxels[i * 4 + 2] = vz;
        voxels[i * 4 + 3] = get_color_index(v, palette, false) + 1;
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
        for (i = 0; i < 256; i++)
            WRITE(uint32_t, palette[i].uint32, file);
    }

    fclose(file);
    free(palette);
}

static void export_as_vox(goxel_t *goxel, const char *path)
{
    vox_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_vox,
    .help = "Save the image as a vox 3d file",
    .func = export_as_vox,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)


static const uint32_t VOX_DEFAULT_PALETTE[256] = {
    0xffffffff, 0xffffccff, 0xffff99ff, 0xffff66ff, 0xffff33ff, 0xffff00ff,
    0xffccffff, 0xffccccff, 0xffcc99ff, 0xffcc66ff, 0xffcc33ff, 0xffcc00ff,
    0xff99ffff, 0xff99ccff, 0xff9999ff, 0xff9966ff, 0xff9933ff, 0xff9900ff,
    0xff66ffff, 0xff66ccff, 0xff6699ff, 0xff6666ff, 0xff6633ff, 0xff6600ff,
    0xff33ffff, 0xff33ccff, 0xff3399ff, 0xff3366ff, 0xff3333ff, 0xff3300ff,
    0xff00ffff, 0xff00ccff, 0xff0099ff, 0xff0066ff, 0xff0033ff, 0xff0000ff,
    0xccffffff, 0xccffccff, 0xccff99ff, 0xccff66ff, 0xccff33ff, 0xccff00ff,
    0xccccffff, 0xccccccff, 0xcccc99ff, 0xcccc66ff, 0xcccc33ff, 0xcccc00ff,
    0xcc99ffff, 0xcc99ccff, 0xcc9999ff, 0xcc9966ff, 0xcc9933ff, 0xcc9900ff,
    0xcc66ffff, 0xcc66ccff, 0xcc6699ff, 0xcc6666ff, 0xcc6633ff, 0xcc6600ff,
    0xcc33ffff, 0xcc33ccff, 0xcc3399ff, 0xcc3366ff, 0xcc3333ff, 0xcc3300ff,
    0xcc00ffff, 0xcc00ccff, 0xcc0099ff, 0xcc0066ff, 0xcc0033ff, 0xcc0000ff,
    0x99ffffff, 0x99ffccff, 0x99ff99ff, 0x99ff66ff, 0x99ff33ff, 0x99ff00ff,
    0x99ccffff, 0x99ccccff, 0x99cc99ff, 0x99cc66ff, 0x99cc33ff, 0x99cc00ff,
    0x9999ffff, 0x9999ccff, 0x999999ff, 0x999966ff, 0x999933ff, 0x999900ff,
    0x9966ffff, 0x9966ccff, 0x996699ff, 0x996666ff, 0x996633ff, 0x996600ff,
    0x9933ffff, 0x9933ccff, 0x993399ff, 0x993366ff, 0x993333ff, 0x993300ff,
    0x9900ffff, 0x9900ccff, 0x990099ff, 0x990066ff, 0x990033ff, 0x990000ff,
    0x66ffffff, 0x66ffccff, 0x66ff99ff, 0x66ff66ff, 0x66ff33ff, 0x66ff00ff,
    0x66ccffff, 0x66ccccff, 0x66cc99ff, 0x66cc66ff, 0x66cc33ff, 0x66cc00ff,
    0x6699ffff, 0x6699ccff, 0x669999ff, 0x669966ff, 0x669933ff, 0x669900ff,
    0x6666ffff, 0x6666ccff, 0x666699ff, 0x666666ff, 0x666633ff, 0x666600ff,
    0x6633ffff, 0x6633ccff, 0x663399ff, 0x663366ff, 0x663333ff, 0x663300ff,
    0x6600ffff, 0x6600ccff, 0x660099ff, 0x660066ff, 0x660033ff, 0x660000ff,
    0x33ffffff, 0x33ffccff, 0x33ff99ff, 0x33ff66ff, 0x33ff33ff, 0x33ff00ff,
    0x33ccffff, 0x33ccccff, 0x33cc99ff, 0x33cc66ff, 0x33cc33ff, 0x33cc00ff,
    0x3399ffff, 0x3399ccff, 0x339999ff, 0x339966ff, 0x339933ff, 0x339900ff,
    0x3366ffff, 0x3366ccff, 0x336699ff, 0x336666ff, 0x336633ff, 0x336600ff,
    0x3333ffff, 0x3333ccff, 0x333399ff, 0x333366ff, 0x333333ff, 0x333300ff,
    0x3300ffff, 0x3300ccff, 0x330099ff, 0x330066ff, 0x330033ff, 0x330000ff,
    0x00ffffff, 0x00ffccff, 0x00ff99ff, 0x00ff66ff, 0x00ff33ff, 0x00ff00ff,
    0x00ccffff, 0x00ccccff, 0x00cc99ff, 0x00cc66ff, 0x00cc33ff, 0x00cc00ff,
    0x0099ffff, 0x0099ccff, 0x009999ff, 0x009966ff, 0x009933ff, 0x009900ff,
    0x0066ffff, 0x0066ccff, 0x006699ff, 0x006666ff, 0x006633ff, 0x006600ff,
    0x0033ffff, 0x0033ccff, 0x003399ff, 0x003366ff, 0x003333ff, 0x003300ff,
    0x0000ffff, 0x0000ccff, 0x000099ff, 0x000066ff, 0x000033ff, 0xee0000ff,
    0xdd0000ff, 0xbb0000ff, 0xaa0000ff, 0x880000ff, 0x770000ff, 0x550000ff,
    0x440000ff, 0x220000ff, 0x110000ff, 0x00ee00ff, 0x00dd00ff, 0x00bb00ff,
    0x00aa00ff, 0x008800ff, 0x007700ff, 0x005500ff, 0x004400ff, 0x002200ff,
    0x001100ff, 0x0000eeff, 0x0000ddff, 0x0000bbff, 0x0000aaff, 0x000088ff,
    0x000077ff, 0x000055ff, 0x000044ff, 0x000022ff, 0x000011ff, 0xeeeeeeff,
    0xddddddff, 0xbbbbbbff, 0xaaaaaaff, 0x888888ff, 0x777777ff, 0x555555ff,
    0x444444ff, 0x222222ff, 0x111111ff, 0x000000ff,
};
