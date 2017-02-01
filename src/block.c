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
 * Here is the convention I used for the cube vertices, edges and faces:
 *
 *           v4 +----------e4---------+ v5
 *             /.                    /|
 *            / .                   / |
 *          e7  .                 e5  |                    +-----------+
 *          /   .                 /   |                   /           /|
 *         /    .                /    |                  /   f1      / |  <f2
 *     v7 +----------e6---------+ v6  |                 +-----------+  |
 *        |     .               |     e9            f5> |           |f4|
 *        |     e8              |     |                 |           |  |
 *        |     .               |     |                 |    f3     |  +
 *        |     .               |     |                 |           | /
 *        |  v0 . . . .e0 . . . | . . + v1              |           |/
 *       e11   .                |    /                  +-----------+
 *        |   .                e10  /                         ^
 *        |  e3                 |  e1                         f0
 *        | .                   | /
 *        |.                    |/
 *     v3 +---------e2----------+ v2
 *
 */

static const int N = BLOCK_SIZE;

#define BLOCK_ITER(x, y, z) \
    for (z = 0; z < N; z++) \
        for (y = 0; y < N; y++) \
            for (x = 0; x < N; x++)

#define BLOCK_ITER_INSIDE(x, y, z) \
    for (z = 1; z < N - 1; z++) \
        for (y = 1; y < N - 1; y++) \
            for (x = 1; x < N - 1; x++)

#define DATA_AT(d, x, y, z) (d->voxels[x + y * N + z * N * N])
#define BLOCK_AT(c, x, y, z) (DATA_AT(c->data, x, y, z))

// face index -> [vertex0, vertex1, vertex2, vertex3]
const int FACES_VERTICES[6][4] = {
    {0, 1, 2, 3},
    {5, 4, 7, 6},
    {0, 4, 5, 1},
    {2, 6, 7, 3},
    {1, 5, 6, 2},
    {0, 3, 7, 4}
};

// face index + edge -> neighbor face index.
const int FACES_NEIGHBORS[6][4] = {
    {2, 4, 3, 5},
    {2, 5, 3, 4},
    {5, 1, 4, 0},
    {4, 1, 5, 0},
    {2, 1, 3, 0},
    {0, 3, 1, 2},
};

// vertex index -> vertex position
const vec3b_t VERTICES_POSITIONS[8] = {
    IVEC(0, 0, 0),
    IVEC(1, 0, 0),
    IVEC(1, 0, 1),
    IVEC(0, 0, 1),
    IVEC(0, 1, 0),
    IVEC(1, 1, 0),
    IVEC(1, 1, 1),
    IVEC(0, 1, 1)
};

const uvec2b_t VERTICE_UV[4] = {
    IVEC(0, 0),
    IVEC(1, 0),
    IVEC(1, 1),
    IVEC(0, 1),
};

const vec3b_t FACES_NORMALS[6] = {
    IVEC( 0, -1,  0),
    IVEC( 0, +1,  0),
    IVEC( 0,  0, -1),
    IVEC( 0,  0, +1),
    IVEC( 1,  0,  0),
    IVEC(-1,  0,  0),
};

// Matrices of transformation: unity plane => cube face plane.
const mat4_t FACES_MATS[6] = {
    MAT(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, -1, 0, 1),
    MAT(-1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1),
    MAT(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, -1, 1),
    MAT(0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1),
    MAT(0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1),
    MAT(0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 1),
};

// XXX: Keep orientation.
const vec4b_t FACES_ROTATIONS[6] = {
    IVEC( 1,  1,  0,  0),
    IVEC(-1,  1,  0,  0),
    IVEC( 2,  1,  0,  0),
    IVEC( 0,  1,  0,  0),
    IVEC( 1,  0,  1,  0),
    IVEC(-1,  0,  1,  0),
};

const int EDGES_VERTICES[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

// Marching cube data.
static const int MC_EDGE_TABLE[256];
static const int8_t MC_TRI_TABLE[256][16];

// Represent a marching cube vertex, by two vertex index and a factor
// between them.  If mu = 0, the vertex is at v0, if mu = 1, the vertex is
// at v1, otherwise the vertex is somewhere in between.
typedef struct {
    int     v0, v1;
    float   mu;
} mc_vert_t;

static vec3b_t mc_interp_pos(const mc_vert_t *vert)
{
    int i;
    vec3b_t ret;
    vec3b_t p0 = VERTICES_POSITIONS[vert->v0];
    vec3b_t p1 = VERTICES_POSITIONS[vert->v1];
    const float mu = vert->mu;
    for (i = 0; i < 3; i++)
        ret.v[i] = (p0.v[i] * (1 - mu) + p1.v[i] * mu) * MC_VOXEL_SUB_POS;
    return ret;
}

static uvec4b_t mc_interp_color(const mc_vert_t *vert,
                               const uvec4b_t neighboors[8])
{
    uvec4b_t ret;
    const uvec4b_t v0 = neighboors[vert->v0];
    const uvec4b_t v1 = neighboors[vert->v1];
    ret.a = 255;
    int i;
    for (i = 0; i < 3; i++)
        ret.v[i] = (v0.v[i] * v0.a + v1.v[i] * v1.a) / (v0.a + v1.a);
    return ret;
}


static int mc_compute(uint8_t cube_index, const uvec4b_t neighboors[8],
                      mc_vert_t (*out)[3])
{
    int edges, i, nb_tri;
    int f0, f1;
    mc_vert_t verts[12];
    edges = MC_EDGE_TABLE[cube_index];
    if (!edges) return 0;
    for (i = 0; i < 12; i++) {
        if (!(edges & (1 << i))) continue;
        verts[i].v0 = EDGES_VERTICES[i][0];
        verts[i].v1 = EDGES_VERTICES[i][1];
        f0 = neighboors[verts[i].v0].a;
        f1 = neighboors[verts[i].v1].a;
        verts[i].mu = (f0 - 127) / (float)(f0 - f1);
    }
    nb_tri = 0;
    for (i = 0; MC_TRI_TABLE[cube_index][i] != -1; i += 3) {
        out[nb_tri][0] = verts[MC_TRI_TABLE[cube_index][i + 0]];
        out[nb_tri][1] = verts[MC_TRI_TABLE[cube_index][i + 1]];
        out[nb_tri][2] = verts[MC_TRI_TABLE[cube_index][i + 2]];
        nb_tri++;
    }
    assert(nb_tri <= 5);
    return nb_tri;
}

static block_data_t *get_empty_data(void)
{
    static block_data_t *data = NULL;
    if (!data) {
        data = calloc(1, sizeof(*data));
        data->ref = 1;
        data->id = 0;
        goxel->block_count++;
    }
    return data;
}

bool block_is_empty(const block_t *block, bool fast)
{
    int x, y, z;
    if (!block) return true;
    if (block->data->id == 0) return true;
    if (fast) return false;

    BLOCK_ITER(x, y, z) {
        if (BLOCK_AT(block, x, y, z).a) return false;
    }
    return true;
}

block_t *block_new(const vec3_t *pos, block_data_t *data)
{
    block_t *block = calloc(1, sizeof(*block));
    block->pos = *pos;
    block->data = data ?: get_empty_data();
    block->data->ref++;
    return block;
}

void block_delete(block_t *block)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    free(block);
}

block_t *block_copy(const block_t *other)
{
    block_t *block = malloc(sizeof(*block));
    *block = *other;
    memset(&block->hh, 0, sizeof(block->hh));
    block->data->ref++;
    return block;
}

void block_set_data(block_t *block, block_data_t *data)
{
    block->data->ref--;
    if (block->data->ref == 0) {
        free(block->data);
        goxel->block_count--;
    }
    block->data = data;
    data->ref++;
}

box_t block_get_box(const block_t *block, bool exact)
{
    box_t ret;
    int x, y, z;
    int xmin = N, xmax = 0, ymin = N, ymax = 0, zmin = N, zmax = 0;
    if (!exact)
        return bbox_from_extents(block->pos, N / 2, N / 2, N / 2);
    BLOCK_ITER(x, y, z) {
        if (BLOCK_AT(block, x, y, z).a) {
            xmin = min(xmin, x);
            ymin = min(ymin, y);
            zmin = min(zmin, z);
            xmax = max(xmax, x);
            ymax = max(ymax, y);
            zmax = max(zmax, z);
        }
    }
    if (xmin > xmax) return box_null;
    ret = bbox_from_points(vec3(xmin - 0.5, ymin - 0.5, zmin - 0.5),
                           vec3(xmax + 0.5, ymax + 0.5, zmax + 0.5));
    vec3_iadd(&ret.p, block->pos);
    vec3_isub(&ret.p, vec3(N / 2 - 0.5, N / 2 - 0.5, N / 2 - 0.5));
    return ret;
}

static uint32_t block_get_neighboors(const block_data_t *data,
                                    int x, int y, int z,
                                    uint8_t neighboors[27])
{
    int xx, yy, zz, i = 0;
    vec3b_t npos;
    uint32_t ret = 0;
#define ITER_NEIGHBORS(x, y, z)         \
     for (z = -1; z <= 1; z++)           \
         for (y = -1; y <= 1; y++)       \
             for (x = -1; x <= 1; x++)
    ITER_NEIGHBORS(xx, yy, zz) {
        npos = vec3b(x + xx, y + yy, z + zz);
        neighboors[i] = DATA_AT(data, npos.x, npos.y, npos.z).a;
        if (neighboors[i] >= 127) ret |= 1 << i;
        i++;
    }
#undef ITER_NEIGHBORS
    return ret;
}

static bool block_is_face_visible(uint32_t neighboors_mask, int f)
{
#define M(x, y, z) (1 << ((x + 1) + (y + 1) * 3 + (z + 1) * 9))
    static const uint32_t MASKS[6] = {
        M(0, -1, 0), M(0, +1, 0), M(0, 0, -1),
        M(0, 0, +1), M(+1, 0, 0), M(-1, 0, 0),
    };
#undef M
    return !(MASKS[f] & neighboors_mask);
}

static vec3b_t block_get_normal(uint32_t neighboors_mask,
                                const uint8_t neighboors[27], int f,
                                bool smooth)
{
    int x, y, z, i = 0;
    int sx = 0, sy = 0, sz = 0;
    int smax;
    if (!smooth) return FACES_NORMALS[f];
    for (z = -1; z <= +1; z++)
    for (y = -1; y <= +1; y++)
    for (x = -1; x <= +1; x++) {
        if (neighboors_mask & (1 << i)) {
            sx -= neighboors[i] * x;
            sy -= neighboors[i] * y;
            sz -= neighboors[i] * z;
        }
        i++;
    }
    if (sx == 0 && sy == 0 && sz == 0)
        return FACES_NORMALS[f];
    smax = max(abs(sx), max(abs(sy), abs(sz)));
    return vec3b(sx * 127 / smax, sy * 127 / smax, sz * 127 / smax);
}

static bool block_get_edge_border(uint32_t neighboors_mask, int f, int e)
{
#define M(x, y, z) (1 << ((x + 1) + (y + 1) * 3 + (z + 1) * 9))
    static const uint32_t MASKS[6][4] = {
        /* F0 */ {M( 0, -1, -1), M(+1, -1,  0), M( 0, -1, +1), M(-1, -1,  0)},
        /* F1 */ {M( 0, +1, -1), M(-1, +1,  0), M( 0, +1, +1), M(+1, +1,  0)},
        /* F2 */ {M(-1,  0, -1), M( 0,  1, -1), M( 1,  0, -1), M( 0, -1, -1)},
        /* F3 */ {M( 1,  0,  1), M( 0,  1,  1), M(-1,  0,  1), M( 0, -1,  1)},
        /* F4 */ {M( 1,  0, -1), M( 1,  1,  0), M( 1,  0,  1), M( 1, -1,  0)},
        /* F5 */ {M(-1, -1,  0), M(-1,  0,  1), M(-1,  1,  0), M(-1,  0, -1)},
    };
#undef M
    return neighboors_mask & MASKS[f][e];
}

static bool block_get_vertice_border(uint32_t neighboors_mask, int f, int i)
{
#define M(x, y, z) (1 << ((x + 1) + (y + 1) * 3 + (z + 1) * 9))
    static const uint32_t MASKS[6][4] = {
        {   // F0
            M(-1, -1,  0) | M( 0, -1, -1) | M(-1, -1, -1),
            M( 0, -1, -1) | M( 1, -1,  0) | M( 1, -1, -1),
            M( 1, -1,  0) | M( 0, -1,  1) | M( 1, -1,  1),
            M( 0, -1,  1) | M(-1, -1,  0) | M(-1, -1,  1),
        },
        {   //F1
            M( 1,  1,  0) | M( 0,  1, -1) | M( 1,  1, -1),
            M( 0,  1, -1) | M(-1,  1,  0) | M(-1,  1, -1),
            M(-1,  1,  0) | M( 0,  1,  1) | M(-1,  1,  1),
            M( 0,  1,  1) | M( 1,  1,  0) | M( 1,  1,  1),
        },
        {   // F2
            M( 0, -1, -1) | M(-1,  0, -1) | M(-1, -1, -1),
            M(-1,  0, -1) | M( 0,  1, -1) | M(-1,  1, -1),
            M( 0,  1, -1) | M( 1,  0, -1) | M( 1,  1, -1),
            M( 1,  0, -1) | M( 0, -1, -1) | M( 1, -1, -1),
        },
        {   // F3
            M( 0, -1,  1) | M( 1,  0,  1) | M( 1, -1,  1),
            M( 1,  0,  1) | M( 0,  1,  1) | M( 1,  1,  1),
            M( 0,  1,  1) | M(-1,  0,  1) | M(-1,  1,  1),
            M(-1,  0,  1) | M( 0, -1,  1) | M(-1, -1,  1),
        },
        {   // F4
            M( 1, -1,  0) | M( 1,  0, -1) | M( 1, -1, -1),
            M( 1,  0, -1) | M( 1,  1,  0) | M( 1,  1, -1),
            M( 1,  1,  0) | M( 1,  0,  1) | M( 1,  1,  1),
            M( 1,  0,  1) | M( 1, -1,  0) | M( 1, -1,  1),
        },
        {   // F5
            M(-1,  0, -1) | M(-1, -1,  0) | M(-1, -1, -1),
            M(-1, -1,  0) | M(-1,  0,  1) | M(-1, -1,  1),
            M(-1,  0,  1) | M(-1,  1,  0) | M(-1,  1,  1),
            M(-1,  1,  0) | M(-1,  0, -1) | M(-1,  1, -1),
        },
    };
#undef M
    return neighboors_mask & MASKS[f][i];
}

static uint8_t block_get_shadow_mask(uint32_t neighboors_mask, int f)
{
    int i;
    uint8_t ret = 0;
    for (i = 0; i < 4; i++) {
        ret |= block_get_vertice_border(neighboors_mask, f, i) ? (1 << i) : 0;
        ret |= block_get_edge_border(neighboors_mask, f, i) ? (0x10 << i) : 0;
    }
    return ret;
}

static uint8_t block_get_border_mask(uint32_t neighboors_mask,
                                     int f, int effects)
{
#define M(x, y, z) (1 << ((x + 1) + (y + 1) * 3 + (z + 1) * 9))
    int e;
    int ret = 0;
    vec3b_t n;
    if (effects & EFFECT_BORDERS_ALL) return 15;
    if (!(effects & EFFECT_BORDERS)) return 0;
    for (e = 0; e < 4; e++) {
        n = FACES_NORMALS[FACES_NEIGHBORS[f][e]];
        if (!(neighboors_mask & M(n.x, n.y, n.z)))
            ret |= 1 << e;
    }
    return ret;
#undef M
}

/* Packing of block id, pos, and face:
 *
 *    x   :  4 bits
 *    y   :  4 bits
 *    z   :  4 bits
 *    pad :  1 bit
 *    face:  3 bits
 *    -------------
 *    tot : 16 bits
 *
 *    So it fits into 2 bytes, and we can feed it into a shader as a vec2.
 */
static uvec2b_t get_pos_as_vec2(int x, int y, int z, int f)
{
    return uvec2b(x << 4 | y,
                  z << 4 | f);
}

static uint8_t block_get_neighboors_mc(const block_data_t *data,
                                       int x, int y, int z,
                                       uvec4b_t neighboors[8])
{
    int i;
    vec3b_t npos;
    uint8_t ret = 0;
    for (i = 0; i < 8; i++) {
        npos = vec3b_add(vec3b(x, y, z), VERTICES_POSITIONS[i]);
        neighboors[i] = DATA_AT(data, npos.x, npos.y, npos.z);
        if (neighboors[i].a > 127) ret |= 1 << i;
    }
    return ret;
}

// XXX: I could use an interpolation of the normal for a smooth effect.
static vec3b_t mc_normal(uint8_t neighboors_mask, uvec4b_t neighboors[8])
{
    int i;
    int ssum = 0;
    int sx = 0, sy = 0, sz = 0;
    for (i = 0; i < 8; i++) {
        ssum += neighboors[i].a;
        sx -= (VERTICES_POSITIONS[i].x * 2 - 1) * neighboors[i].a / 4;
        sy -= (VERTICES_POSITIONS[i].y * 2 - 1) * neighboors[i].a / 4;
        sz -= (VERTICES_POSITIONS[i].z * 2 - 1) * neighboors[i].a / 4;
    }
    return vec3b(sx * 256 / ssum, sy * 256 / ssum, sz * 256 / ssum);
}

static int block_generate_vertices_mc(const block_data_t *data, int effects,
                                      voxel_vertex_t *out)
{
    uint8_t neighboors_mask;
    uvec4b_t neighboors[8];
    int x, y, z, nb_tri, i, v, nb_tri_tot = 0, vi;
    mc_vert_t tri[5][3];
    BLOCK_ITER_INSIDE(x, y, z) {
        neighboors_mask = block_get_neighboors_mc(data, x, y, z, neighboors);
        nb_tri = mc_compute(neighboors_mask, neighboors, tri);
        for (i = 0; i < nb_tri; i++) {
            for (v = 0; v < 3; v++) {
                vi = (nb_tri_tot + i) * 3 + v;
                out[vi].pos = mc_interp_pos(&tri[i][v]);
                vec3b_iaddk(&out[vi].pos, vec3b(x, y, z), MC_VOXEL_SUB_POS);
                vec3b_iadd(&out[vi].pos, vec3b(4, 4, 4));
                out[vi].color = mc_interp_color(&tri[i][v], neighboors);
                out[vi].normal = mc_normal(neighboors_mask, neighboors);

                // XXX: this shouldn't matter.
                out[vi].bshadow_uv = uvec2b(0, 0);
                out[vi].bump_uv = uvec2b(0, 0);
            }
        }
        nb_tri_tot += nb_tri;
    }
    return nb_tri_tot;
}

int block_generate_vertices(const block_data_t *data, int effects,
                            voxel_vertex_t *out)
{
    PROFILED
    int x, y, z, f;
    int i, nb = 0;
    uint32_t neighboors_mask;
    uint8_t shadow_mask, borders_mask;
    vec3b_t normal;
    const int ts = VOXEL_TEXTURE_SIZE;
    uint8_t neighboors[27];
    if (effects & EFFECT_MARCHING_CUBES)
        return block_generate_vertices_mc(data, effects, out);
    BLOCK_ITER_INSIDE(x, y, z) {
        if (DATA_AT(data, x, y, z).a < 127) continue;    // Non visible
        neighboors_mask = block_get_neighboors(data, x, y, z, neighboors);
        for (f = 0; f < 6; f++) {
            if (!block_is_face_visible(neighboors_mask, f)) continue;
            normal = block_get_normal(neighboors_mask, neighboors, f,
                     effects & EFFECT_SMOOTH);
            shadow_mask = block_get_shadow_mask(neighboors_mask, f);
            borders_mask = block_get_border_mask(neighboors_mask, f, effects);
            for (i = 0; i < 4; i++) {
                out[nb * 4 + i].pos = vec3b_add(
                        vec3b(x, y, z),
                        VERTICES_POSITIONS[FACES_VERTICES[f][i]]);
                out[nb * 4 + i].normal = normal;
                out[nb * 4 + i].color = DATA_AT(data, x, y, z);
                out[nb * 4 + i].color.a = out[nb * 4 + i].color.a ? 255 : 0;
                out[nb * 4 + i].bshadow_uv = uvec2b(
                    shadow_mask % 16 * ts + VERTICE_UV[i].x * (ts - 1),
                    shadow_mask / 16 * ts + VERTICE_UV[i].y * (ts - 1));
                out[nb * 4 + i].uv = uvec2b(VERTICE_UV[i].x * 255,
                                            VERTICE_UV[i].y * 255);
                // For testing:
                // This put a border bump on all the edges of the voxel.
                out[nb * 4 + i].bump_uv = uvec2b(borders_mask * 16, f * 16);
                out[nb * 4 + i].pos_data = get_pos_as_vec2(x, y, z, f);
            }
            nb++;
        }
    }
    return nb;
}

static vec3_t block_get_voxel_pos(const block_t *block, int x, int y, int z)
{
    return vec3(block->pos.x + x - BLOCK_SIZE / 2 + 0.5,
                block->pos.y + y - BLOCK_SIZE / 2 + 0.5,
                block->pos.z + z - BLOCK_SIZE / 2 + 0.5);
}

// Copy the data if there are any other blocks having reference to it.
static void block_prepare_write(block_t *block)
{
    if (block->data->ref == 1) {
        return;
    }
    block->data->ref--;
    block_data_t *data;
    data = calloc(1, sizeof(*block->data));
    memcpy(data->voxels, block->data->voxels, N * N * N * 4);
    data->ref = 1;
    block->data = data;
    block->data->id = ++goxel->next_uid;
    goxel->block_count++;
}

void block_fill(block_t *block,
                uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
                void *user_data)
{
    int x, y, z;
    vec3_t p;
    uvec4b_t c;
    block_prepare_write(block);
    BLOCK_ITER(x, y, z) {
        p = block_get_voxel_pos(block, x, y, z);
        c = get_color(&p, user_data);
        BLOCK_AT(block, x, y, z) = c;
    }
}

static bool can_skip(uvec4b_t v, int mode, uvec4b_t c)
{
    return (v.a && (mode == MODE_ADD) && uvec4b_equal(c, v)) ||
            (!v.a && IS_IN(mode, MODE_SUB, MODE_PAINT, MODE_SUB_CLAMP));
}

static uvec4b_t combine(uvec4b_t a, uvec4b_t b, int mode)
{
    uvec4b_t ret = a;
    int i, aa = a.a, ba = b.a;
    if (mode == MODE_PAINT) {
        ret.rgb = uvec3b_mix(a.rgb, b.rgb, ba / 255.);
    }
    else if (mode == MODE_ADD) {
        ret.a = min((int)a.a + b.a, 255);
        if (aa + ba)
            for (i = 0; i < 3; i++)
                ret.v[i] = (a.v[i] * aa + b.v[i] * ba) / (aa + ba);
    }
    else if (mode == MODE_SUB) {
        ret.a = max(0, aa - ba);
    }
    else if (mode == MODE_MAX) {
        ret.a = max(a.a, b.a);
        ret.rgb = b.rgb;
    } else if (mode == MODE_SUB_CLAMP) {
        ret.a = min(aa, 255 - ba);
        ret.rgb = a.rgb;
    } else {
        assert(false);
    }
    return ret;
}

// XXX: cleanup this function.
void block_op(block_t *block, painter_t *painter, const box_t *box)
{
    int x, y, z;
    mat4_t mat = mat4_identity;
    vec3_t p, size;
    float k, v;
    int mode = painter->mode;
    uvec4b_t c;
    float (*shape_func)(const vec3_t*, const vec3_t*, float smoothness);
    shape_func = painter->shape->func;
    bool invert = false;

    if (mode == MODE_INTERSECT) {
        mode = MODE_SUB;
        invert = true;
    }

    size = box_get_size(*box);
    mat4_imul(&mat, box->mat);
    mat4_iscale(&mat, 1 / size.x, 1 / size.y, 1 / size.z);
    mat4_invert(&mat);

    mat4_itranslate(&mat, block->pos.x, block->pos.y, block->pos.z);
    mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

    BLOCK_ITER(x, y, z) {
        c = painter->color;
        if (can_skip(BLOCK_AT(block, x, y, z), mode, c)) continue;
        p = mat4_mul_vec3(mat, vec3(x, y, z));
        k = shape_func(&p, &size, painter->smoothness);
        k = clamp(k / painter->smoothness, -1, 1);
        v = (k + 1) * 0.5;
        if (invert) v = 1.0 - v;
        if (v) {
            block_prepare_write(block);
            c.a *= v;
            BLOCK_AT(block, x, y, z) = combine(
                BLOCK_AT(block, x, y, z), c, mode);
        }
    }
}

void block_merge(block_t *block, const block_t *other, int mode)
{
    int x, y, z;
    block_data_t *data;

    if (!other || other->data == get_empty_data()) return;
    if (IS_IN(mode, MODE_ADD, MODE_MAX) && block->data == get_empty_data()) {
        block_set_data(block, other->data);
        return;
    }

    // Check if the merge op has been cached.
    struct {
        uint64_t id1;
        uint64_t id2;
        int      mode;
    } key = {
        block->data->id, other->data->id, mode
    };
    data = cache_get(&key, sizeof(key));
    if (data) {
        block_set_data(block, data);
        return;
    }

    block_prepare_write(block);
    BLOCK_ITER(x, y, z) {
        BLOCK_AT(block, x, y, z) = combine(DATA_AT(block->data, x, y, z),
                                           DATA_AT(other->data, x, y, z),
                                           mode);
    }
    cache_add(&key, sizeof(key), block->data);
}

uvec4b_t block_get_at(const block_t *block, const vec3_t *pos)
{
    int x, y, z;
    vec3_t p = *pos;
    assert(bbox_contains_vec(block_get_box(block, false), *pos));
    vec3_isub(&p, block->pos);
    vec3_iadd(&p, vec3(N / 2 - 0.5, N / 2 - 0.5, N / 2 - 0.5));
    x = round(p.x);
    y = round(p.y);
    z = round(p.z);
    return BLOCK_AT(block, x, y, z);
}

void block_set_at(block_t *block, const vec3_t *pos, uvec4b_t v)
{
    int x, y, z;
    vec3_t p = *pos;
    assert(bbox_contains_vec(block_get_box(block, false), *pos));
    vec3_isub(&p, block->pos);
    vec3_iadd(&p, vec3(N / 2 - 0.5, N / 2 - 0.5, N / 2 - 0.5));
    block_prepare_write(block);
    x = round(p.x);
    y = round(p.y);
    z = round(p.z);
    BLOCK_AT(block, x, y, z) = v;
}

void block_blit(block_t *block, uvec4b_t *data,
                int x, int y, int z, int w, int h, int d)
{
    int bx, by, bz;
    int dx, dy, dz;
    uvec4b_t v;
    BLOCK_ITER(bx, by, bz) {
        dx = bx - x + block->pos.x - N / 2;
        dy = by - y + block->pos.y - N / 2;
        dz = bz - z + block->pos.z - N / 2;
        if (dx < 0 || dx >= w || dy < 0 || dy >= h || dz < 0 || dz >= d)
            continue;
        v = data[dz * w * h + dy * w + dx];
        if (v.a) {
            block_prepare_write(block);
            BLOCK_AT(block, bx, by, bz) = v;
        }
    }
}

void block_shift_alpha(block_t *block, int v)
{
    block_prepare_write(block);
    int i;
    for (i = 0; i < N * N * N; i++) {
        block->data->voxels[i].a = clamp(block->data->voxels[i].a + v, 0, 255);
    }
}

// Static data for marching cube algo.
static const int MC_EDGE_TABLE[256] = {
    0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

static const int8_t MC_TRI_TABLE[256][16] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
    {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
    {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
    {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
    {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
    {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
    {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
    {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
    {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
    {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
    {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
