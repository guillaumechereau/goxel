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

bool goxel_unproject_on_box(goxel_t *goxel, const vec2_t *view_size,
                     const vec2_t *pos, const box_t *box,
                     vec3_t *out, vec3_t *normal,
                     int *face)
{
    extern const mat4_t FACES_MATS[6];
    int f;
    vec3_t wpos = vec3(pos->x, view_size->y - pos->y, 0);
    vec3_t opos;
    vec4_t onorm;
    plane_t plane;
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);

    opos = unproject(&wpos, &goxel->camera.view_mat,
                            &goxel->camera.proj_mat, &view);
    onorm = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                         vec4(0, 0, -1, 0));

    for (f = 0; f < 6; f++) {
        plane.mat = box->mat;
        mat4_imul(&plane.mat, FACES_MATS[f]);

        if (vec3_dot(plane.n, onorm.xyz) >= 0)
            continue;
        if (!plane_line_intersection(plane, opos, onorm.xyz, out))
            continue;
        if (!(out->x >= -1 && out->x < 1 && out->y >= -1 && out->y < 1))
            continue;
        *face = f;
        *out = mat4_mul_vec3(plane.mat, *out);
        *normal = vec3_normalized(plane.n);
        return true;
    }
    return false;
}

bool goxel_unproject_on_mesh(goxel_t *goxel, const vec2_t *view_size,
                             const vec2_t *pos, mesh_t *mesh,
                             vec3_t *out, vec3_t *normal)
{
    // XXX: No need to render the fbo if it is not dirty.
    extern const vec3b_t FACES_NORMALS[6];

    if (goxel->pick_fbo && !vec2_equal(
                vec2(goxel->pick_fbo->w, goxel->pick_fbo->h), *view_size)) {
        texture_delete(goxel->pick_fbo);
        goxel->pick_fbo = NULL;
    }

    if (!goxel->pick_fbo) {
        goxel->pick_fbo = texture_new_buffer(
                view_size->x, view_size->y, TF_DEPTH);
    }

    renderer_t rend = {
        .view_mat = goxel->rend.view_mat,
        .proj_mat = goxel->rend.proj_mat,
        .settings = goxel->rend.settings,
    };
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
    render_render(&rend);

    x = nearbyint(pos->x);
    y = view_size->y - nearbyint(pos->y) - 1;
    GL(glViewport(0, 0, goxel->screen_size.x, goxel->screen_size.y));
    if (x < 0 || x >= view_size->x ||
        y < 0 || y >= view_size->y) return false;
    GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel));

    unpack_pos_data(pixel, &voxel_pos, &face, &block_id);
    if (!block_id) return false;
    MESH_ITER_BLOCKS(mesh, block) {
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
    int i;
    vec3_t p, n;
    bool r;

    // If tool_plane is set, we specifically use it.
    if (!plane_is_null(goxel->tool_plane)) {
        r = goxel_unproject_on_plane(goxel, view_size, pos,
                                     &goxel->tool_plane, out, normal);
        return r ? SNAP_PLANE : 0;
    }

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

        *out = p;
        *normal = n;
        return 1 << i;
    }
    return 0;
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
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 1, 0, 0);
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 0, 0, 1);

    goxel->image = image_new();

    goxel->layers_mesh = mesh_copy(goxel->image->active_layer->mesh);
    goxel->pick_mesh = mesh_copy(goxel->image->active_layer->mesh);
    goxel->selection = box_null;

    goxel->back_color = HEXCOLOR(0x393939ff);
    goxel->grid_color = HEXCOLOR(0x4a4a4aff);

    goxel->palette = palette_get();
    goxel->tool = TOOL_BRUSH;
    goxel->tool_radius = 0.5;
    goxel->painter = (painter_t) {
        .shape = &shape_cube,
        .op = OP_ADD,
        .smoothness = 0,
        .color = HEXCOLOR(0xEEEEECFF),
    };
    goxel->rend = (renderer_t) {
        .light = {
            .direction = vec3_normalized(vec3(0.5, 0.7, 1)),
            .fixed = true,
            .intensity = 1.
        },
    };
    render_get_default_settings(0, NULL, &goxel->rend.settings);

    model3d_init();
    goxel->plane = plane(vec3(0.5, 0.5, 0.5), vec3(1, 0, 0), vec3(0, 1, 0));
    goxel->snap = SNAP_PLANE | SNAP_MESH;
    gui_init();
}

goxel_t *goxel(void)
{
    return g_goxel;
}

void goxel_iter(goxel_t *goxel, inputs_t *inputs)
{
    float zoom;
    profiler_tick();
    goxel_set_help_text(goxel, NULL);
    goxel->screen_size = vec2i(inputs->window_size[0], inputs->window_size[1]);
    goxel->rend.view_mat = goxel->camera.view_mat;
    goxel->rend.proj_mat = goxel->camera.proj_mat;
    if (goxel->camera.move_to_last_pos) {
        zoom = pow(1.25f, goxel->camera.zoom);
        goxel->camera.move_to_last_pos = !vec3_ilerp_const(
                &goxel->camera.ofs, vec3_neg(goxel->last_pos), 1.0 / zoom);
    }
    gui_iter(goxel, inputs);
    goxel->frame_count++;
}

static quat_t compute_view_rotation(const quat_t *rot,
        const vec2_t *start_pos, const vec2_t *end_pos,
        const vec2_t *view_size)
{
    quat_t q1, q2;
    float x1, y1, x2, y2, x_rot, z_rot;
    vec4_t x_axis;
    x1 = start_pos->x / view_size->x;
    y1 = start_pos->y / view_size->y;
    x2 =   end_pos->x / view_size->x;
    y2 =   end_pos->y / view_size->y;
    z_rot = (x2 - x1) * 2 * M_PI;
    x_rot = (y2 - y1) * 2 * M_PI;
    q1 = quat_from_axis(z_rot, 0, 0, 1);
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
        goxel->camera.move_to_last_pos = true;
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
        goxel->last_pos = vec3_neg(goxel->camera.ofs);
        return;
    }
    // handle mouse rotations
    if (inputs->keys[KEY_LEFT])
        quat_irotate(&goxel->camera.rot, 0.05, 0, 0, +1);
    if (inputs->keys[KEY_RIGHT])
        quat_irotate(&goxel->camera.rot, 0.05, 0, 0, -1);
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
            goxel->last_pos = p;
            goxel->camera.move_to_last_pos = true;
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

static void export_as(goxel_t *goxel, const char *type, const char *path)
{
    char id[128];
    sprintf(id, "export_as_%s", type);
    action_exec2(id, ARG("path", path));
}

ACTION_REGISTER(export_as,
    .help = "Export the image",
    .func = export_as,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("type", TYPE_STRING),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)


// XXX: this function has to be rewritten.
static void export_as_png(goxel_t *goxel, const char *path)
{
    const float size = 16;
    int w = goxel->image->export_width;
    int h = goxel->image->export_height;
    const float aspect = (float)w / h;
    float zoom;
    texture_t *fbo;
    renderer_t rend = goxel->rend;
    mesh_t *mesh;
    LOG_I("Exporting to file %s", path);

    mesh = goxel->layers_mesh;
    fbo = texture_new_buffer(w, h, TF_DEPTH);

    // XXX: this should be put somewhere else!
    rend.view_mat = mat4_identity;
    mat4_itranslate(&rend.view_mat, 0, 0, -goxel->camera.dist);
    mat4_imul_quat(&rend.view_mat, goxel->camera.rot);
    mat4_itranslate(&rend.view_mat,
           goxel->camera.ofs.x, goxel->camera.ofs.y, goxel->camera.ofs.z);
    rend.proj_mat = mat4_ortho(
            -size, +size, -size / aspect, +size / aspect, 0, 1000);
    zoom = pow(1.25f, goxel->camera.zoom);
    mat4_iscale(&rend.proj_mat, zoom, zoom, zoom);

    GL(glViewport(0, 0, w, h));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer));
    GL(glClearColor(0, 0, 0, 0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    render_mesh(&rend, mesh, 0);
    render_render(&rend);
    texture_save_to_file(fbo, path);
}

ACTION_REGISTER(export_as_png,
    .help = "Save the image as a png file",
    .func = export_as_png,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)

static void export_as_obj(goxel_t *goxel, const char *path)
{
    wavefront_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_obj,
    .help = "Save the image as a wavefront obj file",
    .func = export_as_obj,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)

static void export_as_ply(goxel_t *goxel, const char *path)
{
    ply_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_ply,
    .help = "Save the image as a ply file",
    .func = export_as_ply,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)

static void export_as_qubicle(goxel_t *goxel, const char *path)
{
    qubicle_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_qubicle,
    .help = "Save the image as a qubicle 3d file",
    .func = export_as_qubicle,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)

static void export_as_txt(goxel_t *goxel, const char *path)
{
    FILE *out;
    mesh_t *mesh = goxel->layers_mesh;
    block_t *block;
    int x, y, z;
    uvec4b_t v;
    const int N = BLOCK_SIZE;

    out = fopen(path, "w");
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "# One line per voxel\n");
    fprintf(out, "# X Y Z RRGGBB\n");

    MESH_ITER_BLOCKS(mesh, block) {
        for (z = 1; z < N - 1; z++)
        for (y = 1; y < N - 1; y++)
        for (x = 1; x < N - 1; x++) {
            v = block->data->voxels[x + y * N + z * N * N];
            if (v.a < 127) continue;
            fprintf(out, "%d %d %d %2x%2x%2x\n",
                    x + (int)block->pos.x,
                    y + (int)block->pos.y,
                    z + (int)block->pos.z,
                    v.r, v.g, v.b);
        }
    }
    fclose(out);
}

ACTION_REGISTER(export_as_txt,
    .help = "Save the image as a txt file",
    .func = export_as_txt,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)

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

void goxel_import_image_plane(goxel_t *goxel, const char *path)
{
    layer_t *layer;
    layer = image_add_layer(goxel->image);
    sprintf(layer->name, "img");
    layer->image = texture_new_image(path);
    mat4_iscale(&layer->mat, layer->image->w, layer->image->h, 1);
    image_history_push(goxel->image);
}

static layer_t *cut_as_new_layer(goxel_t *goxel, image_t *img,
                                 layer_t *layer, box_t *box)
{
    layer_t *new_layer;
    painter_t painter;

    new_layer = image_duplicate_layer(img, layer);
    painter = (painter_t) {
        .shape = &shape_cube,
        .op = OP_INTERSECT,
    };
    mesh_op(new_layer->mesh, &painter, box);
    painter.op = OP_SUB;
    mesh_op(layer->mesh, &painter, box);
    return new_layer;
}

ACTION_REGISTER(cut_as_new_layer,
    .help = "Cut into a new layer",
    .func = cut_as_new_layer,
    .sig = SIG(TYPE_LAYER, ARG("goxel", TYPE_GOXEL),
                           ARG("img", TYPE_IMAGE),
                           ARG("layer", TYPE_LAYER),
                           ARG("box", TYPE_BOX)),
)

static void clear_selection(goxel_t *goxel)
{
    if (goxel->tool == TOOL_SELECTION)
        tool_cancel(goxel, goxel->tool, goxel->tool_state);
    goxel->selection = box_null;
}

ACTION_REGISTER(clear_selection,
    .help = "Clear the selection",
    .func = clear_selection,
    .sig = SIG(TYPE_LAYER, ARG("goxel", TYPE_GOXEL)),
)
