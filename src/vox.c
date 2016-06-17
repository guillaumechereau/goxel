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

static const uint32_t DEFAULT_PALETTE[256];

#define READ(type, file) \
    ({ type v; int r = fread(&v, sizeof(v), 1, file); (void)r; v;})

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
        color = ctx.palette ? ctx.palette[c] : HEXCOLOR(DEFAULT_PALETTE[c]);
        mesh_set_at(mesh, &pos, color);
    }
    free(ctx.voxels);
    free(ctx.palette);
    mesh_remove_empty_blocks(mesh);
}


static const uint32_t DEFAULT_PALETTE[256] = {
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
