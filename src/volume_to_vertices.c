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
#include "utils/color.h"

#include "../ext_src/meshoptimizer/meshoptimizer.h"

static const int N = BLOCK_SIZE;

// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}


// Implemented in marchingcube.c
int volume_generate_vertices_mc(const volume_t *volume, const int block_pos[3],
                              int effects, voxel_vertex_t *out,
                              int *size, int *pos_scale);

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

static void block_get_normal(int f, int8_t normal[3], int8_t tangent[3])
{
    normal[0] = FACES_NORMALS[f][0];
    normal[1] = FACES_NORMALS[f][1];
    normal[2] = FACES_NORMALS[f][2];
    tangent[0] = FACES_TANGENTS[f][0];
    tangent[1] = FACES_TANGENTS[f][1];
    tangent[2] = FACES_TANGENTS[f][2];
}

static void block_get_gradient(uint32_t neighboors_mask,
                               const uint8_t neighboors[27], int f,
                               int8_t gradient[3])
{
    int x, y, z, i = 0;
    int sx = 0, sy = 0, sz = 0;
    int smax;

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
    if (sx == 0 && sy == 0 && sz == 0) {
        gradient[0] = FACES_NORMALS[f][0];
        gradient[1] = FACES_NORMALS[f][1];
        gradient[2] = FACES_NORMALS[f][2];
        return;
    }
    smax = max(abs(sx), max(abs(sy), abs(sz)));
    gradient[0] = sx * 127 / smax;
    gradient[1] = sy * 127 / smax;
    gradient[2] = sz * 127 / smax;
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

static uint8_t block_get_border_mask(uint32_t neighboors_mask, int f)
{
#define M(x, y, z) (1 << ((x + 1) + (y + 1) * 3 + (z + 1) * 9))
    int e;
    uint8_t ret = 0;
    const int *n, *t;
    for (e = 0; e < 4; e++) {
        n = FACES_NORMALS[f];
        t = FACES_NORMALS[FACES_NEIGHBORS[f][e]];
        if (neighboors_mask & M(n[0] + t[0], n[1] + t[1], n[2] + t[2])) {
            ret |= 2 << (2 * e);
        } else if (!(neighboors_mask & M(t[0], t[1], t[2]))) {
            ret |= 1 << (2 * e);
        }
    }
    return ret;
#undef M
}

#define data_get_at(d, x, y, z, out) do { \
    memcpy(out, &data[( \
                ((x) + 1) + \
                ((y) + 1) * (N + 2) + \
                ((z) + 1) * (N + 2) * (N + 2)) * 4], 4); \
} while (0)

static uint32_t get_neighboors(const uint8_t *data,
                               const int pos[3],
                               uint8_t neighboors[27])
{
    int xx, yy, zz, i = 0;
    uint8_t v[4];
    uint32_t ret = 0;
    for (zz = -1; zz <= 1; zz++)
    for (yy = -1; yy <= 1; yy++)
    for (xx = -1; xx <= 1; xx++) {
        data_get_at(data, pos[0] + xx, pos[1] + yy, pos[2] + zz, v);
        neighboors[i] = v[3];
        if (neighboors[i] >= 127) ret |= 1 << i;
        i++;
    }
    return ret;
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
 */
static uint16_t get_pos_data(uint16_t x, uint16_t y, uint16_t z, uint16_t f)
{
    assert(BLOCK_SIZE == 16);
    return (x << 12) | (y << 8) | (z << 4) | (f << 0);
}


int volume_generate_vertices(const volume_t *volume, const int block_pos[3],
                           int effects, voxel_vertex_t *out,
                           int *size, int *subdivide)
{
    int x, y, z, f;
    int i, nb = 0;
    uint32_t neighboors_mask;
    uint8_t shadow_mask, borders_mask;
    const int ts = VOXEL_TEXTURE_SIZE;
    uint8_t *data, neighboors[27], v[4];
    int8_t normal[3], tangent[3], gradient[3];
    int pos[3];
    const int *vpos;

    if (effects & EFFECT_MARCHING_CUBES)
        return volume_generate_vertices_mc(volume, block_pos, effects, out,
                                         size, subdivide);

    *size = 4;      // Quad.
    *subdivide = 1; // Unit is one voxel.

    // To speed things up we first get the voxel cube around the block.
    // XXX: can we do this while still using volume iterators somehow?
#define IVEC(...) ((int[]){__VA_ARGS__})
    data = malloc((N + 2) * (N + 2) * (N + 2) * 4);
    volume_read(volume,
              IVEC(block_pos[0] - 1, block_pos[1] - 1, block_pos[2] - 1),
              IVEC(N + 2, N + 2, N + 2), data);

    for (z = 0; z < N; z++)
    for (y = 0; y < N; y++)
    for (x = 0; x < N; x++) {
        pos[0] = x;
        pos[1] = y;
        pos[2] = z;
        data_get_at(data, x, y, z, v);
        if (v[3] < 127) continue;    // Non visible
        neighboors_mask = get_neighboors(data, pos, neighboors);
        for (f = 0; f < 6; f++) {
            if (!block_is_face_visible(neighboors_mask, f)) continue;
            block_get_normal(f, normal, tangent);
            block_get_gradient(neighboors_mask, neighboors, f, gradient);
            shadow_mask = block_get_shadow_mask(neighboors_mask, f);
            borders_mask = block_get_border_mask(neighboors_mask, f);
            for (i = 0; i < 4; i++) {
                vpos = VERTICES_POSITIONS[FACES_VERTICES[f][i]];
                out[nb * 4 + i].pos[0] = x + vpos[0];
                out[nb * 4 + i].pos[1] = y + vpos[1];
                out[nb * 4 + i].pos[2] = z + vpos[2];
                memcpy(out[nb * 4 + i].normal, normal, sizeof(normal));
                memcpy(out[nb * 4 + i].tangent, tangent, sizeof(tangent));
                memcpy(out[nb * 4 + i].gradient, gradient, sizeof(gradient));
                memcpy(out[nb * 4 + i].color, v, sizeof(v));
                out[nb * 4 + i].color[3] = out[nb * 4 + i].color[3] ? 255 : 0;
                out[nb * 4 + i].occlusion_uv[0] =
                    shadow_mask % 16 * ts + VERTICE_UV[i][0] * (ts - 1);
                out[nb * 4 + i].occlusion_uv[1] =
                    shadow_mask / 16 * ts + VERTICE_UV[i][1] * (ts - 1);
                out[nb * 4 + i].uv[0] = VERTICE_UV[i][0] * 255;
                out[nb * 4 + i].uv[1] = VERTICE_UV[i][1] * 255;
                // For testing:
                // This put a border bump on all the edges of the voxel.
                out[nb * 4 + i].bump_uv[0] = (borders_mask % 16) * 16;
                out[nb * 4 + i].bump_uv[1] = (borders_mask / 16) * 16;
                out[nb * 4 + i].pos_data = get_pos_data(x, y, z, f);
            }
            nb++;
        }
    }
    free(data);
    return nb;
}

static void fill_mesh(volume_mesh_t *mesh,
                      const voxel_vertex_t *verts, int nb, int size,
                      int subdivide, const int bpos[3],
                      const palette_t *palette)
{
    int i, c, idx, s = 0;
    float normal[3];
    float color[4];

    // The palette texture size.
    if (palette != NULL)
        s = max(next_pow2(ceil(log2(palette->size))), 16);

    // Fill up the vertices.
    mesh->vertices = realloc(
            mesh->vertices, (mesh->vertices_count + nb * size) *
            sizeof(*mesh->vertices));
    memset(mesh->vertices + mesh->vertices_count, 0,
           nb * size * sizeof(*mesh->vertices));
    for (i = 0; i < nb * size; i++) {
        idx = mesh->vertices_count + i;
        normal[0] = verts[i].normal[0];
        normal[1] = verts[i].normal[1];
        normal[2] = verts[i].normal[2];
        vec3_normalize(normal, normal);
        mesh->vertices[idx] = (typeof(*mesh->vertices)) {
            .pos = {
                verts[i].pos[0] / subdivide + bpos[0],
                verts[i].pos[1] / subdivide + bpos[1],
                verts[i].pos[2] / subdivide + bpos[2]},
            .normal = {normal[0], normal[1], normal[2]},
        };
        if (palette == NULL) {
            srgba8_to_rgba(verts[i].color, color);
            mesh->vertices[idx].color[0] = color[0];
            mesh->vertices[idx].color[1] = color[1];
            mesh->vertices[idx].color[2] = color[2];
            mesh->vertices[idx].color[3] = color[3];
        } else {
            c = palette_search(palette, verts[i].color, true);
            assert(c != -1);
            mesh->vertices[idx].texcoord[0] = (c % s + 0.5) / s;
            mesh->vertices[idx].texcoord[1] = (c / s + 0.5) / s;
        }
    }

    // Add the indices.
    if (size == 4) {
        mesh->indices = realloc(
                mesh->indices, (mesh->indices_count + 6 * nb) *
                sizeof(*mesh->indices));
        for (i = 0; i < nb * 6; i++) {
            mesh->indices[mesh->indices_count + i] = mesh->vertices_count +
                (i / 6) * 4 + ((int[]){0, 1, 2, 2, 3, 0})[i % 6];
        }
        mesh->indices_count += nb * 6;
    } else {
        mesh->indices = realloc(
                mesh->indices, (mesh->indices_count + 3 * nb) *
                sizeof(*mesh->indices));
        for (i = 0; i < nb * 3; i++) {
            mesh->indices[mesh->indices_count + i] = mesh->vertices_count + i;
        }
        mesh->indices_count += nb * 3;
    }

    mesh->vertices_count += nb * size;
}

static void optimize_mesh(volume_mesh_t *mesh, float simplify)
{
    unsigned int *remap;
    unsigned int *tmp_indices;
    typeof(*mesh->vertices) *tmp_vertices;
    size_t vertices_count;
    size_t indices_count;
    int target_index_count;
	float target_error = 1e-2f;

    // Merge duplicated vertices.
    remap = calloc(mesh->vertices_count, sizeof(unsigned int));
    vertices_count = meshopt_generateVertexRemap(
            remap, mesh->indices, mesh->indices_count, mesh->vertices,
            mesh->vertices_count, sizeof(*mesh->vertices));

    tmp_indices = malloc(mesh->indices_count * sizeof(*tmp_indices));
    meshopt_remapIndexBuffer(
            tmp_indices, mesh->indices, mesh->indices_count, remap);

    tmp_vertices = malloc(vertices_count * sizeof(*mesh->vertices));
    meshopt_remapVertexBuffer(
            tmp_vertices, mesh->vertices, mesh->vertices_count,
            sizeof(*mesh->vertices), remap);

    free(remap);
    SWAP(mesh->vertices, tmp_vertices);
    SWAP(mesh->indices, tmp_indices);
    mesh->vertices_count = vertices_count;

    // Also simplify the mesh if required.
	target_index_count = (int)(mesh->indices_count * (1 - simplify));
    target_index_count = fmax(target_index_count, 1);
    if (target_index_count < mesh->indices_count) {
        indices_count = meshopt_simplify(
                tmp_indices, mesh->indices, mesh->indices_count,
                (const float*)mesh->vertices, mesh->vertices_count,
                sizeof(*mesh->vertices), target_index_count, target_error,
                0, NULL);
        vertices_count = meshopt_optimizeVertexFetch(
                tmp_vertices, tmp_indices, indices_count,
                mesh->vertices, mesh->vertices_count, sizeof(*mesh->vertices));
        SWAP(mesh->vertices, tmp_vertices);
        SWAP(mesh->indices, tmp_indices);
        mesh->vertices_count = vertices_count;
        mesh->indices_count = indices_count;
    }

    free(tmp_vertices);
    free(tmp_indices);
}

volume_mesh_t *volume_generate_mesh(
        const volume_t *volume, int effects, const palette_t *palette,
        float simplify)
{
    volume_iterator_t iter;
    int bpos[3];
    voxel_vertex_t *verts;
    int i, nb, size, subdivide;
    volume_mesh_t *mesh = calloc(1, sizeof(*mesh));

    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    iter = volume_get_iterator(volume,
            VOLUME_ITER_TILES | VOLUME_ITER_INCLUDES_NEIGHBORS);
    while (volume_iter(&iter, bpos)) {
        nb = volume_generate_vertices(volume, bpos, effects, verts,
                                      &size, &subdivide);
        if (nb == 0) continue;
        fill_mesh(mesh, verts, nb, size, subdivide, bpos, palette);
    }

    optimize_mesh(mesh, simplify);

    mesh->pos_min[0] = +FLT_MAX;
    mesh->pos_min[1] = +FLT_MAX;
    mesh->pos_min[2] = +FLT_MAX;
    mesh->pos_max[0] = -FLT_MAX;
    mesh->pos_max[1] = -FLT_MAX;
    mesh->pos_max[2] = -FLT_MAX;
    for (i = 0; i < mesh->vertices_count; i++) {
        mesh->pos_min[0] = min(mesh->vertices[i].pos[0], mesh->pos_min[0]);
        mesh->pos_min[1] = min(mesh->vertices[i].pos[1], mesh->pos_min[1]);
        mesh->pos_min[2] = min(mesh->vertices[i].pos[2], mesh->pos_min[2]);
        mesh->pos_max[0] = max(mesh->vertices[i].pos[0], mesh->pos_max[0]);
        mesh->pos_max[1] = max(mesh->vertices[i].pos[1], mesh->pos_max[1]);
        mesh->pos_max[2] = max(mesh->vertices[i].pos[2], mesh->pos_max[2]);
    }

    return mesh;
}

void volume_mesh_free(volume_mesh_t *mesh)
{
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}
