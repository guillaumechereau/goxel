/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

static const int N = BLOCK_SIZE;

#define BLOCK_ITER_INSIDE(x, y, z) \
    for (z = 1; z < N - 1; z++) \
        for (y = 1; y < N - 1; y++) \
            for (x = 1; x < N - 1; x++)

#define DATA_AT(d, x, y, z) (d->voxels[x + y * N + z * N * N])

// Implemented in marchingcube.c
int block_generate_vertices_mc(const block_data_t *data, int effects,
                               voxel_vertex_t *out);

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

/* Packing of block id, pos, and face:
 *
 *    x   :  4 bits
 *    y   :  4 bits
 *    z   :  4 bits
 *    pad :  1 bit
 *    face:  3 bits
 *    id  : 16 bits
 *    -------------
 *    tot : 32 bits
 */
static uint32_t get_pos_data(uint32_t x, uint32_t y, uint32_t z, uint32_t f,
                             uint32_t i)
{
    assert(BLOCK_SIZE == 16);
    return (x << 28) | (y << 24) | (z << 20) | (f << 16) | (i & 0xffff);
}


int block_generate_vertices(const block_data_t *data, int effects,
                            int block_id, voxel_vertex_t *out)
{
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
                out[nb * 4 + i].pos_data = get_pos_data(x, y, z, f, block_id);
            }
            nb++;
        }
    }
    return nb;
}

