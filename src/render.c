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

#include "shader_cache.h"

#ifndef RENDER_CACHE_SIZE
#   define RENDER_CACHE_SIZE (1 * GB)
#endif

/*
 * The rendering is delayed from the time we call the different render
 * functions.  This allows to call `render_xxx` anywhere in the code, without
 * having to worry about the current OpenGL state.
 *
 * Since generating the blocks vertex buffers is slow, we buffer them in a hash
 * table.  We can evict blocks from the buffer when we need to save space or
 * if we know that the block won't be used anymore.
 */

// TODO: we need to get ride of unused blocks in the buffer, that hav not been
// rendered for too long.  First we might need to keep track of how much video
// memory is used, so we get an idea of how much we need that.

enum {
    ITEM_MESH = 1,
    ITEM_MODEL3D,
    ITEM_GRID,
};

typedef struct {
    uint64_t ids[27];
    int effects;
} block_item_key_t;

struct render_item_t
{
    render_item_t   *next, *prev;   // The rendering queue.
    int             type;
    block_item_key_t key;

    union {
        mesh_t          *mesh;
        float           mat[4][4];
    };
    material_t      material;
    uint8_t         color[4];
    float           clip_box[4][4];
    bool            proj_screen; // Render with a 2d proj.
    model3d_t       *model3d;
    texture_t       *tex;
    int             effects;

    GLuint      vertex_buffer;
    int         size;           // 4 (quads) or 3 (triangles).
    int         nb_elements;    // Number of quads or triangle.
    int         subdivide;      // Unit per voxel (usually 1).
};

// The buffered item hash table.  For the moment it is only used of the blocks.
// static render_item_t *g_items = NULL;

// The cache of the g_items.
static cache_t   *g_items_cache;
static const int BATCH_QUAD_COUNT = 1 << 14;
static model3d_t *g_cube_model;
static model3d_t *g_line_model;
static model3d_t *g_wire_cube_model;
static model3d_t *g_sphere_model;
static model3d_t *g_grid_model;
static model3d_t *g_rect_model;
static model3d_t *g_wire_rect_model;

static GLuint g_index_buffer;
static GLuint g_background_array_buffer;
static GLuint g_occlusion_tex;
static GLuint g_bump_tex;
static GLuint g_shadow_map_fbo;
static texture_t *g_shadow_map; // XXX: the fbo should be part of the tex.

#define OFFSET(n) offsetof(voxel_vertex_t, n)

enum {
    A_POS_LOC = 0,
    A_NORMAL_LOC,
    A_TANGENT_LOC,
    A_GRADIENT_LOC,
    A_COLOR_LOC,
    A_POS_DATA_LOC,
    A_UV_LOC,
    A_BUMP_UV_LOC,
    A_OCCLUSION_UV_LOC,
};

// The list of all the attributes used by the shaders.
static const struct {
    int size;
    int type;
    int norm;
    int offset;
} ATTRIBUTES[] = {
    [A_POS_LOC] = {3, GL_UNSIGNED_BYTE, false, OFFSET(pos)},
    [A_NORMAL_LOC] = { 3, GL_BYTE, false, OFFSET(normal)},
    [A_TANGENT_LOC] = {3, GL_BYTE, false, OFFSET(tangent)},
    [A_GRADIENT_LOC] = {3, GL_BYTE, false, OFFSET(gradient)},
    [A_COLOR_LOC] = {4, GL_UNSIGNED_BYTE, true, OFFSET(color)},
    [A_POS_DATA_LOC] = {2, GL_UNSIGNED_BYTE, true, OFFSET(pos_data)},
    [A_UV_LOC] = {2, GL_UNSIGNED_BYTE, true,  OFFSET(uv)},
    [A_BUMP_UV_LOC] = {2, GL_UNSIGNED_BYTE, false, OFFSET(bump_uv)},
    [A_OCCLUSION_UV_LOC] = {2, GL_UNSIGNED_BYTE, false, OFFSET(occlusion_uv)},
};

static const char *ATTR_NAMES[] = {
    [A_POS_LOC] = "a_pos",
    [A_NORMAL_LOC] = "a_normal",
    [A_TANGENT_LOC] = "a_tangent",
    [A_GRADIENT_LOC] = "a_gradient",
    [A_COLOR_LOC] = "a_color",
    [A_POS_DATA_LOC] = "a_pos_data",
    [A_UV_LOC] = "a_uv",
    [A_BUMP_UV_LOC] = "a_bump_uv",
    [A_OCCLUSION_UV_LOC] = "a_occlusion_uv",
    NULL,
};

/*
 *  Create a texture atlas of the 256 possible border textures for a voxel
 *  face.  Each pixel correspond to the minimum distance to a border or an
 *  edge.
 *
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |14 |15 |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |16 |.. |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *
 *  The index into the atlas is the bit mask of the touching side for
 *  the 4 corners and the 4 edges => 8bit => 256 blocks.
 *
 */

static void copy_color(const uint8_t in[4], uint8_t out[4])
{
    if (in == NULL) {
        out[0] = 255;
        out[1] = 255;
        out[2] = 255;
        out[3] = 255;
    } else {
        memcpy(out, in, 4);
    }
}

static float get_border_dist(float x, float y, int mask)
{
    const float corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const float normals[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    float ret = 1;
    int i;
    float u[2];
    float p[2] = {x, y};
    for (i = 0; i < 4; i++) {
        if (mask & (1 << i))        // Corners.
            ret = min(ret, vec2_dist(p, corners[i]));
        if (mask & (0x10 << i)) {  // Edges.
            vec2_sub(p, corners[i], u);
            ret = min(ret, vec2_dot(normals[i], u));
        }
    }
    return ret;
}

static void init_occlusion_texture(void)
{
    const int s = VOXEL_TEXTURE_SIZE;    // the individual tile size.
    uint8_t data[256 * s * s];
    int mask, x, y, ax, ay;
    for (mask = 0; mask < 256; mask++) {
        for (y = 0; y < s; y++) for (x = 0; x < s; x++) {
            ay = mask / 16 * s + y;
            ax = mask % 16 * s + x;
            data[ay * s * 16 + ax] = 255 * sqrt(get_border_dist(
                    (float)x / s + 0.5 / s, (float)y / s + 0.5 / s, mask));
        }
    }
    GL(glGenTextures(1, &g_occlusion_tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, g_occlusion_tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, s * 16, s * 16,
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data));
}

static void set_bump_block(uint8_t (*data)[3], int bx, int by, uint8_t mask)
{
    float v[3];
    int y, x, k;

    for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++) {
        vec3_set(v, 0, 0, 1);
        if ((mask &   1) && x == 15) vec2_add(v, VEC( 1,  0), v);
        if ((mask &   2) && x == 15) vec2_add(v, VEC(-1,  0), v);
        if ((mask &   4) && y == 15) vec2_add(v, VEC( 0,  1), v);
        if ((mask &   8) && y == 15) vec2_add(v, VEC( 0, -1), v);
        if ((mask &  16) && x ==  0) vec2_add(v, VEC(-1,  0), v);
        if ((mask &  32) && x ==  0) vec2_add(v, VEC( 1,  0), v);
        if ((mask &  64) && y ==  0) vec2_add(v, VEC( 0, -1), v);
        if ((mask & 128) && y ==  0) vec2_add(v, VEC( 0, +1), v);
        vec3_normalize(v, v);
        for (k = 0; k < 3; k++) {
            data[(by * 16 + y) * 256 + bx * 16 + x][k] =
                mix(0, 255, (v[k] / 2 + 0.5));
        }
    }
}

/*
 * The bump texture is an atlas of 256 16x16 rects for each combination
 * of edge neighboor:
 *
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |14 |15 |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |16 |.. |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *
 *
 * If we index the edges of a face like that:
 *
 *  y
 *  ^
 *  |
 *  +-----------+
 *  |     1     |
 *  |           |
 *  |2         0|
 *  |           |
 *  |     3     |
 *  +-----------+---> x
 *
 * The index in the atlas is the 8 bit concatenation of the mask for each edge:
 * [e0, e1, e2, e3]
 *
 * With each edge mask a 2 bits value:
 * 0 -> z =  0
 * 1 -> z = +1
 * 2 -> z = -1
 *
 */
static void init_bump_texture(void)
{
    uint8_t (*data)[3];
    int i;
    data = calloc(1, 256 * 256 * 3);
    for (i = 0; i < 256; i++) {
        set_bump_block(data, i % 16, i / 16, i);
    }
    GL(glGenTextures(1, &g_bump_tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, g_bump_tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256,
                    0, GL_RGB, GL_UNSIGNED_BYTE, data));
    free(data);
}

static void shader_init(gl_shader_t *shader)
{
    GL(glUseProgram(shader->prog));
    gl_update_uniform(shader, "u_normal_sampler", 0);
    gl_update_uniform(shader, "u_occlusion_tex", 1);
    gl_update_uniform(shader, "u_shadow_tex", 2);
}

void render_init()
{
    // 6 vertices (2 triangles) per face.
    uint16_t *index_array = NULL;
    int i;

    LOG_D("render init");
    GL(glGenBuffers(1, &g_index_buffer));
    GL(glGenBuffers(1, &g_background_array_buffer));

    // Index buffer start with the quads, followed by the lines.
    index_array = calloc(BATCH_QUAD_COUNT * (6 + 8), sizeof(*index_array));
    for (i = 0; i < BATCH_QUAD_COUNT * 6; i++) {
        index_array[i] = (i / 6) * 4 + ((int[]){0, 1, 2, 2, 3, 0})[i % 6];
    }
    for (i = 0; i < BATCH_QUAD_COUNT * 8; i++) {
        index_array[BATCH_QUAD_COUNT * 6 + i] =
            (i / 8) * 4 + ((int[]){0, 1, 1, 2, 2, 3, 3, 0})[i % 8];
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, BATCH_QUAD_COUNT * (6 + 8) * 2,
                    index_array, GL_STATIC_DRAW));
#ifndef GLES2
    GL(glEnable(GL_LINE_SMOOTH));
#endif

    free(index_array);
    init_occlusion_texture();
    init_bump_texture();

    // XXX: pick the proper memory size according to what is available.
    g_items_cache = cache_create(RENDER_CACHE_SIZE);
    g_cube_model = model3d_cube();
    g_line_model = model3d_line();
    g_wire_cube_model = model3d_wire_cube();
    g_sphere_model = model3d_sphere(16, 16);
    g_grid_model = model3d_grid(8, 8);
    g_rect_model = model3d_rect();
    g_wire_rect_model = model3d_wire_rect();
}

void render_deinit(void)
{
    cache_delete(g_items_cache);
    GL(glDeleteBuffers(1, &g_index_buffer));
    g_index_buffer = 0;
    model3d_delete(g_cube_model);
    model3d_delete(g_line_model);
    model3d_delete(g_wire_cube_model);
    model3d_delete(g_sphere_model);
    model3d_delete(g_grid_model);
    model3d_delete(g_rect_model);
    model3d_delete(g_wire_rect_model);
}

// A global buffer large enough to contain all the vertices for any block.
static voxel_vertex_t* g_vertices_buffer = NULL;

// Used for the cache.
static int item_delete(void *item_)
{
    render_item_t *item = item_;
    GL(glDeleteBuffers(1, &item->vertex_buffer));
    free(item);
    return 0;
}

static render_item_t *get_item_for_block(
        const mesh_t *mesh,
        mesh_iterator_t *iter,
        const int block_pos[3],
        int effects, float smoothness)
{
    render_item_t *item;
    const int effects_mask = EFFECT_MARCHING_CUBES | EFFECT_MC_SMOOTH;
    uint64_t block_data_id;
    int p[3], i, x, y, z;
    block_item_key_t key = {};

    memset(&key, 0, sizeof(key)); // Just to be sure!
    key.effects = effects & effects_mask;
    // The hash key take into consideration all the blocks adjacent to
    // the current block!
    for (i = 0, z = -1; z <= 1; z++)
    for (y = -1; y <= 1; y++)
    for (x = -1; x <= 1; x++, i++) {
        p[0] = block_pos[0] + x * BLOCK_SIZE;
        p[1] = block_pos[1] + y * BLOCK_SIZE;
        p[2] = block_pos[2] + z * BLOCK_SIZE;
        mesh_get_block_data(mesh, NULL, p, &block_data_id);
        key.ids[i] = block_data_id;
    }

    item = cache_get(g_items_cache, &key, sizeof(key));
    if (item) return item;

    item = calloc(1, sizeof(*item));
    item->key = key;
    GL(glGenBuffers(1, &item->vertex_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, item->vertex_buffer));
    if (!g_vertices_buffer)
        g_vertices_buffer = calloc(
                BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE * 6 * 4,
                sizeof(*g_vertices_buffer));
    item->nb_elements = mesh_generate_vertices(
            mesh, block_pos, effects, g_vertices_buffer,
            &item->size, &item->subdivide);
    if (item->nb_elements > BATCH_QUAD_COUNT) {
        LOG_W("Too many quads!");
        item->nb_elements = BATCH_QUAD_COUNT;
    }
    if (item->nb_elements != 0) {
        GL(glBufferData(GL_ARRAY_BUFFER,
                item->nb_elements * item->size * sizeof(*g_vertices_buffer),
                g_vertices_buffer, GL_STATIC_DRAW));
    }

    cache_add(g_items_cache, &key, sizeof(key), item,
              item->nb_elements * item->size * sizeof(*g_vertices_buffer),
              item_delete);
    return item;
}

static void render_block_(renderer_t *rend, mesh_t *mesh,
                          mesh_iterator_t *iter,
                          const int block_pos[3],
                          int block_id,
                          const material_t *material,
                          int effects, gl_shader_t *shader,
                          const float model[4][4])
{
    render_item_t *item;
    float block_model[4][4];
    int attr;
    float block_id_f[2];

    item = get_item_for_block(mesh, iter, block_pos, effects,
                              rend->settings.smoothness);
    if (item->nb_elements == 0) return;
    GL(glBindBuffer(GL_ARRAY_BUFFER, item->vertex_buffer));
    if (gl_has_uniform(shader, "u_block_id")) {
        block_id_f[1] = ((block_id >> 8) & 0xff) / 255.0;
        block_id_f[0] = ((block_id >> 0) & 0xff) / 255.0;
        gl_update_uniform(shader, "u_block_id", block_id_f);
    }
    gl_update_uniform(shader, "u_pos_scale", 1.f / item->subdivide);

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++) {
        GL(glVertexAttribPointer(attr,
                                 ATTRIBUTES[attr].size,
                                 ATTRIBUTES[attr].type,
                                 ATTRIBUTES[attr].norm,
                                 sizeof(voxel_vertex_t),
                                 (void*)(intptr_t)ATTRIBUTES[attr].offset));
    }

    mat4_copy(model, block_model);
    mat4_itranslate(block_model, block_pos[0], block_pos[1], block_pos[2]);
    gl_update_uniform(shader, "u_model", block_model);
    if (item->size == 4) {
        if (!(effects & (EFFECT_GRID | EFFECT_EDGES))) {
            GL(glDrawElements(GL_TRIANGLES, item->nb_elements * 6,
                              GL_UNSIGNED_SHORT, 0));
        } else {
            gl_update_uniform(shader, "u_l_amb", 0.0);
            gl_update_uniform(shader, "u_z_ofs", -0.001);
            GL(glDrawElements(GL_LINES, item->nb_elements * 8,
                              GL_UNSIGNED_SHORT,
                              (void*)(uintptr_t)(BATCH_QUAD_COUNT * 6 * 2)));
            gl_update_uniform(shader, "u_l_amb", rend->settings.ambient);
            gl_update_uniform(shader, "u_z_ofs", 0.0);
        }
    } else {
        GL(glDrawArrays(GL_TRIANGLES, 0, item->nb_elements * item->size));
    }

#ifndef GLES2
    if (effects & EFFECT_WIREFRAME) {
        gl_update_uniform(shader, "u_l_amb", 0.0);
        GL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
        if (item->size == 4)
            GL(glDrawElements(GL_TRIANGLES, item->nb_elements * 6,
                              GL_UNSIGNED_SHORT, 0));
        else
            GL(glDrawArrays(GL_TRIANGLES, 0, item->nb_elements * item->size));
        GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
        gl_update_uniform(shader, "u_l_amb", rend->settings.ambient);
    }
#endif
}

static void get_light_dir(const renderer_t *rend, float out[3])
{
    float light_dir[4];
    float m[4][4];

    mat4_set_identity(m);
    mat4_irotate(m, rend->light.yaw, 0, 0, 1);
    mat4_irotate(m, rend->light.pitch, 1, 0, 0);
    mat4_mul_vec4(m, VEC(0, 0, 1, 0), light_dir);

    if (rend->light.fixed) {
        mat4_invert(rend->view_mat, m);
        mat4_irotate(m, -M_PI / 4, 1, 0, 0);
        mat4_irotate(m, -M_PI / 4, 0, 0, 1);
        mat4_mul_vec4(m, light_dir, light_dir);
    }
    vec3_copy(light_dir, out);
}

void render_get_light_dir(const renderer_t *rend, float out[3])
{
    get_light_dir(rend, out);
}

// Compute the minimum projection box to use for the shadow map.
static void compute_shadow_map_box(
                const renderer_t *rend,
                float rect[6])
{
    const float POS[8][3] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 0, 1},
        {0, 0, 1},
        {0, 1, 0},
        {1, 1, 0},
        {1, 1, 1},
        {0, 1, 1},
    };
    const int N = BLOCK_SIZE;

    render_item_t *item;
    float p[3];
    int i, bpos[3];
    mesh_iterator_t iter;
    float view_mat[4][4], light_dir[3];

    get_light_dir(rend, light_dir);
    mat4_lookat(view_mat, light_dir, VEC(0, 0, 0), VEC(0, 1, 0));
    rect[0] = +FLT_MAX;
    rect[1] = -FLT_MAX;
    rect[2] = +FLT_MAX;
    rect[3] = -FLT_MAX;
    rect[4] = +FLT_MAX;
    rect[5] = -FLT_MAX;

    DL_FOREACH(rend->items, item) {
        if (item->type != ITEM_MESH) continue;
        iter = mesh_get_iterator(item->mesh, MESH_ITER_BLOCKS);
        while (mesh_iter(&iter, bpos)) {
            for (i = 0; i < 8; i++) {
                vec3_set(p, bpos[0], bpos[1], bpos[2]);
                vec3_addk(p, POS[i], N, p);
                mat4_mul_vec3(view_mat, p, p);
                rect[0] = min(rect[0], p[0]);
                rect[1] = max(rect[1], p[0]);
                rect[2] = min(rect[2], p[1]);
                rect[3] = max(rect[3], p[1]);
                rect[4] = min(rect[4], -p[2]);
                rect[5] = max(rect[5], -p[2]);
            }
        }
    }
}

static void render_mesh_(renderer_t *rend, mesh_t *mesh,
                         const material_t *material, int effects,
                         const float shadow_mvp[4][4])
{
    gl_shader_t *shader;
    float model[4][4], camera[4][4];
    int attr, block_pos[3], block_id;
    float light_dir[3], alpha;
    bool shadow = false;
    mesh_iterator_t iter;

    mat4_set_identity(model);
    get_light_dir(rend, light_dir);

    if (effects & EFFECT_MARCHING_CUBES)
        effects &= ~EFFECT_BORDERS;

    if (effects & EFFECT_RENDER_POS)
        shader = shader_get("pos_data", NULL, ATTR_NAMES, shader_init);
    else if (effects & EFFECT_SHADOW_MAP)
        shader = shader_get("shadow_map", NULL, ATTR_NAMES, shader_init);
    else {
        shadow = rend->settings.shadow;
        shader_define_t defines[] = {
            {"SHADOW", shadow},
            {"MATERIAL_UNLIT", (rend->settings.effects & EFFECT_UNLIT) ||
                               (effects & EFFECT_EDGES)},
            {"HAS_TANGENTS", effects & EFFECT_BORDERS},
            {"ONLY_EDGES", effects & EFFECT_EDGES},
            {"HAS_OCCLUSION_MAP", rend->settings.occlusion_strength > 0},
            {"VERTEX_LIGHTNING", !(effects & (EFFECT_BORDERS | EFFECT_UNLIT))},
            {"SMOOTHNESS", rend->settings.smoothness > 0},
            {}
        };
        shader = shader_get("mesh", defines, ATTR_NAMES, shader_init);
    }

    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(GL_BACK));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, g_bump_tex));

    GL(glActiveTexture(GL_TEXTURE1));
    GL(glBindTexture(GL_TEXTURE_2D, g_occlusion_tex));

    GL(glDisable(GL_BLEND));

    if (effects & EFFECT_SEE_BACK) {
        GL(glCullFace(GL_FRONT));
        vec3_imul(light_dir, -0.5);
    }

    alpha = material->base_color[3];
    if (effects & EFFECT_SEMI_TRANSPARENT) alpha *= 0.75;

    if (alpha < 1) {
        GL(glEnable(GL_BLEND));
        GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR));
        GL(glBlendColor(alpha, alpha, alpha, alpha));
    }

    GL(glUseProgram(shader->prog));

    if (shadow) {
        assert(shadow_mvp);
        GL(glActiveTexture(GL_TEXTURE2));
        GL(glBindTexture(GL_TEXTURE_2D, g_shadow_map->tex));
        gl_update_uniform(shader, "u_shadow_mvp", shadow_mvp);
        gl_update_uniform(shader, "u_shadow_tex", 2);
        gl_update_uniform(shader, "u_shadow_strength", rend->settings.shadow);
    }

    gl_update_uniform(shader, "u_proj", rend->proj_mat);
    gl_update_uniform(shader, "u_view", rend->view_mat);
    gl_update_uniform(shader, "u_normal_sampler", 0);
    gl_update_uniform(shader, "u_occlusion_tex", 1);
    gl_update_uniform(shader, "u_normal_scale",
                      effects & EFFECT_BORDERS ? 0.5 : 0.0);
    gl_update_uniform(shader, "u_l_dir", light_dir);
    gl_update_uniform(shader, "u_l_int", rend->light.intensity);
    gl_update_uniform(shader, "u_l_amb", rend->settings.ambient);

    gl_update_uniform(shader, "u_m_metallic", material->metallic);
    gl_update_uniform(shader, "u_m_roughness", material->roughness);
    gl_update_uniform(shader, "u_m_base_color", material->base_color);
    gl_update_uniform(shader, "u_m_emissive_factor", material->emission);
    gl_update_uniform(shader, "u_m_smoothness", rend->settings.smoothness);

    gl_update_uniform(shader, "u_occlusion_strength",
                      rend->settings.occlusion_strength);

    mat4_invert(rend->view_mat, camera);
    gl_update_uniform(shader, "u_camera", camera[3]);

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glEnableVertexAttribArray(attr));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));

    block_id = 1;
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        render_block_(rend, mesh, &iter, block_pos,
                      block_id++, material, effects, shader, model);
    }
    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glDisableVertexAttribArray(attr));

    if (effects & EFFECT_SEE_BACK) {
        effects &= ~EFFECT_SEE_BACK;
        effects |= EFFECT_SEMI_TRANSPARENT;
        render_mesh_(rend, mesh, material, effects, shadow_mvp);
    }
    GL(glDisable(GL_BLEND));
}

// XXX: this is quite ugly.  We could maybe use a callback of some sort
// in the renderer instead.
void render_get_block_pos(renderer_t *rend, const mesh_t *mesh,
                          int id, int pos[3])
{
    // Basically we simulate the algo of render_mesh_ but without rendering
    // anything.
    int block_id, block_pos[3];
    mesh_iterator_t iter;
    block_id = 1;
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        if (block_id == id) {
            memcpy(pos, block_pos, sizeof(block_pos));
            return;
        }
        block_id++;
    }
}

void render_mesh(renderer_t *rend, const mesh_t *mesh,
                 const material_t *material, int effects)
{
    render_item_t *item;
    const material_t default_material = MATERIAL_DEFAULT;
    float alpha;

    material = material ?: &default_material;

    if (!(effects & EFFECT_GRID_ONLY)) {
        item = calloc(1, sizeof(*item));
        item->type = ITEM_MESH;
        item->mesh = mesh_copy(mesh);
        item->material = *material;
        item->effects = effects | rend->settings.effects;
        item->effects &= ~(EFFECT_GRID | EFFECT_EDGES);
        // With EFFECT_RENDER_POS we need to remove some effects.
        if (item->effects & EFFECT_RENDER_POS)
            item->effects &= ~(EFFECT_SEMI_TRANSPARENT | EFFECT_SEE_BACK |
                               EFFECT_MARCHING_CUBES);
        DL_APPEND(rend->items, item);
    }

    if (effects & EFFECT_GRID_ONLY) effects |= EFFECT_GRID;

    if (effects & EFFECT_GRID) {
        alpha = 0.1;
        item = calloc(1, sizeof(*item));
        item->type = ITEM_MESH;
        item->mesh = mesh_copy(mesh);
        item->effects = EFFECT_GRID | EFFECT_BORDERS;
        item->material = *material;
        vec4_set(item->material.base_color, 0, 0, 0, alpha);
        DL_APPEND(rend->items, item);
    }

    if (effects & EFFECT_EDGES) {
        alpha = 0.2;
        item = calloc(1, sizeof(*item));
        item->type = ITEM_MESH;
        item->mesh = mesh_copy(mesh);
        item->effects = EFFECT_EDGES | EFFECT_BORDERS;
        item->material = *material;
        vec4_set(item->material.base_color, 0, 0, 0, alpha);
        DL_APPEND(rend->items, item);
    }
}

static void render_model_item(renderer_t *rend, const render_item_t *item,
                              const float viewport[4])
{
    float proj[4][4];
    const float (*proj_mat)[4][4];
    const float (*view_mat)[4][4];
    float light[3];

    if (item->proj_screen) {
        mat4_ortho(proj, 0, viewport[2], 0, viewport[3], -128, +128);
        proj_mat = &proj;
        view_mat = &mat4_identity;
    } else {
        proj_mat = &rend->proj_mat;
        view_mat = &rend->view_mat;
    }

    if (!(item->effects & EFFECT_WIREFRAME))
        get_light_dir(rend, light);

    model3d_render(item->model3d,
                   item->mat, *view_mat, *proj_mat,
                   item->color,
                   item->tex, light, item->clip_box, item->effects);
}

static void render_grid_item(renderer_t *rend, const render_item_t *item)
{
    int x, y, n;
    float model_mat[4][4];

    n = 3;
    for (y = -n; y < n; y++)
    for (x = -n; x < n; x++) {
        mat4_copy(item->mat, model_mat);
        mat4_translate(model_mat, x + 0.5, y + 0.5, 0, model_mat);
        model3d_render(item->model3d,
                       model_mat, rend->view_mat, rend->proj_mat,
                       item->color, NULL, NULL, item->clip_box,
                       item->effects);
    }
}

void render_grid(renderer_t *rend, const float plane[4][4],
                 const uint8_t color[4], const float clip_box[4][4])
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_GRID;
    mat4_copy(plane, item->mat);
    mat4_iscale(item->mat, 1024, 1024, 1);
    mat4_itranslate(item->mat, 0, 0, 0.01);
    item->model3d = g_rect_model;
    copy_color(color, item->color);
    item->effects = EFFECT_GRID;
    if (clip_box) mat4_copy(clip_box, item->clip_box);
    DL_APPEND(rend->items, item);
}

void render_img(renderer_t *rend, texture_t *tex, const float mat[4][4],
                int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    mat ? mat4_copy(mat, item->mat) : mat4_set_identity(item->mat);
    item->proj_screen = !mat || (effects & EFFECT_PROJ_SCREEN);
    item->tex = texture_copy(tex);
    item->model3d = g_rect_model;
    copy_color(NULL, item->color);
    item->effects = effects;
    DL_APPEND(rend->items, item);
}

void render_img2(renderer_t *rend,
                 const uint8_t *data, int w, int h, int bpp,
                 const float mat[4][4], int effects)
{
    // Experimental for the moment!
    // Same as render_img, but we flip the texture!
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    mat ? mat4_copy(mat, item->mat) : mat4_set_identity(item->mat);
    mat4_iscale(item->mat, 1, -1, 1);
    item->proj_screen = effects & EFFECT_PROJ_SCREEN;
    item->tex = texture_new_from_buf(data, w, h, bpp,
            (effects & EFFECT_ANTIALIASING) ? 0 : TF_NEAREST);
    item->model3d = g_rect_model;
    copy_color(NULL, item->color);
    item->effects = effects;
    DL_APPEND(rend->items, item);
}


void render_rect(renderer_t *rend, const float plane[4][4], int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    assert((effects & EFFECT_STRIP) == effects);
    item->type = ITEM_MODEL3D;
    mat4_copy(plane, item->mat);
    item->model3d = g_wire_rect_model;
    copy_color(NULL, item->color);
    item->proj_screen = true;
    item->effects = effects;
    DL_APPEND(rend->items, item);
}

// Return a plane whose u vector is the line ab.
static void line_create_plane(const float a[3], const float b[3],
                              float out[4][4])
{
    mat4_set_identity(out);
    vec3_copy(a, out[3]);
    vec3_sub(b, a, out[0]);
}

void render_line(renderer_t *rend, const float a[3], const float b[3],
                 const uint8_t color[4], int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->model3d = g_line_model;
    line_create_plane(a, b, item->mat);
    copy_color(color, item->color);
    mat4_itranslate(item->mat, 0.5, 0, 0);
    item->proj_screen = effects & EFFECT_PROJ_SCREEN;
    DL_APPEND(rend->items, item);
}

void render_box(renderer_t *rend, const float box[4][4],
                const uint8_t color[4], int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    assert((effects & (EFFECT_STRIP | EFFECT_WIREFRAME | EFFECT_SEE_BACK |
                       EFFECT_GRID)) == effects);
    item->type = ITEM_MODEL3D;
    mat4_copy(box, item->mat);
    copy_color(color, item->color);
    item->effects = effects;
    item->model3d = (effects & EFFECT_WIREFRAME) ? g_wire_cube_model :
                                                   g_cube_model;
    DL_APPEND(rend->items, item);
}

void render_sphere(renderer_t *rend, const float mat[4][4])
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    mat4_copy(mat, item->mat);
    item->model3d = g_sphere_model;
    DL_APPEND(rend->items, item);
}

static float item_sort_value(const render_item_t *a)
{
    // First, non transparent full models (like the image box).
    if ((a->type == ITEM_MODEL3D) && !(a->effects & EFFECT_WIREFRAME) &&
            !(a->tex) && (a->color[3] == 255)) return 0;

    // Then all the non transparent meshes.
    if (a->type == ITEM_MESH && a->material.base_color[3] == 1) return 2;

    // Then all the transparent meshes.
    if (a->type == ITEM_MESH && a->material.base_color[3] < 1) return 4;

    // Then the grids.
    if (a->type == ITEM_GRID) return 5;

    // Then all the rest.
    return 10;
}

static int item_sort_cmp(const render_item_t *a, const render_item_t *b)
{
    return cmp(item_sort_value(a), item_sort_value(b));
}


static void render_shadow_map(renderer_t *rend, float shadow_mvp[4][4])
{
    render_item_t *item;
    float rect[6], light_dir[3];
    int effects;
    // Create a renderer looking at the scene from the light.
    compute_shadow_map_box(rend, rect);
    float bias_mat[4][4] = {{0.5, 0.0, 0.0, 0.0},
                            {0.0, 0.5, 0.0, 0.0},
                            {0.0, 0.0, 0.5, 0.0},
                            {0.5, 0.5, 0.5, 1.0}};
    float ret[4][4];
    renderer_t srend = {};
    get_light_dir(rend, light_dir);
    mat4_lookat(srend.view_mat, light_dir, VEC(0, 0, 0), VEC(0, 1, 0));
    mat4_ortho(srend.proj_mat,
               rect[0], rect[1], rect[2], rect[3], rect[4], rect[5]);

    // Generate the depth buffer.
    if (!g_shadow_map_fbo) {
        GL(glGenFramebuffers(1, &g_shadow_map_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, g_shadow_map_fbo));
        #ifndef GLES2
        GL(glDrawBuffer(GL_NONE));
        GL(glReadBuffer(GL_NONE));
        #endif
        g_shadow_map = texture_new_surface(2048, 2048, 0);
        GL(glBindTexture(GL_TEXTURE_2D, g_shadow_map->tex));
        GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                        2048, 2048, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_TEXTURE_2D, g_shadow_map->tex, 0));
    }

    GL(glBindFramebuffer(GL_FRAMEBUFFER, g_shadow_map_fbo));
    GL(glViewport(0, 0, 2048, 2048));
    GL(glClear(GL_DEPTH_BUFFER_BIT));

    DL_FOREACH(rend->items, item) {
        if (item->type == ITEM_MESH) {
            effects = (item->effects & EFFECT_MARCHING_CUBES);
            effects |= EFFECT_SHADOW_MAP;
            render_mesh_(&srend, item->mesh, &item->material, effects, NULL);
        }
    }
    mat4_copy(bias_mat, ret);
    mat4_imul(ret, srend.proj_mat);
    mat4_imul(ret, srend.view_mat);
    mat4_copy(ret, shadow_mvp);
}

static void render_background(renderer_t *rend, const uint8_t col[4])
{
    gl_shader_t *shader;
    typedef struct {
        int8_t  pos[3]       __attribute__((aligned(4)));
        float   color[4]     __attribute__((aligned(4)));
    } vertex_t;
    vertex_t vertices[4];
    float c1[4], c2[4];

    if (!col || col[3] == 0) {
        GL(glClearColor(0, 0, 0, 0));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        return;
    }
    GL(glClear(GL_DEPTH_BUFFER_BIT));
    // Add a small gradient to the color.
    vec4_set(c1, col[0] / 255., col[1] / 255., col[2] / 255., col[3] / 255.);
    vec4_set(c2, col[0] / 255., col[1] / 255., col[2] / 255., col[3] / 255.);
    vec3_iadd(c1, VEC(+0.2, +0.2, +0.2));
    vec3_iadd(c2, VEC(-0.2, -0.2, -0.2));

    vertices[0] = (vertex_t){{-1, -1, 0}, {c1[0], c1[1], c1[2], c1[3]}};
    vertices[1] = (vertex_t){{+1, -1, 0}, {c1[0], c1[1], c1[2], c1[3]}};
    vertices[2] = (vertex_t){{+1, +1, 0}, {c2[0], c2[1], c2[2], c2[3]}};
    vertices[3] = (vertex_t){{-1, +1, 0}, {c2[0], c2[1], c2[2], c2[3]}};

    shader = shader_get("background", NULL, ATTR_NAMES, shader_init);
    GL(glUseProgram(shader->prog));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, g_background_array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
                    vertices, GL_DYNAMIC_DRAW));

    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(0, 3, GL_BYTE, false, sizeof(vertex_t),
                             (void*)(intptr_t)offsetof(vertex_t, pos)));
    GL(glEnableVertexAttribArray(4));
    GL(glVertexAttribPointer(4, 4, GL_FLOAT, false, sizeof(vertex_t),
                             (void*)(intptr_t)offsetof(vertex_t, color)));
    GL(glDepthMask(false));
    GL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));
    GL(glDepthMask(true));
}

void render_submit(renderer_t *rend, const float viewport[4],
                   const uint8_t clear_color[4])
{
    render_item_t *item, *tmp;
    float shadow_mvp[4][4];
    const float s = rend->scale;
    bool shadow = rend->settings.shadow &&
        !(rend->settings.effects & (EFFECT_RENDER_POS | EFFECT_SHADOW_MAP));

    if (shadow) {
        GL(glDisable(GL_SCISSOR_TEST));
        render_shadow_map(rend, shadow_mvp);
    }

    GL(glBindFramebuffer(GL_FRAMEBUFFER, rend->fbo));
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glViewport(viewport[0] * s, viewport[1] * s,
                  viewport[2] * s, viewport[3] * s));
    GL(glScissor(viewport[0] * s, viewport[1] * s,
                 viewport[2] * s, viewport[3] * s));
    GL(glLineWidth(rend->scale));
    render_background(rend, clear_color);

    DL_SORT(rend->items, item_sort_cmp);
    DL_FOREACH_SAFE(rend->items, item, tmp) {
        switch (item->type) {
        case ITEM_MESH:
            render_mesh_(rend, item->mesh, &item->material, item->effects,
                         shadow_mvp);
            mesh_delete(item->mesh);
            break;
        case ITEM_MODEL3D:
            render_model_item(rend, item, viewport);
            break;
        case ITEM_GRID:
            render_grid_item(rend, item);
            break;
        default:
            assert(false);
        }
        DL_DELETE(rend->items, item);
        texture_delete(item->tex);
        free(item);
    }
    assert(rend->items == NULL);
}

void render_on_low_memory(renderer_t *rend)
{
    cache_clear(g_items_cache);
}
