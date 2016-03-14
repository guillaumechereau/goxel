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
    int id;
    int effects;
} block_item_key_t;

struct render_item_t
{
    UT_hash_handle  hh;             // Handle into the global hash.
    render_item_t   *next, *prev;   // The rendering queue.
    int             type;
    block_item_key_t key;

    union {
        mesh_t          *mesh;
        mat4_t          mat;
    };
    vec3_t          grid;
    uvec4b_t        color;
    bool            strip;  // XXX: move into effects?
    model3d_t       *model3d;
    texture_t       *tex;
    int             effects;
    int             last_used_frame;

    GLuint      vertex_buffer;
    int         size;           // 4 (quads) or 3 (triangles).
    int         nb_elements;    // Number of quads or triangle.
};

// The buffered item hash table.  For the moment it is only used of the blocks.
static render_item_t *g_items = NULL;

static const int BATCH_QUAD_COUNT = 1 << 14;
static model3d_t *g_cube_model;
static model3d_t *g_line_model;
static model3d_t *g_wire_cube_model;
static model3d_t *g_sphere_model;
static model3d_t *g_grid_model;
static model3d_t *g_rect_model;

// All the shaders code is at the bottom of the file.
static const char *VSHADER;
static const char *FSHADER;
static const char *POS_DATA_VSHADER;
static const char *POS_DATA_FSHADER;

typedef struct {
    GLint prog;
    GLint u_model_l;
    GLint u_view_l;
    GLint u_proj_l;
    GLint u_l_dir_l;
    GLint u_l_int_l;
    GLint u_m_amb_l;
    GLint u_m_dif_l;
    GLint u_m_spe_l;
    GLint u_m_shi_l;
    GLint u_m_smo_l;
    GLint u_bshadow_tex_l;
    GLint u_bshadow_l;
    GLint u_bump_tex_l;
    GLint u_block_id_l;
    GLint u_pos_scale_l;
} prog_t;

static prog_t prog_render, prog_pos_data;
static GLuint index_buffer;
static GLuint g_border_tex;
static GLuint g_bump_tex;

#define OFFSET(n) offsetof(voxel_vertex_t, n)

// The list of all the attributes used by the shaders.
static const struct {
    const char *name;
    int size;
    int type;
    int norm;
    int offset;
} ATTRIBUTES[] = {
    {"a_pos",           3, GL_BYTE,            false, OFFSET(pos)},
    {"a_normal",        3, GL_BYTE,            false, OFFSET(normal)},
    {"a_color",         4, GL_UNSIGNED_BYTE,   true,  OFFSET(color)},
    {"a_pos_data",      2, GL_UNSIGNED_BYTE,   true,  OFFSET(pos_data)},
    {"a_uv",            2, GL_UNSIGNED_BYTE,   true,  OFFSET(uv)},
    {"a_bump_uv",       2, GL_UNSIGNED_BYTE,   false, OFFSET(bump_uv)},
    {"a_bshadow_uv",    2, GL_UNSIGNED_BYTE,   false, OFFSET(bshadow_uv)},
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

static float get_border_dist(float x, float y, int mask)
{
    const vec2_t corners[4] = {
        vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
    const vec2_t normals[4] = {
        vec2(0, 1), vec2(-1, 0), vec2(0, -1), vec2(1, 0)};
    float ret = 1;
    int i;
    vec2_t p = vec2(x, y);
    for (i = 0; i < 4; i++) {
        if (mask & (1 << i))        // Corners.
            ret = min(ret, vec2_dist(p, corners[i]));
        if (mask & (0x10 << i))  // Edges.
            ret = min(ret, vec2_dot(normals[i], vec2_sub(p, corners[i])));
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

static void bump_img_fill(uvec3b_t *data, int x, int y, int w, int h,
                          uvec3b_t v)
{
    int i, j;
    for (j = y; j < y + h; j++)
    for (i = x; i < x + w; i++) {
        data[j * 256 + i] = v;
    }
}

static uvec3b_t bump_neighbor_value(int f, int e)
{
    extern const vec3b_t FACES_NORMALS[6];
    extern const int FACES_NEIGHBORS[6][4];
    assert(e >= 0 && e < 4);
    assert(f >= 0 && f < 6);
    vec3b_t n0, n1;
    n0 = FACES_NORMALS[f];
    n1 = FACES_NORMALS[FACES_NEIGHBORS[f][e]];
    return uvec3b(
        (int)(n0.x + n1.x) * 127 / 2 + 127,
        (int)(n0.y + n1.y) * 127 / 2 + 127,
        (int)(n0.z + n1.z) * 127 / 2 + 127
    );
}

static void set_bump_block(uvec3b_t *data, int bx, int by, int f, int mask)
{
    extern const vec3b_t FACES_NORMALS[6];
    uvec3b_t v = uvec3b(127 + FACES_NORMALS[f].x * 127,
                        127 + FACES_NORMALS[f].y * 127,
                        127 + FACES_NORMALS[f].z * 127);
    bump_img_fill(data, bx * 16, by * 16, 16, 16, v);
    if (mask & 1) bump_img_fill(data, bx * 16, by * 16, 16, 1,
                                bump_neighbor_value(f, 0));
    if (mask & 2) bump_img_fill(data, bx * 16 + 15, by * 16, 1, 16,
                                bump_neighbor_value(f, 1));
    if (mask & 4) bump_img_fill(data, bx * 16, by * 16 + 15, 16, 1,
                                bump_neighbor_value(f, 2));
    if (mask & 8) bump_img_fill(data, bx * 16, by * 16, 1, 16,
                                bump_neighbor_value(f, 3));
}

static void init_bump_texture(void)
{
    uvec3b_t *data;
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

static void init_prog(prog_t *prog, const char *vshader, const char *fshader)
{
    char include[128];
    int attr;
    sprintf(include, "#define VOXEL_TEXTURE_SIZE %d.0\n",
                     VOXEL_TEXTURE_SIZE);
    prog->prog = create_program(vshader, fshader, include);
    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++) {
        GL(glBindAttribLocation(prog->prog, attr, ATTRIBUTES[attr].name));
    }
    GL(glLinkProgram(prog->prog));
#define UNIFORM(x) GL(prog->x##_l = glGetUniformLocation(prog->prog, #x))
    UNIFORM(u_model);
    UNIFORM(u_view);
    UNIFORM(u_proj);
    UNIFORM(u_l_dir);
    UNIFORM(u_l_int);
    UNIFORM(u_m_amb);
    UNIFORM(u_m_dif);
    UNIFORM(u_m_spe);
    UNIFORM(u_m_shi);
    UNIFORM(u_m_smo);
    UNIFORM(u_bshadow_tex);
    UNIFORM(u_bshadow);
    UNIFORM(u_bump_tex);
    UNIFORM(u_block_id);
    UNIFORM(u_pos_scale);
#undef UNIFORM
}

void render_init()
{
    LOG_D("render init");
    init_prog(&prog_render, VSHADER, FSHADER);
    init_prog(&prog_pos_data, POS_DATA_VSHADER, POS_DATA_FSHADER);
    GL(glUseProgram(prog_render.prog));
    GL(glUniform1i(prog_render.u_bshadow_tex_l, 0));
    GL(glUniform1i(prog_render.u_bump_tex_l, 1));

    GL(glGenBuffers(1, &index_buffer));

    // 6 vertices (2 triangles) per face.
    uint16_t *index_array = NULL;
    int i;
    index_array = calloc(BATCH_QUAD_COUNT * 6, sizeof(*index_array));
    for (i = 0; i < BATCH_QUAD_COUNT * 6; i++) {
        index_array[i] = (i / 6) * 4 + ((int[]){0, 1, 2, 2, 3, 0})[i % 6];
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, BATCH_QUAD_COUNT * 6 * 2,
                    index_array, GL_STATIC_DRAW));
    free(index_array);
    init_border_texture();
    init_bump_texture();

    g_cube_model = model3d_cube();
    g_line_model = model3d_line();
    g_wire_cube_model = model3d_wire_cube();
    g_sphere_model = model3d_sphere(16, 16);
    g_grid_model = model3d_grid(8, 8);
    g_rect_model = model3d_rect();
}

void render_deinit(void)
{
    delete_program(prog_render.prog);
    memset(&prog_render, 0, sizeof(prog_render));
    GL(glDeleteBuffers(1, &index_buffer));
    index_buffer = 0;
}

// A global buffer large enough to contain all the vertices for any block.
static voxel_vertex_t* g_vertices_buffer = NULL;

static render_item_t *get_item_for_block(const block_t *block, int effects)
{
    render_item_t *item;
    const int effects_mask = EFFECT_BORDERS | EFFECT_BORDERS_ALL |
                             EFFECT_MARCHING_CUBES;
    block_item_key_t key = {
        .id = block->data->id,
        .effects = effects & effects_mask,
    };
    HASH_FIND(hh, g_items, &key, sizeof(key), item);
    if (item) goto end;

    item = calloc(1, sizeof(*item));
    item->key = key;
    GL(glGenBuffers(1, &item->vertex_buffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, item->vertex_buffer));
    if (!g_vertices_buffer)
        g_vertices_buffer = calloc(
                BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE * 6 * 4,
                sizeof(*g_vertices_buffer));
    item->nb_elements = block_generate_vertices(block->data, effects,
                                                g_vertices_buffer);
    item->size = (effects & EFFECT_MARCHING_CUBES) ? 3 : 4;
    if (item->nb_elements > BATCH_QUAD_COUNT) {
        LOG_W("Too many quads!");
        item->nb_elements = BATCH_QUAD_COUNT;
    }
    if (item->nb_elements != 0) {
        GL(glBufferData(GL_ARRAY_BUFFER,
                item->nb_elements * item->size * sizeof(*g_vertices_buffer),
                g_vertices_buffer, GL_STATIC_DRAW));
    }
    HASH_ADD(hh, g_items, key, sizeof(key), item);
end:
    item->last_used_frame = goxel()->frame_count;
    return item;
}

static void render_block_(renderer_t *rend, block_t *block, int effects,
                          prog_t *prog, mat4_t *model)
{
    render_item_t *item;
    vec2_t block_id_vec;
    mat4_t block_model;
    int attr;

    item = get_item_for_block(block, effects);
    if (item->nb_elements == 0) return;
    GL(glBindBuffer(GL_ARRAY_BUFFER, item->vertex_buffer));

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++) {
        GL(glVertexAttribPointer(attr,
                                 ATTRIBUTES[attr].size,
                                 ATTRIBUTES[attr].type,
                                 ATTRIBUTES[attr].norm,
                                 sizeof(voxel_vertex_t),
                                 (void*)(intptr_t)ATTRIBUTES[attr].offset));
    }

    block_id_vec = vec2(
        (block->id >> 8)   / 255.0,
        (block->id & 0xff) / 255.0
    );
    GL(glUniform2fv(prog->u_block_id_l, 1, block_id_vec.v));

    block_model = *model;
    mat4_itranslate(&block_model, block->pos.x, block->pos.y, block->pos.z);
    mat4_itranslate(&block_model, -BLOCK_SIZE / 2,
                                  -BLOCK_SIZE / 2,
                                  -BLOCK_SIZE / 2);
    GL(glUniformMatrix4fv(prog->u_model_l, 1, 0, block_model.v));
    if (item->size == 4) {
        // Use indexed triangles.
        GL(glDrawElements(GL_TRIANGLES, item->nb_elements * 6,
                          GL_UNSIGNED_SHORT, 0));
    } else {
        GL(glDrawArrays(GL_TRIANGLES, 0, item->nb_elements * item->size));
    }
}

static void render_mesh_(renderer_t *rend, mesh_t *mesh, int effects)
{
    prog_t *prog;
    block_t *block;
    mat4_t model = mat4_identity;
    int attr;
    float pos_scale = 1.0f;
    vec4_t light_dir = vec4_zero;
    light_dir.xyz = rend->light.direction;
    if (!rend->light.fixed)
        light_dir = mat4_mul_vec(rend->view_mat, light_dir);
    if (effects & EFFECT_MARCHING_CUBES)
        pos_scale = 1.0 / MC_VOXEL_SUB_POS;

    prog = effects & EFFECT_RENDER_POS ? &prog_pos_data : &prog_render;

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
        vec3_imul(&light_dir.xyz, -0.5);
    }
    if (effects & EFFECT_SEMI_TRANSPARENT) {
        GL(glEnable(GL_BLEND));
        GL(glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR));
        GL(glBlendColor(0.75, 0.75, 0.75, 0.75));
    }

    GL(glUseProgram(prog->prog));

    GL(glUniformMatrix4fv(prog->u_proj_l, 1, 0, rend->proj_mat.v));
    GL(glUniformMatrix4fv(prog->u_view_l, 1, 0, rend->view_mat.v));
    GL(glUniform1i(prog->u_bshadow_tex_l, 0));
    GL(glUniform1i(prog->u_bump_tex_l, 1));
    GL(glUniform3fv(prog->u_l_dir_l, 1, light_dir.v));
    GL(glUniform1f(prog->u_l_int_l, rend->light.intensity));
    GL(glUniform1f(prog->u_m_amb_l, rend->settings.ambient));
    GL(glUniform1f(prog->u_m_dif_l, rend->settings.diffuse));
    GL(glUniform1f(prog->u_m_spe_l, rend->settings.specular));
    GL(glUniform1f(prog->u_m_shi_l, rend->settings.shininess));
    GL(glUniform1f(prog->u_m_smo_l, rend->settings.smoothness));
    GL(glUniform1f(prog->u_bshadow_l, rend->settings.border_shadow));
    GL(glUniform1f(prog->u_pos_scale_l, pos_scale));

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glEnableVertexAttribArray(attr));

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer));

    MESH_ITER_BLOCKS(mesh, block) {
        render_block_(rend, block, effects, prog, &model);
    }

    for (attr = 0; attr < ARRAY_SIZE(ATTRIBUTES); attr++)
        GL(glDisableVertexAttribArray(attr));

    if (effects & EFFECT_SEE_BACK) {
        effects &= ~EFFECT_SEE_BACK;
        effects |= EFFECT_SEMI_TRANSPARENT;
        render_mesh_(rend, mesh, effects);
    }
}

void render_mesh(renderer_t *rend, const mesh_t *mesh, int effects)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MESH;
    item->mesh = mesh_copy(mesh);
    item->effects = effects | rend->settings.effects | EFFECT_SMOOTH;
    // With EFFECT_RENDER_POS we need to remove some effects.
    if (item->effects & EFFECT_RENDER_POS)
        item->effects &= ~(EFFECT_SEMI_TRANSPARENT | EFFECT_SEE_BACK |
                           EFFECT_MARCHING_CUBES);
    DL_APPEND(rend->items, item);
}

static void render_model_item(renderer_t *rend, const render_item_t *item)
{
    mat4_t view = rend->view_mat;
    mat4_imul(&view, item->mat);
    model3d_render(item->model3d, &view, &rend->proj_mat, &item->color,
                   item->tex, item->strip, 0, NULL);
}

static void render_grid_item(renderer_t *rend, const render_item_t *item)
{
    vec4_t norm;
    int x, y, n;
    mat4_t view2, view3;
    view2 = rend->view_mat;
    vec3_t center;

    mat4_imul(&view2, item->mat);
    norm = mat4_mul_vec(mat4_mul(rend->proj_mat, view2), vec4(0, 0, 1, 0));
    mat4_itranslate(&view2, 0, 0, 0.5 * sign(norm.z));
    center = mat4_mul_vec3(view2, vec3_zero);
    n = 3;
    for (y = -n; y <= n; y++)
    for (x = -n; x <= n; x++) {
        view3 = mat4_translate(view2, x, y, 0);
        model3d_render(item->model3d, &view3, &rend->proj_mat, &item->color,
                       false, NULL, 32, &center);
    }
}

void render_plane(renderer_t *rend, const plane_t *plane,
                  const uvec4b_t *color)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_GRID;
    item->mat = plane->mat;
    mat4_itranslate(&item->mat, 0.5, 0.5, 0);
    mat4_iscale(&item->mat, 8, 8, 1);
    item->model3d = g_grid_model;
    item->color = *color;
    DL_APPEND(rend->items, item);
}

void render_img(renderer_t *rend, texture_t *tex, const mat4_t *mat)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->mat = *mat;
    item->tex = texture_copy(tex);
    item->model3d = g_rect_model;
    item->color = uvec4b(255, 255, 255, 255);
    DL_APPEND(rend->items, item);
}

// Return a plane whose u vector is the line ab.
static plane_t line_create_plane(const vec3_t *a, const vec3_t *b)
{
    plane_t ret;
    ret.mat = mat4_identity;
    ret.p = *a;
    ret.u = vec3_sub(*b, *a);
    return ret;
}

void render_line(renderer_t *rend, const vec3_t *a, const vec3_t *b,
                 const uvec4b_t *color)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->model3d = g_line_model;
    item->mat = line_create_plane(a, b).mat;
    item->color = color ? *color : HEXCOLOR(0xffffffff);
    mat4_itranslate(&item->mat, 0.5, 0, 0);
    DL_APPEND(rend->items, item);
}

void render_box(renderer_t *rend, const box_t *box, bool solid,
                const uvec4b_t *color, bool strip)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->mat = box->mat;
    item->color = color ? *color : HEXCOLOR(0xffffffff);
    item->strip = strip;
    item->model3d = solid ? g_cube_model : g_wire_cube_model;
    DL_APPEND(rend->items, item);
}

void render_sphere(renderer_t *rend, const mat4_t *mat)
{
    render_item_t *item = calloc(1, sizeof(*item));
    item->type = ITEM_MODEL3D;
    item->mat = *mat;
    item->model3d = g_sphere_model;
    DL_APPEND(rend->items, item);
}

// Evict item from g_items to save memory.
static void cleanup_buffer(void)
{
    // XXX: do not evict all the items like that.
    render_item_t *item, *tmp;
    HASH_ITER(hh, g_items, item, tmp) {
        if (item->type == ITEM_BLOCK) {
            if (item->last_used_frame < goxel()->frame_count) {
                GL(glDeleteBuffers(1, &item->vertex_buffer));
                HASH_DEL(g_items, item);
                free(item);
            }
        }
    }
}

static int item_sort_value(const render_item_t *a)
{
    switch (a->type) {
        case ITEM_MESH:     return 0;
        case ITEM_MODEL3D:  return 1;
        case ITEM_GRID:     return 2;
        default:            return 0;
    }
}

static int item_sort_cmp(const render_item_t *a, const render_item_t *b)
{
    return sign(item_sort_value(a) - item_sort_value(b));
}

void render_render(renderer_t *rend)
{
    PROFILED
    render_item_t *item, *tmp;
    DL_SORT(rend->items, item_sort_cmp);
    DL_FOREACH_SAFE(rend->items, item, tmp) {
        item->last_used_frame = goxel()->frame_count;
        switch (item->type) {
        case ITEM_MESH:
            render_mesh_(rend, item->mesh, item->effects);
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
    cleanup_buffer();
}

int render_get_default_settings(int i, char **name, render_settings_t *out)
{
    if (!out) return 5;

    *out = (render_settings_t) {
        .border_shadow = 0.4,
        .ambient = 0.1,
        .diffuse = 0.9,
        .specular = 0.4,
        .shininess = 2.0,
        .smoothness = 0.0,
        .effects = EFFECT_BORDERS,
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
            break;
        case 4:
            if (name) *name = "Marching";
            out->smoothness = 1.0;
            out->effects = EFFECT_MARCHING_CUBES;
            break;
    }
    return 5;
}


// #### All the shaders code #######

/*
 * I followed those name conventions.  All the vectors are expressed in eye
 * coordinates.
 *
 *         reflection         light source
 *
 *               r              s
 *                 ^         ^
 *                  \   n   /
 *  eye              \  ^  /
 *     v  <....       \ | /
 *              -----__\|/
 *                  ----+----
 *
 *
 */

static const char *VSHADER =
    "                                                                   \n"
    "attribute vec3 a_pos;                                              \n"
    "attribute vec3 a_normal;                                           \n"
    "attribute vec4 a_color;                                            \n"
    "attribute vec2 a_bshadow_uv;                                       \n"
    "attribute vec2 a_bump_uv;   // bump tex base coordinates [0,255]   \n"
    "attribute vec2 a_uv;        // uv coordinates [0,1]                \n"
    "uniform   mat4 u_model;                                            \n"
    "uniform   mat4 u_view;                                             \n"
    "uniform   mat4 u_proj;                                             \n"
    "uniform   float u_pos_scale;                                       \n"
    "                                                                   \n"
    "varying   vec3 v_pos;                                              \n"
    "varying   vec4 v_color;                                            \n"
    "varying   vec2 v_bshadow_uv;                                       \n"
    "varying   vec2 v_uv;                                               \n"
    "varying   vec2 v_bump_uv;                                          \n"
    "varying   vec3 v_normal;                                           \n"
    "                                                                   \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    v_normal = a_normal;                                           \n"
    "    v_color = a_color;                                             \n"
    "    v_bshadow_uv = (a_bshadow_uv + 0.5) /                          \n"
    "                          (16.0 * VOXEL_TEXTURE_SIZE);             \n"
    "    v_pos = a_pos * u_pos_scale;                                   \n"
    "    v_uv = a_uv;                                                   \n"
    "    v_bump_uv = a_bump_uv;                                         \n"
    "    gl_Position = u_proj * u_view * u_model * vec4(v_pos, 1.0);    \n"
    "}                                                                  \n"
;

static const char *FSHADER =
    "                                                                   \n"
    "#ifdef GL_ES                                                       \n"
    "precision highp float;                                             \n"
    "#endif                                                             \n"
    "                                                                   \n"
    "uniform   mat4 u_model;                                            \n"
    "uniform   mat4 u_view;                                             \n"
    "uniform   mat4 u_proj;                                             \n"
    "                                                                   \n"
    "// Light parameters                                                \n"
    "uniform   vec3 u_l_dir;                                            \n"
    "uniform  float u_l_int;                                            \n"
    "// Material parameters                                             \n"
    "uniform  float u_m_amb; // Ambient light coef.                     \n"
    "uniform  float u_m_dif; // Diffuse light coef.                     \n"
    "uniform  float u_m_spe; // Specular light coef.                    \n"
    "uniform  float u_m_shi; // Specular light shininess.               \n"
    "uniform  float u_m_smo; // Smoothness.                             \n"
    "                                                                   \n"
    "uniform sampler2D u_bshadow_tex;                                   \n"
    "uniform sampler2D u_bump_tex;                                      \n"
    "uniform float u_bshadow;                                           \n"
    "                                                                   \n"
    "varying lowp vec3 v_pos;                                           \n"
    "varying lowp vec4 v_color;                                         \n"
    "varying lowp vec2 v_uv;                                            \n"
    "varying lowp vec2 v_bshadow_uv;                                    \n"
    "varying lowp vec2 v_bump_uv;                                       \n"
    "varying lowp vec3 v_normal;                                        \n"
    "                                                                   \n"
    "vec2 uv, bump_uv;                                                  \n"
    "vec3 n, s, r, v, bump;                                             \n"
    "float s_dot_n;                                                     \n"
    "float l_amb, l_dif, l_spe;                                         \n"
    "float bshadow;                                                     \n"
    "                                                                   \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    // clamp uv so to prevent overflow with multismapling.         \n"
    "    uv = clamp(v_uv, 0.0, 1.0);                                    \n"
    "    s = normalize(u_l_dir);                                        \n"
    "    n = normalize((u_view * u_model * vec4(v_normal, 0.0)).xyz);   \n"
    "    bump_uv = (v_bump_uv + 0.5 + uv * 15.0) / 256.0;               \n"
    "    bump = texture2D(u_bump_tex, bump_uv).xyz - 0.5;               \n"
    "    bump = normalize((u_view * u_model * vec4(bump, 0.0)).xyz);    \n"
    "    n = mix(bump, n, u_m_smo);                                     \n"
    "    s_dot_n = dot(s, n);                                           \n"
    "    l_dif = u_m_dif * max(0.0, s_dot_n);                           \n"
    "    l_amb = u_m_amb;                                               \n"
    "                                                                   \n"
    "    // Specular light.                                             \n"
    "    v = normalize(-(u_view * u_model * vec4(v_pos, 1.0)).xyz);     \n"
    "    r = reflect(-s, n);                                            \n"
    "    l_spe = 0.0;                                                   \n"
    "    if (s_dot_n > 0.0)                                             \n"
    "       l_spe = u_m_spe * pow(max(dot(r, v), 0.0), u_m_shi);        \n"
    "                                                                   \n"
    "                                                                   \n"
    "    bshadow = texture2D(u_bshadow_tex, v_bshadow_uv).r;            \n"
    "    bshadow = sqrt(bshadow);                                       \n"
    "    bshadow = mix(1.0, bshadow, u_bshadow);                        \n"
    "    gl_FragColor = v_color;                                        \n"
    "    gl_FragColor.rgb *= (l_dif + l_amb + l_spe) * u_l_int;         \n"
    "    gl_FragColor.rgb *= bshadow;                                   \n"
    "}                                                                  \n"
;

static const char *POS_DATA_VSHADER =
    "                                                                   \n"
    "attribute vec3 a_pos;                                              \n"
    "attribute vec2 a_pos_data;                                         \n"
    "uniform   mat4 u_model;                                            \n"
    "uniform   mat4 u_view;                                             \n"
    "uniform   mat4 u_proj;                                             \n"
    "varying   vec2 v_pos_data;                                         \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    vec3 pos = a_pos;                                              \n"
    "    gl_Position = u_proj * u_view * u_model * vec4(pos, 1.0);      \n"
    "    v_pos_data = a_pos_data;                                       \n"
    "}                                                                  \n"
;

static const char *POS_DATA_FSHADER =
    "                                                                 \n"
    "#ifdef GL_ES                                                     \n"
    "precision highp float;                                           \n"
    "#endif                                                           \n"
    "                                                                 \n"
    "varying lowp vec2 v_pos_data;                                    \n"
    "uniform vec2 u_block_id;                                         \n"
    "                                                                 \n"
    "void main()                                                      \n"
    "{                                                                \n"
    "    gl_FragColor.rg = v_pos_data;                                \n"
    "    gl_FragColor.ba = u_block_id;                                \n"
    "}                                                                \n"
;
