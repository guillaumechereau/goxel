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
    ITEM_BLOCK,
    ITEM_MESH,
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

typedef struct uniform {
    char        name[64];
    GLint       size;
    GLenum      type;
    GLint       loc;
} uniform_t;

typedef struct {
    // Those values are used as a key to identify the shader.
    const char *path;
    const char *include;

    GLint prog;
    uniform_t uniforms[32];
} prog_t;

// Static list of programs.  Need to be big enough for all the possible
// shaders we are going to use.
static prog_t g_progs[5] = {};


static GLuint g_index_buffer;
static GLuint g_background_array_buffer;
static GLuint g_border_tex;
static GLuint g_bump_tex;
static GLuint g_shadow_map_fbo;
static texture_t *g_shadow_map; // XXX: the fbo should be part of the tex.

#define OFFSET(n) offsetof(voxel_vertex_t, n)

// The list of all the attributes used by the shaders.
static const struct {
    const char *name;
    int size;
    int type;
    int norm;
    int offset;
} ATTRIBUTES[] = {
    {"a_pos",           3, GL_UNSIGNED_BYTE,   false, OFFSET(pos)},
    {"a_normal",        3, GL_BYTE,            false, OFFSET(normal)},
    {"a_color",         4, GL_UNSIGNED_BYTE,   true,  OFFSET(color)},
    {"a_pos_data",      2, GL_UNSIGNED_BYTE,   true,  OFFSET(pos_data)},
    {"a_uv",            2, GL_UNSIGNED_BYTE,   true,  OFFSET(uv)},
    {"a_bump_uv",       2, GL_UNSIGNED_BYTE,   false, OFFSET(bump_uv)},
    {"a_occlusion_uv",  2, GL_UNSIGNED_BYTE,   false, OFFSET(occlusion_uv)},
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

static bool gl_has_uniform(prog_t *prog, const char *name)
{
    uniform_t *uni;
    for (uni = &prog->uniforms[0]; uni->size; uni++) {
        if (strcmp(uni->name, name) == 0) return true;
    }
    return false;
}

static void gl_update_uniform(
        prog_t *prog, const char *name, ...)
{
    uniform_t *uni;
    va_list args;

    for (uni = &prog->uniforms[0]; uni->size; uni++) {
        if (strcmp(uni->name, name) == 0) break;
    }
    if (!uni->size) return; // No such uniform.

    va_start(args, name);
    switch (uni->type) {
    case GL_INT:
    case GL_SAMPLER_2D:
        GL(glUniform1i(uni->loc, va_arg(args, int)));
        break;
    case GL_FLOAT:
        GL(glUniform1f(uni->loc, va_arg(args, double)));
        break;
    case GL_FLOAT_VEC2:
        GL(glUniform2fv(uni->loc, 1, va_arg(args, const float*)));
        break;
    case GL_FLOAT_VEC3:
        GL(glUniform3fv(uni->loc, 1, va_arg(args, const float*)));
        break;
    case GL_FLOAT_MAT4:
        GL(glUniformMatrix4fv(uni->loc, 1, 0, va_arg(args, const float*)));
        break;
    default:
        assert(false);
    }
    va_end(args);
}

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

static void init_border_texture(void)
{
    const int s = VOXEL_TEXTURE_SIZE;    // the individual tile size.
    uint8_t data[256 * s * s];
    int mask, x, y, ax, ay;
    for (mask = 0; mask < 256; mask++) {
        for (y = 0; y < s; y++) for (x = 0; x < s; x++) {
            ay = mask / 16 * s + y;
            ax = mask % 16 * s + x;
            data[ay * s * 16 + ax] = 255 * get_border_dist(
                    (float)x / s + 0.5 / s, (float)y / s + 0.5 / s, mask);
        }
    }
    GL(glGenTextures(1, &g_border_tex));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, g_border_tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, s * 16, s * 16,
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data));
}

static void bump_img_fill(uint8_t (*data)[3], int x, int y, int w, int h,
                          const uint8_t v[3])
{
    int i, j;
    for (j = y; j < y + h; j++) {
        for (i = x; i < x + w; i++) {
            memcpy(data[j * 256 + i], v, 3);
        }
    }
}

static void bump_neighbor_value(int f, int e, uint8_t out[3])
{
    assert(e >= 0 && e < 4);
    assert(f >= 0 && f < 6);
    const int *n0, *n1;
    n0 = FACES_NORMALS[f];
    n1 = FACES_NORMALS[FACES_NEIGHBORS[f][e]];
    out[0] = (int)(n0[0] + n1[0]) * 127 / 2 + 127;
    out[1] = (int)(n0[1] + n1[1]) * 127 / 2 + 127;
    out[2] = (int)(n0[2] + n1[2]) * 127 / 2 + 127;
}

static void set_bump_block(uint8_t (*data)[3], int bx, int by, int f, int mask)
{
    const uint8_t v[3] = {127 + FACES_NORMALS[f][0] * 127,
                          127 + FACES_NORMALS[f][1] * 127,
                          127 + FACES_NORMALS[f][2] * 127};
    uint8_t nv[3];
    bump_img_fill(data, bx * 16, by * 16, 16, 16, v);
    if (mask & 1) {
        bump_neighbor_value(f, 0, nv);
        bump_img_fill(data, bx * 16, by * 16, 16, 1, nv);
    }
    if (mask & 2) {
        bump_neighbor_value(f, 1, nv);
        bump_img_fill(data, bx * 16 + 15, by * 16, 1, 16, nv);
    }
    if (mask & 4) {
        bump_neighbor_value(f, 2, nv);
        bump_img_fill(data, bx * 16, by * 16 + 15, 16, 1, nv);
    }
    if (mask & 8) {
        bump_neighbor_value(f, 3, nv);
        bump_img_fill(data, bx * 16, by * 16, 1, 16, nv);
    }
}

static void init_bump_texture(void)
{
    uint8_t (*data)[3];
    int i, f, mask;
    data = calloc(1, 256 * 256 * 3);
    for (i = 0; i < 256; i++) {
        f = (i / 16) % 6;
        mask = i % 16;
        set_bump_block(data, i % 16, i / 16, f, mask);
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

static void init_prog(prog_t *prog, const char *path, const char *include)
{
    char include_full[256];
    const char *code;
    int attr, count, i;
    uniform_t *uni;

    sprintf(include_full, "#define VOXEL_TEXTURE_SIZE %d.0\n%s\n",
            VOXEL_TEXTURE_SIZE, include ?: "");
    code = assets_get(path, NULL);
    prog->path = path;
    prog->include = include;
    prog->prog = gl_create_prog(code, code, include_full);
    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++) {
        GL(glBindAttribLocation(prog->prog, attr, ATTRIBUTES[attr].name));
    }
    GL(glLinkProgram(prog->prog));
    GL(glUseProgram(prog->prog));

    GL(glGetProgramiv(prog->prog, GL_ACTIVE_UNIFORMS, &count));
    for (i = 0; i < count; i++) {
        uni = &prog->uniforms[i];
        GL(glGetActiveUniform(prog->prog, i, sizeof(uni->name),
                              NULL, &uni->size, &uni->type, uni->name));
        GL(uni->loc = glGetUniformLocation(prog->prog, uni->name));
    }

    gl_update_uniform(prog, "u_occlusion_tex", 0);
    gl_update_uniform(prog, "u_bump_tex", 1);
    gl_update_uniform(prog, "u_shadow_tex", 2);
}

static prog_t *get_prog(const char *path, const char *include)
{
    int i;
    prog_t *p = NULL;
    for (i = 0; i < ARRAY_SIZE(g_progs); i++) {
        p = &g_progs[i];
        if (!p->path) break;
        if (p->path == path && p->include == include) return p;
    }
    assert(i < ARRAY_SIZE(g_progs));
    init_prog(p, path, include);
    return p;
}

void render_init()
{
    LOG_D("render init");
    GL(glGenBuffers(1, &g_index_buffer));
    GL(glGenBuffers(1, &g_background_array_buffer));

    // 6 vertices (2 triangles) per face.
    uint16_t *index_array = NULL;
    int i;
    index_array = calloc(BATCH_QUAD_COUNT * 6, sizeof(*index_array));
    for (i = 0; i < BATCH_QUAD_COUNT * 6; i++) {
        index_array[i] = (i / 6) * 4 + ((int[]){0, 1, 2, 2, 3, 0})[i % 6];
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, BATCH_QUAD_COUNT * 6 * 2,
                    index_array, GL_STATIC_DRAW));
#ifndef GLES2
    GL(glEnable(GL_LINE_SMOOTH));
#endif

    free(index_array);
    init_border_texture();
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
    int i;
    for (i = 0; i < ARRAY_SIZE(g_progs); i++) {
        if (g_progs[i].prog)
            gl_delete_prog(g_progs[i].prog);
    }
    memset(&g_progs, 0, sizeof(g_progs));
    GL(glDeleteBuffers(1, &g_index_buffer));
    g_index_buffer = 0;
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
        int effects)
{
    render_item_t *item;
    const int effects_mask = EFFECT_BORDERS | EFFECT_BORDERS_ALL |
                             EFFECT_MARCHING_CUBES | EFFECT_SMOOTH |
                             EFFECT_FLAT;
    uint64_t block_data_id;
    int p[3], i, x, y, z;
    block_item_key_t key = {};

    // For the moment we always compute the smooth normal no mater what.
    effects |= EFFECT_SMOOTH;

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
                          int effects, prog_t *prog,
                          const float model[4][4])
{
    render_item_t *item;
    float block_model[4][4];
    int attr;
    float block_id_f[2];

    item = get_item_for_block(mesh, iter, block_pos, effects);
    if (item->nb_elements == 0) return;
    GL(glBindBuffer(GL_ARRAY_BUFFER, item->vertex_buffer));
    if (gl_has_uniform(prog, "u_block_id")) {
        block_id_f[1] = ((block_id >> 8) & 0xff) / 255.0;
        block_id_f[0] = ((block_id >> 0) & 0xff) / 255.0;
        gl_update_uniform(prog, "u_block_id", block_id_f);
    }
    gl_update_uniform(prog, "u_pos_scale", 1.f / item->subdivide);

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
    gl_update_uniform(prog, "u_model", block_model);
    if (item->size == 4) {
        // Use indexed triangles.
        GL(glDrawElements(GL_TRIANGLES, item->nb_elements * 6,
                          GL_UNSIGNED_SHORT, 0));
    } else {
        GL(glDrawArrays(GL_TRIANGLES, 0, item->nb_elements * item->size));
    }

#ifndef GLES2
    if (effects & EFFECT_WIREFRAME) {
        gl_update_uniform(prog, "u_m_amb", 0.0);
        GL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
        if (item->size == 4)
            GL(glDrawElements(GL_TRIANGLES, item->nb_elements * 6,
                              GL_UNSIGNED_SHORT, 0));
        else
            GL(glDrawArrays(GL_TRIANGLES, 0, item->nb_elements * item->size));
        GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
        gl_update_uniform(prog, "u_m_amb", rend->settings.ambient);
    }
#endif
}

static void get_light_dir(const renderer_t *rend, bool model_view,
                          float out[3])
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
    if (model_view)
        mat4_mul_vec4(rend->view_mat, light_dir, light_dir);
    vec3_copy(light_dir, out);
}

void render_get_light_dir(const renderer_t *rend, float out[3])
{
    get_light_dir(rend, false, out);
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

    get_light_dir(rend, false, light_dir);
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

static void render_mesh_(renderer_t *rend, mesh_t *mesh, int effects,
                         const float shadow_mvp[4][4])
{
    prog_t *prog;
    float model[4][4];
    int attr, block_pos[3], block_id;
    float light_dir[3];
    bool shadow = false;
    mesh_iterator_t iter;

    mat4_set_identity(model);
    get_light_dir(rend, true, light_dir);

    if (effects & EFFECT_RENDER_POS)
        prog = get_prog("asset://data/shaders/pos_data.glsl", NULL);
    else if (effects & EFFECT_SHADOW_MAP)
        prog = get_prog("asset://data/shaders/shadow_map.glsl", NULL);
    else {
        shadow = rend->settings.shadow;
        prog = get_prog("asset://data/shaders/mesh.glsl",
                         shadow ? "#define SHADOW" : NULL);
    }

    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LESS));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(GL_BACK));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, g_border_tex));
    GL(glActiveTexture(GL_TEXTURE1));
    GL(glBindTexture(GL_TEXTURE_2D, g_bump_tex));
    GL(glDisable(GL_BLEND));

    if (effects & EFFECT_SEE_BACK) {
        GL(glCullFace(GL_FRONT));
        vec3_imul(light_dir, -0.5);
    }
    if (effects & EFFECT_SEMI_TRANSPARENT) {
        GL(glEnable(GL_BLEND));
        GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR));
        GL(glBlendColor(0.75, 0.75, 0.75, 0.75));
    }

    GL(glUseProgram(prog->prog));

    if (shadow) {
        assert(shadow_mvp);
        GL(glActiveTexture(GL_TEXTURE2));
        GL(glBindTexture(GL_TEXTURE_2D, g_shadow_map->tex));
        gl_update_uniform(prog, "u_shadow_mvp", shadow_mvp);
        gl_update_uniform(prog, "u_shadow_tex", 2);
        gl_update_uniform(prog, "u_shadow_k", rend->settings.shadow);
    }

    gl_update_uniform(prog, "u_proj", rend->proj_mat);
    gl_update_uniform(prog, "u_view", rend->view_mat);
    gl_update_uniform(prog, "u_occlusion_tex", 0);
    gl_update_uniform(prog, "u_bump_tex", 1);
    gl_update_uniform(prog, "u_l_dir", light_dir);
    gl_update_uniform(prog, "u_l_int", rend->light.intensity);
    gl_update_uniform(prog, "u_m_amb", rend->settings.ambient);
    gl_update_uniform(prog, "u_m_dif", rend->settings.diffuse);
    gl_update_uniform(prog, "u_m_spe", rend->settings.specular);
    gl_update_uniform(prog, "u_m_shi", rend->settings.shininess);
    gl_update_uniform(prog, "u_m_smo", rend->settings.smoothness);
    gl_update_uniform(prog, "u_occlusion", rend->settings.border_shadow);

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glEnableVertexAttribArray(attr));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));

    block_id = 1;
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        render_block_(rend, mesh, &iter, block_pos,
                      block_id++, effects, prog, model);
    }
    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glDisableVertexAttribArray(attr));

    if (effects & EFFECT_SEE_BACK) {
        effects &= ~EFFECT_SEE_BACK;
        effects |= EFFECT_SEMI_TRANSPARENT;
        render_mesh_(rend, mesh, effects, shadow_mvp);
    }
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

void render_mesh(renderer_t *rend, const mesh_t *mesh, int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MESH;
    item->mesh = mesh_copy(mesh);
    item->effects = effects | rend->settings.effects;
    // With EFFECT_RENDER_POS we need to remove some effects.
    if (item->effects & EFFECT_RENDER_POS)
        item->effects &= ~(EFFECT_SEMI_TRANSPARENT | EFFECT_SEE_BACK |
                           EFFECT_MARCHING_CUBES);
    DL_APPEND(rend->items, item);
}

static void render_model_item(renderer_t *rend, const render_item_t *item)
{
    float proj[4][4];
    const float (*proj_mat)[4][4];
    const float (*view_mat)[4][4];
    float light[3];

    if (item->proj_screen) {
        mat4_ortho(proj, -0.5, +0.5, -0.5, +0.5, -10, +10);
        proj_mat = &proj;
        view_mat = &mat4_identity;
    } else {
        proj_mat = &rend->proj_mat;
        view_mat = &rend->view_mat;
    }

    if (!(item->effects & EFFECT_WIREFRAME))
        get_light_dir(rend, false, light);

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
                       item->color, NULL, NULL, item->clip_box, 0);
    }
}

void render_grid(renderer_t *rend, const float plane[4][4],
                 const uint8_t color[4], const float clip_box[4][4])
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_GRID;
    mat4_copy(plane, item->mat);
    mat4_iscale(item->mat, 8, 8, 1);
    item->model3d = g_grid_model;
    copy_color(color, item->color);
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
                 const uint8_t color[4])
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->model3d = g_line_model;
    line_create_plane(a, b, item->mat);
    copy_color(color, item->color);
    mat4_itranslate(item->mat, 0.5, 0, 0);
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

static int item_sort_value(const render_item_t *a)
{
    if (a->effects & EFFECT_WIREFRAME) return 20;
    if (a->proj_screen)     return 10;
    switch (a->type) {
        case ITEM_MESH:     return 0;
        case ITEM_MODEL3D:  return 1;
        case ITEM_GRID:     return 2;
        default:            return 0;
    }
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
    get_light_dir(rend, false, light_dir);
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
            render_mesh_(&srend, item->mesh, effects, NULL);
        }
    }
    mat4_copy(bias_mat, ret);
    mat4_imul(ret, srend.proj_mat);
    mat4_imul(ret, srend.view_mat);
    mat4_copy(ret, shadow_mvp);
}

static void render_background(renderer_t *rend, const uint8_t col[4])
{
    prog_t *prog;
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

    prog = get_prog("asset://data/shaders/background.glsl", NULL);
    GL(glUseProgram(prog->prog));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, g_background_array_buffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
                    vertices, GL_DYNAMIC_DRAW));

    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(0, 3, GL_BYTE, false, sizeof(vertex_t),
                             (void*)(intptr_t)offsetof(vertex_t, pos)));
    GL(glEnableVertexAttribArray(2));
    GL(glVertexAttribPointer(2, 4, GL_FLOAT, false, sizeof(vertex_t),
                             (void*)(intptr_t)offsetof(vertex_t, color)));
    GL(glDepthMask(false));
    GL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));
    GL(glDepthMask(true));
}

void render_submit(renderer_t *rend, const int rect[4],
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
    GL(glViewport(rect[0] * s, rect[1] * s, rect[2] * s, rect[3] * s));
    GL(glScissor(rect[0] * s, rect[1] * s, rect[2] * s, rect[3] * s));
    GL(glLineWidth(rend->scale));
    render_background(rend, clear_color);

    DL_SORT(rend->items, item_sort_cmp);
    DL_FOREACH_SAFE(rend->items, item, tmp) {
        switch (item->type) {
        case ITEM_MESH:
            render_mesh_(rend, item->mesh, item->effects, shadow_mvp);
            DL_DELETE(rend->items, item);
            mesh_delete(item->mesh);
            break;
        case ITEM_MODEL3D:
            render_model_item(rend, item);
            DL_DELETE(rend->items, item);
            break;
        case ITEM_GRID:
            render_grid_item(rend, item);
            DL_DELETE(rend->items, item);
            break;
        }
        texture_delete(item->tex);
        free(item);
    }
    assert(rend->items == NULL);
}

int render_get_default_settings(int i, char **name, render_settings_t *out)
{
    if (!out) return 6;

    *out = (render_settings_t) {
        .border_shadow = 0.4,
        .ambient = 0.3,
        .diffuse = 0.6,
        .specular = 0.1,
        .shininess = 2.0,
        .smoothness = 0.0,
        .effects = EFFECT_BORDERS,
        .shadow = 0.3,
    };
    switch (i) {
        case 0:
            if (name) *name = "Borders";
            break;
        case 1:
            if (name) *name = "Cubes";
            out->effects = EFFECT_BORDERS_ALL;
            break;
        case 2:
            if (name) *name = "Plain";
            out->effects = 0;
            break;
        case 3:
            if (name) *name = "Smooth";
            out->effects = 0;
            out->smoothness = 1;
            out->border_shadow = 0;
            out->shadow = 0;
            break;
        case 4:
            if (name) *name = "Half smooth";
            out->smoothness = 0.2;
            break;
        case 5:
            if (name) *name = "Marching";
            out->smoothness = 1.0;
            out->effects = EFFECT_MARCHING_CUBES | EFFECT_FLAT;
            break;
    }
    if (DEFINED(GOXEL_NO_SHADOW)) out->shadow = 0;
    return 5;
}
