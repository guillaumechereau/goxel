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

typedef struct {
    GLint prog;
    GLint a_pos_l;
    GLint a_color_l;
    GLint a_normal_l;
    GLint a_uv_l;
    GLint u_color_l;
    GLint u_model_l;
    GLint u_view_l;
    GLint u_proj_l;
    GLint u_clip_l;
    GLint u_tex_l;
    GLint u_uv_scale_l;
    GLint u_strip_l;
    GLint u_time_l;
    GLint u_grid_alpha_l;

    GLint u_l_dir_l;
    GLint u_l_diff_l;
    GLint u_l_emit_l;
} prog_t;

static prog_t prog;
static texture_t *g_white_tex = NULL;

static void init_prog(prog_t *prog, const char *vshader, const char *fshader)
{
    prog->prog = gl_create_prog(vshader, fshader, NULL);
#define UNIFORM(x) GL(prog->x##_l = glGetUniformLocation(prog->prog, #x))
#define ATTRIB(x)  GL(prog->x##_l = glGetAttribLocation(prog->prog, #x))
    ATTRIB(a_pos);
    ATTRIB(a_color);
    ATTRIB(a_normal);
    ATTRIB(a_uv);
    UNIFORM(u_color);
    UNIFORM(u_model);
    UNIFORM(u_view);
    UNIFORM(u_proj);
    UNIFORM(u_clip);
    UNIFORM(u_tex);
    UNIFORM(u_uv_scale);
    UNIFORM(u_strip);
    UNIFORM(u_time);
    UNIFORM(u_grid_alpha);
    UNIFORM(u_l_dir);
    UNIFORM(u_l_diff);
    UNIFORM(u_l_emit);
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
    const char *shader;
    shader = assets_get("asset://data/shaders/model3d.glsl", NULL);
    init_prog(&prog, shader, shader);
    g_white_tex = create_white_tex();
}

model3d_t *model3d_cube(void)
{
    int f, i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->solid = true;
    model->cull = true;
    const int *p;
    model->nb_vertices = 6 * 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 6; i++) {
            v = (int[]){0, 1, 2, 2, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            vec3_set(model->vertices[f * 6 + i].pos,
                (p[0] - 0.5) * 2, (p[1] - 0.5) * 2, (p[2] - 0.5) * 2);
            vec3_set(model->vertices[f * 6 + i].normal, FACES_NORMALS[f][0],
                                                        FACES_NORMALS[f][1],
                                                        FACES_NORMALS[f][2]);
            vec4_set(model->vertices[f * 6 + i].color, 255, 255, 255, 255);
        }
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_wire_cube(void)
{
    int f, i, v;
    const int *p;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6 * 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 8; i++) {
            v = (int[]){0, 1, 1, 2, 2, 3, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            vec3_set(model->vertices[f * 8 + i].pos,
                (p[0] - 0.5) * 2, (p[1] - 0.5) * 2, (p[2] - 0.5) * 2);
            vec4_set(model->vertices[f * 8 + i].color, 255, 255, 255, 255);
            vec2_set(model->vertices[f * 8 + i].uv, 0.5, 0.5);
        }
    }
    model->cull = true;
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
            vec3_set(v[ind + 0].pos, r0 * cos(a0), r0 * sin(a0), z0);
            vec3_set(v[ind + 1].pos, r0 * cos(a1), r0 * sin(a1), z0);
            vec3_set(v[ind + 2].pos, r1 * cos(a0), r1 * sin(a0), z1);
            vec3_set(v[ind + 3].pos, r1 * cos(a1), r1 * sin(a1), z1);
            vec3_set(v[ind + 4].pos, r1 * cos(a0), r1 * sin(a0), z1);
            vec3_set(v[ind + 5].pos, r0 * cos(a1), r0 * sin(a1), z0);
            for (i = 0; i < 6; i++)
                vec3_copy(v[ind + i].pos, v[ind + i].normal);

        }
    }
    model->cull = true;
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
    const uint8_t c[4]  = {255, 255, 255, 160};
    const uint8_t cb[4] = {255, 255, 255, 255};

    model->nb_vertices = (nx + ny + 2) * 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    v = model->vertices;
    for (i = 0; i < nx + 1; i++) {
        side = i == 0 || i == nx;
        x = (float)i / nx - 0.5;
        vec3_set(v[i * 2 + 0].pos, x, -0.5, 0);
        vec3_set(v[i * 2 + 1].pos, x, +0.5, 0);
        vec4_copy(side ? cb : c, v[i * 2 + 0].color);
        vec4_copy(side ? cb : c, v[i * 2 + 1].color);
    }
    v = model->vertices + 2 * nx + 2;
    for (i = 0; i < ny + 1; i++) {
        side = i == 0 || i == ny;
        y = (float)i / ny - 0.5;
        vec3_set(v[i * 2 + 0].pos, -0.5, y, 0);
        vec3_set(v[i * 2 + 1].pos, +0.5, y, 0);
        vec4_copy(side ? cb : c, v[i * 2 + 0].color);
        vec4_copy(side ? cb : c, v[i * 2 + 1].color);
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_line(void)
{
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    vec3_set(model->vertices[0].pos, -0.5, 0, 0);
    vec3_set(model->vertices[1].pos, +0.5, 0, 0);
    vec4_set(model->vertices[0].color, 255, 255, 255, 255);
    vec4_set(model->vertices[1].color, 255, 255, 255, 255);
    model->dirty = true;
    return model;
}

model3d_t *model3d_rect(void)
{
    int i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    const float POS_UV[4][2][2] = {
        {{-0.5, -0.5}, {0, 1}},
        {{+0.5, -0.5}, {1, 1}},
        {{+0.5, +0.5}, {1, 0}},
        {{-0.5, +0.5}, {0, 0}},
    };

    for (i = 0; i < 6; i++) {
        v = (int[]){0, 1, 2, 2, 3, 0}[i];
        vec2_copy(POS_UV[v][0], model->vertices[i].pos);
        vec2_copy(POS_UV[v][1], model->vertices[i].uv);
        vec4_set(model->vertices[i].color, 255, 255, 255, 255);
        vec3_set(model->vertices[i].normal, 0, 1, 0);
    }
    model->solid = true;
    model->dirty = true;
    return model;
}

model3d_t *model3d_wire_rect(void)
{
    int i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    const float POS_UV[4][2][2] = {
        {{-0.5, -0.5}, {0, 1}},
        {{+0.5, -0.5}, {1, 1}},
        {{+0.5, +0.5}, {1, 0}},
        {{-0.5, +0.5}, {0, 0}},
    };

    for (i = 0; i < 8; i++) {
        v = ((i + 1) / 2) % 4;
        vec2_copy(POS_UV[v][0], model->vertices[i].pos);
        vec2_copy(POS_UV[v][1], model->vertices[i].uv);
        vec4_set(model->vertices[i].color, 255, 255, 255, 255);
    }
    model->dirty = true;
    return model;
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

void model3d_render(model3d_t *model3d,
                    const float model[4][4],
                    const float view[4][4],
                    const float proj[4][4],
                    const uint8_t color[4],
                    const texture_t *tex,
                    const float light[3],
                    const float clip_box[4][4],
                    int   effects)
{
    uint8_t c[4];
    float cf[4];
    float light_dir[3];
    float clip[4][4] = {};
    float grid_alpha;

    copy_color(color, c);
    GL(glUseProgram(prog.prog));
    GL(glUniformMatrix4fv(prog.u_model_l, 1, 0, (float*)model));
    GL(glUniformMatrix4fv(prog.u_view_l, 1, 0, (float*)view));
    GL(glUniformMatrix4fv(prog.u_proj_l, 1, 0, (float*)proj));
    GL(glEnableVertexAttribArray(prog.a_pos_l));
    GL(glEnable(GL_BLEND));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LESS));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glCullFace(GL_BACK));
    model3d->cull ? GL(glEnable(GL_CULL_FACE)) :
                    GL(glDisable(GL_CULL_FACE));
    GL(glCullFace(effects & EFFECT_SEE_BACK ? GL_FRONT : GL_BACK));
    if (!model3d->solid) GL(glDepthMask(false));

    vec4_set(cf, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, c[3] / 255.0);
    GL(glUniform4fv(prog.u_color_l, 1, cf));
    GL(glUniform1f(prog.u_strip_l, effects & EFFECT_STRIP ? 1.0 : 0.0));
    GL(glUniform1f(prog.u_time_l, 0)); // No moving strip effects.

    tex = tex ?: g_white_tex;
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    GL(glUniform2f(prog.u_uv_scale_l, (float)tex->w / tex->tex_w,
                                      (float)tex->h / tex->tex_h));

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
    GL(glEnableVertexAttribArray(prog.a_normal_l));
    GL(glVertexAttribPointer(prog.a_normal_l, 3, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, normal)));

    GL(glUniform1f(prog.u_l_emit_l, 1.0f));
    GL(glUniform1f(prog.u_l_diff_l, 0.0f));

    if (clip_box && !box_is_null(clip_box)) mat4_invert(clip_box, clip);
    GL(glUniformMatrix4fv(prog.u_clip_l, 1, 0, (float*)clip));

    grid_alpha = (effects & EFFECT_GRID) ? 0.05 : 0.0;
    GL(glUniform1f(prog.u_grid_alpha_l, grid_alpha));

    if (model3d->solid) {
        if (light && (!(effects & EFFECT_NO_SHADING))) {
            vec3_copy(light, light_dir);
            if (effects & EFFECT_SEE_BACK) vec3_imul(light_dir, -1);
            GL(glUniform3fv(prog.u_l_dir_l, 1, light_dir));
            GL(glUniform1f(prog.u_l_emit_l, 0.2f));
            GL(glUniform1f(prog.u_l_diff_l, 0.8f));
        }
        GL(glDrawArrays(GL_TRIANGLES, 0, model3d->nb_vertices));
    } else {
        GL(glDrawArrays(GL_LINES, 0, model3d->nb_vertices));
    }
    GL(glDisableVertexAttribArray(prog.a_pos_l));
    GL(glDisableVertexAttribArray(prog.a_color_l));
    GL(glCullFace(GL_BACK));
    GL(glDepthMask(true));
}
