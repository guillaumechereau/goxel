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

static const char *VSHADER;
static const char *FSHADER;

typedef struct {
    GLint prog;
    GLint a_pos_l;
    GLint a_color_l;
    GLint a_normal_l;
    GLint a_uv_l;
    GLint u_color_l;
    GLint u_model_l;
    GLint u_proj_l;
    GLint u_tex_l;
    GLint u_fade_l;
    GLint u_fade_center_l;
} prog_t;

static prog_t prog;
static texture_t *g_white_tex = NULL;

static void init_prog(prog_t *prog, const char *vshader, const char *fshader)
{
    prog->prog = create_program(vshader, fshader, NULL);
#define UNIFORM(x) GL(prog->x##_l = glGetUniformLocation(prog->prog, #x))
#define ATTRIB(x)  GL(prog->x##_l = glGetAttribLocation(prog->prog, #x))
    ATTRIB(a_pos);
    ATTRIB(a_color);
    ATTRIB(a_normal);
    ATTRIB(a_uv);
    UNIFORM(u_color);
    UNIFORM(u_model);
    UNIFORM(u_proj);
    UNIFORM(u_tex);
    UNIFORM(u_fade);
    UNIFORM(u_fade_center);
#undef ATTRIB
#undef UNIFORM
    GL(glUseProgram(prog->prog));
    GL(glUniform1i(prog->u_tex_l, 0));
}

static texture_t *create_white_tex(void)
{
    texture_t *tex;
    uint8_t *buffer;
    const int s = 16;
    tex = texture_new_surface(s, s, TF_RGB);
    buffer = malloc(s * s * 3);
    memset(buffer, 255, s * s * 3);
    glBindTexture(GL_TEXTURE_2D, tex->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s, s, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, buffer);
    free(buffer);
    return tex;
}

void model3d_init(void)
{
    init_prog(&prog, VSHADER, FSHADER);
    g_white_tex = create_white_tex();
}

model3d_t *model3d_cube(void)
{
    extern const int FACES_VERTICES[6][4];
    extern const vec3b_t VERTICES_POSITIONS[8];
    extern const vec3b_t FACES_NORMALS[6];
    int f, i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->solid = true;
    vec3b_t p;
    model->nb_vertices = 6 * 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 6; i++) {
            v = (int[]){0, 1, 2, 2, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            model->vertices[f * 6 + i].pos =
                vec3((p.x - 0.5) * 2, (p.y - 0.5) * 2, (p.z - 0.5) * 2);
            model->vertices[f * 6 + i].normal =
                vec3(VEC3_SPLIT(FACES_NORMALS[f]));
            model->vertices[f * 6 + i].color = uvec4b(255, 255, 255, 255);
        }
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_wire_cube(void)
{
    extern const int FACES_VERTICES[6][4];
    extern const vec3b_t VERTICES_POSITIONS[8];
    int f, i, v;
    vec3b_t p;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6 * 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 8; i++) {
            v = (int[]){0, 1, 1, 2, 2, 3, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            model->vertices[f * 8 + i].pos =
                vec3((p.x - 0.5) * 2, (p.y - 0.5) * 2, (p.z - 0.5) * 2);
            model->vertices[f * 8 + i].color = uvec4b(255, 255, 255, 255);
            model->vertices[f * 8 + i].uv = vec2(0.5, 0.5);
        }
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_sphere(int slices, int stacks)
{
    int slice, stack, ind, i;
    float z0, z1, r0, r1, a0, a1;
    model3d_t *model = calloc(1, sizeof(*model));
    model_vertex_t *v;
    model->nb_vertices = slices * stacks * 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    v = model->vertices;;
    for (stack = 0; stack < stacks; stack++) {
        z0 = -1 + stack * 2.0f / stacks;
        z1 = -1 + (stack + 1) * 2.0f / stacks;
        r0 = sqrt(1 - z0 * z0);
        r1 = sqrt(1 - z1 * z1);
        for (slice = 0; slice < slices; slice++) {
            a0 = slice * M_PI * 2 / slices;
            a1 = (slice + 1) * M_PI * 2 / slices;
            ind = (stack * slices + slice) * 6;
            v[ind + 0].pos = vec3(r0 * cos(a0), r0 * sin(a0), z0);
            v[ind + 1].pos = vec3(r0 * cos(a1), r0 * sin(a1), z0);
            v[ind + 2].pos = vec3(r1 * cos(a0), r1 * sin(a0), z1);
            v[ind + 3].pos = vec3(r1 * cos(a1), r1 * sin(a1), z1);
            v[ind + 4].pos = vec3(r1 * cos(a0), r1 * sin(a0), z1);
            v[ind + 5].pos = vec3(r0 * cos(a1), r0 * sin(a1), z0);
            for (i = 0; i < 6; i++) v[ind + i].normal = v[ind + i].pos;

        }
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_grid(int nx, int ny)
{
    model3d_t *model = calloc(1, sizeof(*model));
    model_vertex_t *v;
    int i;
    float x, y;
    bool side;
    const uvec4b_t c  = uvec4b(255, 255, 255, 160);
    const uvec4b_t cb = uvec4b(255, 255, 255, 255);

    model->nb_vertices = (nx + ny + 2) * 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    v = model->vertices;
    for (i = 0; i < nx + 1; i++) {
        side = i == 0 || i == nx;
        x = (float)i / nx - 0.5;
        v[i * 2 + 0].pos = vec3(x, -0.5, 0);
        v[i * 2 + 1].pos = vec3(x, +0.5, 0);
        v[i * 2 + 0].color = side ? cb : c;
        v[i * 2 + 1].color = side ? cb : c;
    }
    v = model->vertices + 2 * nx + 2;
    for (i = 0; i < ny + 1; i++) {
        side = i == 0 || i == ny;
        y = (float)i / ny - 0.5;
        v[i * 2 + 0].pos = vec3(-0.5, y, 0);
        v[i * 2 + 1].pos = vec3(+0.5, y, 0);
        v[i * 2 + 0].color = side ? cb : c;
        v[i * 2 + 1].color = side ? cb : c;
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_line(void)
{
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    model->vertices[0].pos = vec3(-0.5, 0, 0);
    model->vertices[1].pos = vec3(+0.5, 0, 0);
    model->vertices[0].color = uvec4b(255, 255, 255, 255);
    model->vertices[1].color = uvec4b(255, 255, 255, 255);
    model->dirty = true;
    return model;
}

model3d_t *model3d_rect(void)
{
    int i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    const vec2_t POS_UV[4][2] = {
        {vec2(-0.5, -0.5), vec2(0, 1)},
        {vec2(+0.5, -0.5), vec2(1, 1)},
        {vec2(+0.5, +0.5), vec2(1, 0)},
        {vec2(-0.5, +0.5), vec2(0, 0)},
    };

    for (i = 0; i < 6; i++) {
        v = (int[]){0, 1, 2, 2, 3, 0}[i];
        model->vertices[i].pos.xy = POS_UV[v][0];
        model->vertices[i].uv = POS_UV[v][1];
        model->vertices[i].color = uvec4b(255, 255, 255, 255);
        model->vertices[i].normal = vec3(0, 1, 0);
    }
    model->solid = true;
    model->dirty = true;
    return model;
}

void model3d_render(model3d_t *model3d,
                    const mat4_t *model, const mat4_t *proj,
                    const uvec4b_t *color,
                    const texture_t *tex,
                    float fade, const vec3_t *fade_center)
{
    uvec4b_t c = color ? *color : HEXCOLOR(0xffffffff);
    vec4_t cf;
    GL(glUseProgram(prog.prog));
    GL(glUniformMatrix4fv(prog.u_model_l, 1, 0, model->v));
    GL(glUniformMatrix4fv(prog.u_proj_l, 1, 0, proj->v));
    GL(glEnableVertexAttribArray(prog.a_pos_l));
    GL(glEnable(GL_BLEND));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LESS));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glEnable(GL_CULL_FACE));
    cf = vec4(c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
    GL(glCullFace(GL_BACK));
    GL(glUniform4fv(prog.u_color_l, 1, cf.v));
    if (fade_center) {
        GL(glUniform1f(prog.u_fade_l, fade));
        GL(glUniform3fv(prog.u_fade_center_l, 1, fade_center->v));
    } else {
        GL(glUniform1f(prog.u_fade_l, 0));
    }

    tex = tex ?: g_white_tex;
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));

    if (model3d->dirty) {
        if (!model3d->vertex_buffer)
            GL(glGenBuffers(1, &model3d->vertex_buffer));
        GL(glBindBuffer(GL_ARRAY_BUFFER, model3d->vertex_buffer));
        GL(glBufferData(GL_ARRAY_BUFFER,
                        model3d->nb_vertices * sizeof(*model3d->vertices),
                        model3d->vertices, GL_STATIC_DRAW));
        model3d->dirty = false;
    }
    GL(glBindBuffer(GL_ARRAY_BUFFER, model3d->vertex_buffer));
    GL(glEnableVertexAttribArray(prog.a_pos_l));
    GL(glVertexAttribPointer(prog.a_pos_l, 3, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, pos)));
    GL(glEnableVertexAttribArray(prog.a_color_l));
    GL(glVertexAttribPointer(prog.a_color_l, 4, GL_UNSIGNED_BYTE, true,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, color)));
    GL(glEnableVertexAttribArray(prog.a_uv_l));
    GL(glVertexAttribPointer(prog.a_uv_l, 2, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, uv)));
    if (model3d->solid)
        GL(glDrawArrays(GL_TRIANGLES, 0, model3d->nb_vertices));
    else
        GL(glDrawArrays(GL_LINES, 0, model3d->nb_vertices));
    GL(glDisableVertexAttribArray(prog.a_pos_l));
    GL(glDisableVertexAttribArray(prog.a_color_l));
    GL(glCullFace(GL_BACK));
}

static const char *VSHADER =
    "                                                                   \n"
    "attribute vec3  a_pos;                                             \n"
    "attribute vec4  a_color;                                           \n"
    "attribute vec2  a_uv;                                              \n"
    "uniform   mat4  u_model;                                           \n"
    "uniform   mat4  u_proj;                                            \n"
    "uniform   vec4  u_color;                                           \n"
    "uniform   float u_fade;                                            \n"
    "uniform   vec3  u_fade_center;                                     \n"
    "                                                                   \n"
    "varying   vec4 v_color;                                            \n"
    "varying   vec2 v_uv;                                               \n"
    "                                                                   \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    vec3 pos = (u_model * vec4(a_pos, 1.0)).xyz;                   \n"
    "    gl_Position = u_proj * vec4(pos, 1.0);                         \n"
    "    v_color = u_color * a_color;                                   \n"
    "    if (u_fade > 0.0) {                                            \n"
    "       float fdist = distance(pos, u_fade_center);                 \n"
    "       float fade = smoothstep(u_fade, 0.0, fdist);                \n"
    "       v_color.a *= fade;                                          \n"
    "    }                                                              \n"
    "    v_uv = a_uv;                                                   \n"
    "}                                                                  \n"
;

static const char *FSHADER =
    "                                                                   \n"
    "#ifdef GL_ES                                                       \n"
    "precision highp float;                                             \n"
    "#endif                                                             \n"
    "                                                                   \n"
    "uniform sampler2D u_tex;                                           \n"
    "                                                                   \n"
    "varying lowp vec4  v_color;                                        \n"
    "varying      vec2  v_uv;                                           \n"
    "                                                                   \n"
    "void main()                                                        \n"
    "{                                                                  \n"
    "    gl_FragColor = v_color * texture2D(u_tex, v_uv);               \n"
    "}                                                                  \n"
;
