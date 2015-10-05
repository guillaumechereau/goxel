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
#include <stdarg.h>

static goxel_t *g_goxel = NULL;

static void unpack_pos_data(const uvec4b_t data, vec3b_t *pos, int *face,
                            int *cube_id)
{
    int x, y, z, f, i;
    x = data.r >> 4;
    y = data.r & 0x0f;
    z = data.g >> 4;
    f = data.g & 0x0f;
    assert(f < 6);
    i = ((int)data.b) << 8 | data.a;
    *pos = vec3b(x, y, z);
    *face = f;
    *cube_id = i;
}

// Similar to gluUnproject.
static vec3_t unproject(const vec3_t *win, const mat4_t *model,
                        const mat4_t *proj, const vec4_t *view)
{
    mat4_t inv = mat4_mul(*proj, *model);
    mat4_invert(&inv); // XXX: check for return value.
    vec3_t norm_pos = vec3(
            (2 * win->x - view->v[0]) / view->v[2] - 1,
            (2 * win->y - view->v[1]) / view->v[3] - 1,
            2 * win->z - 1);
    return mat4_mul_vec3(inv, norm_pos);
}

// XXX: can we merge this with unproject?
static vec3_t unproject_delta(const vec3_t *win, const mat4_t *model,
                              const mat4_t *proj, const vec4_t *view)
{
    mat4_t inv = mat4_mul(*proj, *model);
    mat4_invert(&inv); // XXX: check for return value.
    vec4_t norm_pos = vec4(
            win->x / view->v[2],
            win->y / view->v[3],
             0, 0);
    return mat4_mul_vec(inv, norm_pos).xyz;
}

// XXX: lot of cleanup to do here.
bool goxel_unproject_on_screen(goxel_t *goxel, const vec2_t *view_size,
                               const vec2_t *pos,
                               vec3_t *out, vec3_t *normal)
{
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);
    // XXX: pos should already be in windows coordinates.
    vec3_t wpos = vec3(pos->x, view_size->y - pos->y, 0);
    *out = unproject(&wpos, &goxel->camera.view_mat,
                            &goxel->camera.proj_mat, &view);
    *normal = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                         vec4(0, 0, -1, 0)).xyz;
    return true;
}

// XXX: lot of cleanup to do here.
bool goxel_unproject_on_plane(goxel_t *goxel, const vec2_t *view_size,
                              const vec2_t *pos, const plane_t *plane,
                              vec3_t *out, vec3_t *normal)
{
    // If the angle between the screen and the plane is close to 90 deg,
    // the projection fails.  This prevents projecting too far away.
    const float min_angle_cos = 0.1;
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);
    // XXX: pos should already be in windows coordinates.
    vec3_t wpos = vec3(pos->x, view_size->y - pos->y, 0);
    vec3_t opos;
    vec4_t onorm;

    opos = unproject(&wpos, &goxel->camera.view_mat,
                            &goxel->camera.proj_mat, &view);
    onorm = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                         vec4(0, 0, -1, 0));
    if (fabs(vec3_dot(onorm.xyz, plane->n)) <= min_angle_cos)
        return false;

    if (!plane_line_intersection(*plane, opos, onorm.xyz, out))
        return false;
    *out = mat4_mul_vec3(plane->mat, *out);
    *normal = plane->n;
    return true;
}

bool goxel_unproject_on_mesh(goxel_t *goxel, const vec2_t *view_size,
                             const vec2_t *pos, mesh_t *mesh,
                             vec3_t *out, vec3_t *normal)
{
    // XXX: No need to render the fbo if it is not dirty.
    extern const vec3b_t FACES_NORMALS[6];

    if (goxel->pick_fbo && !vec2_equal(
                vec2(goxel->pick_fbo->w, goxel->pick_fbo->h), *view_size)) {
        texture_dec_ref(goxel->pick_fbo);
        goxel->pick_fbo = NULL;
    }

    if (!goxel->pick_fbo) {
        goxel->pick_fbo = texture_create_buffer(
                view_size->x, view_size->y, TF_DEPTH);
        texture_inc_ref(goxel->pick_fbo);
    }

    renderer_t rend = {.material = goxel->rend.material};
    uvec4b_t pixel;
    vec3b_t voxel_pos;
    block_t *block;
    int face, block_id;
    int x, y;

    GL(glViewport(0, 0, view_size->x, view_size->y));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, goxel->pick_fbo->framebuffer));
    GL(glClearColor(0, 0, 0, 0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    render_mesh(&rend, mesh, EFFECT_RENDER_POS);
    render_render(&rend, &goxel->camera.view_mat, &goxel->camera.proj_mat);

    x = nearbyint(pos->x);
    y = view_size->y - nearbyint(pos->y) - 1;
    GL(glViewport(0, 0, goxel->screen_size.x, goxel->screen_size.y));
    if (x < 0 || x >= view_size->x ||
        y < 0 || y >= view_size->y) return false;
    GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel));

    unpack_pos_data(pixel, &voxel_pos, &face, &block_id);
    if (!block_id) return false;
    DL_FOREACH(mesh->blocks, block) {
        if (block->id == block_id) break;
    }
    assert(block);
    *out = vec3(block->pos.x + voxel_pos.x - BLOCK_SIZE / 2 + 0.5,
                block->pos.y + voxel_pos.y - BLOCK_SIZE / 2 + 0.5,
                block->pos.z + voxel_pos.z - BLOCK_SIZE / 2 + 0.5);
    *normal = vec3(VEC3_SPLIT(FACES_NORMALS[face]));
    return true;
}

int goxel_unproject(goxel_t *goxel, const vec2_t *view_size,
                    const vec2_t *pos, vec3_t *out, vec3_t *normal)
{
    int i, ret = 0;
    float d, best = FLT_MAX;
    vec3_t p, n;
    bool r = false;
    mat4_t proj = mat4_mul(goxel->camera.proj_mat, goxel->camera.view_mat);

    for (i = 0; i < 2; i++) {
        if (!(goxel->snap & (1 << i))) continue;
        if ((1 << i) == SNAP_MESH)
            r = goxel_unproject_on_mesh(goxel, view_size, pos,
                                        goxel->pick_mesh, &p, &n);
        if ((1 << i) == SNAP_PLANE)
            r = goxel_unproject_on_plane(goxel, view_size, pos,
                                         &goxel->plane, &p, &n);
        if (!r)
            continue;

        p.x = nearbyint(p.x - 0.5) + 0.5;
        p.y = nearbyint(p.y - 0.5) + 0.5;
        p.z = nearbyint(p.z - 0.5) + 0.5;

        d = mat4_mul_vec3(proj, p).z;
        if (d >= best) continue;
        ret = 1 << i;
        best = d;
        *out = p;
        *normal = n;
    }
    return ret;
}

void goxel_init(goxel_t *goxel)
{
    g_goxel = goxel;
    memset(goxel, 0, sizeof(*goxel));

    render_init();
    shapes_init();
    goxel->camera.ofs = vec3_zero;
    goxel->camera.rot = quat_identity;
    goxel->camera.dist = 128;
    quat_irotate(&goxel->camera.rot, M_PI / 4, 1, 0, 0);
    quat_irotate(&goxel->camera.rot, M_PI / 4, 0, 1, 0);

    goxel->image = image_new();

    goxel->layers_mesh = mesh_copy(goxel->image->active_layer->mesh);
    goxel->pick_mesh = mesh_copy(goxel->image->active_layer->mesh);

    goxel->back_color = HEXCOLOR(0x0000ffff);
    goxel->grid_color = HEXCOLOR(0xffffffff);

    goxel->palette = palette_get();
    goxel->tool = TOOL_BRUSH;
    goxel->tool_radius = 0.5;
    goxel->painter = (painter_t) {
        .shape = &shape_cube,
        .op = OP_ADD,
        .color = HEXCOLOR(0xEEEEECFF),
    };
    goxel->rend = (renderer_t) {
        .border_shadow = 0.25,
        .light = {
            .direction = vec3_normalized(vec3(0.5, 0.7, 1)),
            .fixed = true,
            .intensity = 1.
        },
        // XXX: find good values.
        .material = {
            .ambient = 0.3,
            .diffuse = 0.8,
            .specular = 0.2,
            .shininess = 2.0,
            .smoothness = 0.0,
            .effects = EFFECT_BORDERS_ALL,
        },
    };

    model3d_init();
    goxel->plane = plane(vec3(0.5, 0.5, 0.5), vec3(1, 0, 0), vec3(0, 0, -1));
    goxel->snap = SNAP_PLANE | SNAP_MESH;
    gui_init();
}

goxel_t *goxel(void)
{
    return g_goxel;
}

void goxel_iter(goxel_t *goxel, inputs_t *inputs)
{
    profiler_tick();
    goxel_set_help_text(goxel, NULL);
    goxel->screen_size = vec2i(inputs->window_size[0], inputs->window_size[1]);
    gui_iter(goxel, inputs);
    goxel->frame_count++;
}

static quat_t compute_view_rotation(const quat_t *rot,
        const vec2_t *start_pos, const vec2_t *end_pos,
        const vec2_t *view_size)
{
    quat_t q1, q2;
    float x1, y1, x2, y2, x_rot, y_rot;
    vec4_t x_axis;
    x1 = start_pos->x / view_size->x;
    y1 = start_pos->y / view_size->y;
    x2 =   end_pos->x / view_size->x;
    y2 =   end_pos->y / view_size->y;
    y_rot = (x2 - x1) * 2 * M_PI;
    x_rot = (y2 - y1) * 2 * M_PI;
    q1 = quat_from_axis(y_rot, 0, 1, 0);
    x_axis = quat_mul_vec4(quat_conjugate(quat_mul(*rot, q1)),
                           vec4(1, 0, 0, 0));
    q2 = quat_from_axis(x_rot, x_axis.x, x_axis.y, x_axis.z);
    return quat_mul(q1, q2);
}

// XXX: Cleanup this.
void goxel_mouse_in_view(goxel_t *goxel, const vec2_t *view_size,
                         const inputs_t *inputs, bool inside)
{
    vec4_t x_axis;

    if (inputs->mouse_wheel) {
        goxel->camera.zoom += inputs->mouse_wheel;
        // XXX: update the camera.dist ?
        return;
    }
    // Middle click: rotate the view.
    if (inputs->mouse_down[1]) {
        if (!goxel->moving) {
            goxel->moving = true;
            goxel->move_origin.rotation = goxel->camera.rot;
            goxel->move_origin.pos = inputs->mouse_pos;
        }
        goxel->camera.rot= quat_mul(goxel->move_origin.rotation,
                compute_view_rotation(&goxel->move_origin.rotation,
                    &goxel->move_origin.pos, &inputs->mouse_pos, view_size));
        return;
    }
    // Right click: pan the view
    if (inputs->mouse_down[2]) {
        if (!goxel->moving) {
            goxel->moving = true;
            goxel->move_origin.camera_ofs = goxel->camera.ofs;
            goxel->move_origin.pos = inputs->mouse_pos;
        }

        vec4_t view = vec4(0, 0, view_size->x, view_size->y);
        vec3_t wpos = vec3(inputs->mouse_pos.x,
                           view_size->y - inputs->mouse_pos.y, 0);
        vec3_t worigin_pos = vec3(goxel->move_origin.pos.x,
                                  view_size->y - goxel->move_origin.pos.y,
                                  0);
        vec3_t wdelta = vec3_sub(wpos, worigin_pos);
        vec3_t odelta = unproject_delta(&wdelta, &goxel->camera.view_mat,
                                        &goxel->camera.proj_mat, &view);
        goxel->camera.ofs = vec3_add(goxel->move_origin.camera_ofs,
                                     odelta);
        return;
    }
    // handle mouse rotations
    if (inputs->keys[KEY_LEFT])
        quat_irotate(&goxel->camera.rot, 0.05, 0, +1, 0);
    if (inputs->keys[KEY_RIGHT])
        quat_irotate(&goxel->camera.rot, 0.05, 0, -1, 0);
    if (inputs->keys[KEY_UP]) {
        x_axis = quat_mul_vec4(quat_conjugate(goxel->camera.rot),
                               vec4(1, 0, 0, 0));
        quat_irotate(&goxel->camera.rot, -0.05, x_axis.x, x_axis.y, x_axis.z);
    }
    if (inputs->keys[KEY_DOWN]) {
        x_axis = quat_mul_vec4(quat_conjugate(goxel->camera.rot),
                               vec4(1, 0, 0, 0));
        quat_irotate(&goxel->camera.rot, +0.05, x_axis.x, x_axis.y, x_axis.z);
    }
    // C: recenter the view:
    // XXX: need to do it only on key down!
    if (inputs->keys['C']) {
        vec3_t p, n;
        if (goxel_unproject_on_mesh(goxel, view_size, &inputs->mouse_pos,
                                    goxel->pick_mesh, &p, &n)) {
            goxel->camera.ofs = vec3_neg(p);
        }
    }


    if (goxel->moving && !inputs->mouse_down[1] && !inputs->mouse_down[2])
        goxel->moving = false;

    // Paint with the current tool if needed.
    goxel->tool_state = tool_iter(goxel, goxel->tool, inputs,
                                  goxel->tool_state, view_size, inside);
}

void goxel_render(goxel_t *goxel)
{
    GL(glViewport(0, 0, goxel->screen_size.x, goxel->screen_size.y));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL(glClearColor(0, 0, 0, 1));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    gui_render();
}

void goxel_update_meshes(goxel_t *goxel, bool pick)
{
    layer_t *layer;
    mesh_clear(goxel->layers_mesh);
    DL_FOREACH(goxel->image->layers, layer) {
        if (!layer->visible) continue;
        mesh_merge(goxel->layers_mesh, layer->mesh);
    }
    if (pick)
        mesh_set(&goxel->pick_mesh, goxel->layers_mesh);
}

// XXX: this function has to be rewritten.
void goxel_export_as_png(goxel_t *goxel, const char *path)
{
    const float size = 16;
    int w = goxel->image->export_width;
    int h = goxel->image->export_height;
    const float aspect = (float)w / h;
    float zoom;
    texture_t *fbo;
    renderer_t *rend = &goxel->rend;
    mesh_t *mesh;
    LOG_I("Exporting to file %s", path);

    mesh = goxel->layers_mesh;
    fbo = texture_create_buffer(w, h, TF_DEPTH);

    // XXX: this should be put somewhere else!
    goxel->camera.view = vec4(0, 0, w, h);
    goxel->camera.view_mat = mat4_identity;
    mat4_itranslate(&goxel->camera.view_mat, 0, 0, -goxel->camera.dist);
    mat4_imul_quat(&goxel->camera.view_mat, goxel->camera.rot);
    mat4_itranslate(&goxel->camera.view_mat,
           goxel->camera.ofs.x, goxel->camera.ofs.y, goxel->camera.ofs.z);
    goxel->camera.proj_mat = mat4_ortho(
            -size, +size, -size / aspect, +size / aspect, 0, 1000);
    zoom = pow(1.25f, goxel->camera.zoom);
    mat4_iscale(&goxel->camera.proj_mat, zoom, zoom, zoom);

    GL(glViewport(0, 0, w, h));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    render_mesh(rend, mesh, 0);
    render_render(rend, &goxel->camera.view_mat, &goxel->camera.proj_mat);
    texture_save_to_file(fbo, path, 0);
}

void goxel_export_as_obj(goxel_t *goxel, const char *path)
{
    wavefront_export(goxel->layers_mesh, path);
}

void goxel_export_as_ply(goxel_t *goxel, const char *path)
{
    ply_export(goxel->layers_mesh, path);
}

void goxel_set_help_text(goxel_t *goxel, const char *msg, ...)
{
    va_list args;
    free(goxel->help_text);
    goxel->help_text = NULL;
    if (!msg) return;
    va_start(args, msg);
    vasprintf(&goxel->help_text, msg, args);
    va_end(args);
}

void goxel_undo(goxel_t *goxel)
{
    tool_cancel(goxel, goxel->tool, goxel->tool_state);
    image_undo(goxel->image);
}

void goxel_redo(goxel_t *goxel)
{
    tool_cancel(goxel, goxel->tool, goxel->tool_state);
    image_redo(goxel->image);
}
