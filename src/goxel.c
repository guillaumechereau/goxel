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

goxel_t *goxel = NULL;

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
    vec3_t onorm;

    camera_get_ray(&goxel->camera, &wpos.xy, &view, &opos, &onorm);
    if (fabs(vec3_dot(onorm, plane->n)) <= min_angle_cos)
        return false;

    if (!plane_line_intersection(*plane, opos, onorm, out))
        return false;
    *out = mat4_mul_vec3(plane->mat, *out);
    *normal = plane->n;
    return true;
}

bool goxel_unproject_on_box(goxel_t *goxel, const vec2_t *view_size,
                     const vec2_t *pos, const box_t *box, bool inside,
                     vec3_t *out, vec3_t *normal,
                     int *face)
{
    extern const mat4_t FACES_MATS[6];
    int f;
    vec3_t wpos = vec3(pos->x, view_size->y - pos->y, 0);
    vec3_t opos;
    vec3_t onorm;
    plane_t plane;
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);

    if (box_is_null(*box)) return false;
    camera_get_ray(&goxel->camera, &wpos.xy, &view, &opos, &onorm);
    for (f = 0; f < 6; f++) {
        plane.mat = box->mat;
        mat4_imul(&plane.mat, FACES_MATS[f]);

        if (!inside && vec3_dot(plane.n, onorm) >= 0)
            continue;
        if (inside && vec3_dot(plane.n, onorm) <= 0)
            continue;
        if (!plane_line_intersection(plane, opos, onorm, out))
            continue;
        if (!(out->x >= -1 && out->x < 1 && out->y >= -1 && out->y < 1))
            continue;
        if (face) *face = f;
        *out = mat4_mul_vec3(plane.mat, *out);
        *normal = vec3_normalized(plane.n);
        if (inside) vec3_imul(normal, -1);
        vec3_iaddk(out, *normal, 0.5);
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
    int rect[4] = {0, 0, view_size->x, view_size->y};

    rend.settings.shadow = 0;
    rend.fbo = goxel->pick_fbo->framebuffer;
    render_mesh(&rend, mesh, EFFECT_RENDER_POS);
    render_render(&rend, rect, &vec4_zero);

    x = round(pos->x);
    y = view_size->y - round(pos->y) - 1;
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
                    const vec2_t *pos, bool on_surface,
                    vec3_t *out, vec3_t *normal)
{
    int i, ret = 0;
    vec3_t p = vec3_zero, n = vec3_zero;
    float dist, best = INFINITY;
    bool r = false;

    // If tool_plane is set, we specifically use it.
    if (!plane_is_null(goxel->tool_plane)) {
        r = goxel_unproject_on_plane(goxel, view_size, pos,
                                     &goxel->tool_plane, out, normal);
        return r ? SNAP_PLANE : 0;
    }

    for (i = 0; i < 4; i++) {
        if (!(goxel->snap & (1 << i))) continue;
        if ((1 << i) == SNAP_MESH) {
            r = goxel_unproject_on_mesh(goxel, view_size, pos,
                                        goxel->pick_mesh, &p, &n);
            if (on_surface) vec3_iaddk(&p, n, 1);
        }
        if ((1 << i) == SNAP_PLANE)
            r = goxel_unproject_on_plane(goxel, view_size, pos,
                                         &goxel->plane, &p, &n);
        if ((1 << i) == SNAP_SELECTION_IN)
            r = goxel_unproject_on_box(goxel, view_size, pos,
                                       &goxel->selection, true,
                                       &p, &n, NULL);
        if ((1 << i) == SNAP_SELECTION_OUT)
            r = goxel_unproject_on_box(goxel, view_size, pos,
                                       &goxel->selection, false,
                                       &p, &n, NULL);
        if (!r)
            continue;

        dist = -mat4_mul_vec3(goxel->camera.view_mat, p).z;
        if (dist < 0 || dist > best) continue;

        best = dist;
        p.x = round(p.x - 0.5) + 0.5;
        p.y = round(p.y - 0.5) + 0.5;
        p.z = round(p.z - 0.5) + 0.5;

        *out = p;
        *normal = n;
        ret = 1 << i;
    }
    return ret;
}

void goxel_init(goxel_t *gox)
{
    goxel = gox;
    memset(goxel, 0, sizeof(*goxel));

    render_init();
    shapes_init();
    goxel->camera.ofs = vec3_zero;
    goxel->camera.rot = quat_identity;
    goxel->camera.dist = 128;
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 1, 0, 0);
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 0, 0, 1);

    goxel->image = image_new();

    goxel->layers_mesh = mesh_new();
    goxel->pick_mesh = mesh_new();
    goxel->tool_mesh_orig = mesh_new();
    goxel->tool_mesh = mesh_new();
    goxel_update_meshes(goxel, -1);
    goxel->selection = box_null;

    goxel->back_color = HEXCOLOR(0x393939ff);
    goxel->grid_color = HEXCOLOR(0x4a4a4aff);

    palette_load_all(&goxel->palettes);
    goxel->palette = goxel->palettes;
    goxel->tool = TOOL_BRUSH;
    goxel->tool_radius = 0.5;
    goxel->painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_ADD,
        .smoothness = 0,
        .color = HEXCOLOR(0xEEEEECFF),
    };
    goxel->rend = (renderer_t) {
        .light = {
            .pitch = 10 * DD2R,
            .yaw = 120 * DD2R,
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

void goxel_release(goxel_t *goxel)
{
    proc_release(&goxel->proc);
    gui_release();
}

void goxel_iter(goxel_t *goxel, inputs_t *inputs)
{
    goxel->frame_clock = get_clock();
    goxel_set_help_text(goxel, NULL);
    goxel_set_hint_text(goxel, NULL);
    goxel->screen_size = vec2i(inputs->window_size[0], inputs->window_size[1]);
    camera_update(&goxel->camera);
    goxel->rend.view_mat = goxel->camera.view_mat;
    goxel->rend.proj_mat = goxel->camera.proj_mat;
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
        goxel->camera.dist -= inputs->mouse_wheel * 10;
        return;
    }
    // Middle click: rotate the view.
    if (inputs->mouse_down[1]) {
        if (!goxel->moving) {
            goxel->moving = true;
            goxel->move_origin.rotation = goxel->camera.rot;
            goxel->move_origin.pos = inputs->mouse_pos;
        }
        goxel->camera.move_to_target = true;
        goxel->camera.rot = quat_mul(goxel->move_origin.rotation,
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
        vec3_imul(&odelta, 2); // XXX: why do I need that?
        if (!goxel->camera.ortho)
            vec3_imul(&odelta, goxel->camera.dist);
        goxel->camera.ofs = vec3_add(goxel->move_origin.camera_ofs,
                                     odelta);
        goxel->camera.target = vec3_neg(goxel->camera.ofs);
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
            goxel->camera.target = p;
            goxel->camera.move_to_target = true;
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
    GL(glStencilMask(0xFF));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));
    gui_render();
}

static void render_export_viewport(goxel_t *goxel, const vec4_t *view)
{
    // Render the export viewport.
    int w = goxel->image->export_width;
    int h = goxel->image->export_height;
    float aspect = (float)w/h;
    plane_t plane;
    float sx, sy;
    plane.mat = mat4_identity;
    mat4_itranslate(&plane.mat, 0, 0, -1);
    sy = 2 * tan(goxel->camera.fovy / 2. / 180. * M_PI);
    sx = sy * w / h;
    if (goxel->camera.aspect < aspect) {
        sx *= goxel->camera.aspect / aspect;
        sy *= goxel->camera.aspect / aspect;
    }
    mat4_iscale(&plane.mat, sx, sy, 1);
    render_rect(&goxel->rend, &plane, 1);
}

void goxel_render_view(goxel_t *goxel, const vec4_t *rect)
{
    layer_t *layer;
    renderer_t *rend = &goxel->rend;

    goxel->camera.aspect = rect->z / rect->w;
    camera_update(&goxel->camera);
    render_mesh(rend, goxel->layers_mesh, 0);

    // Render all the image layers.
    DL_FOREACH(goxel->image->layers, layer) {
        if (layer->visible && layer->image)
            render_img(rend, layer->image, &layer->mat);
    }

    render_box(rend, &goxel->selection, false, NULL, 2);

    // XXX: make a toggle for debug informations.
    if (0) {
        box_t b;
        uvec4b_t c;
        c = HEXCOLOR(0x00FF0050);
        b = mesh_get_box(goxel->layers_mesh, true);
        render_box(rend, &b, false, &c, false);
        c = HEXCOLOR(0x00FFFF50);
        b = mesh_get_box(goxel->layers_mesh, false);
        render_box(rend, &b, false, &c, false);
    }
    if (!goxel->plane_hidden && plane_is_null(goxel->tool_plane))
        render_plane(rend, &goxel->plane, &goxel->grid_color);
    if (goxel->show_export_viewport)
        render_export_viewport(goxel, rect);
}

void goxel_update_meshes(goxel_t *goxel, int mask)
{
    layer_t *layer;
    if (mask & MESH_LAYERS) {
        mesh_clear(goxel->layers_mesh);
        DL_FOREACH(goxel->image->layers, layer) {
            if (!layer->visible) continue;
            mesh_merge(goxel->layers_mesh, layer->mesh, MODE_ADD);
        }
    }
    if (mask & MESH_PICK)
        mesh_set(goxel->pick_mesh, goxel->layers_mesh);
}

static void export_as(const char *type, const char *path)
{
    char id[128];
    assert(path);
    // If not provided, try to guess the type from the path extension.
    if (!type) {
        type = strrchr(path, '.');
        if (!type) {
            LOG_E("Cannot guess file extension");
            return;
        }
        type++;
    }
    sprintf(id, "export_as_%s", type);
    action_exec2(id, "p", path);
}

ACTION_REGISTER(export_as,
    .help = "Export the image",
    .cfunc = export_as,
    .csig = "vpp",
)


// XXX: this function has to be rewritten.
static void export_as_png(const char *path, int w, int h)
{
    w = w ?: goxel->image->export_width;
    h = h ?: goxel->image->export_height;
    int rect[4] = {0, 0, w * 2, h * 2};
    uint8_t *data2, *data;
    texture_t *fbo;
    renderer_t rend = goxel->rend;
    mesh_t *mesh;
    camera_t camera = goxel->camera;

    camera.aspect = (float)w / h;
    LOG_I("Exporting to file %s", path);

    mesh = goxel->layers_mesh;
    fbo = texture_new_buffer(w * 2, h * 2, TF_DEPTH);

    camera_update(&camera);
    rend.view_mat = camera.view_mat;
    rend.proj_mat = camera.proj_mat;
    rend.fbo = fbo->framebuffer;

    render_mesh(&rend, mesh, 0);
    render_render(&rend, rect, &vec4_zero);
    data2 = calloc(w * h * 4 , 4);
    data = calloc(w * h, 4);
    texture_get_data(fbo, w * 2, h * 2, 4, data2);
    img_downsample(data2, w * 2, h * 2, 4, data);
    img_write(data, w, h, 4, path);
    free(data);
}

ACTION_REGISTER(export_as_png,
    .help = "Save the image as a png file",
    .cfunc = export_as_png,
    .csig = "vpii",
)

static void export_as_txt(const char *path)
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
    .cfunc = export_as_txt,
    .csig = "vp",
)

// XXX: we could merge all the set_xxx_text function into a single one.
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

void goxel_set_hint_text(goxel_t *goxel, const char *msg, ...)
{
    va_list args;
    free(goxel->hint_text);
    goxel->hint_text = NULL;
    if (!msg) return;
    va_start(args, msg);
    vasprintf(&goxel->hint_text, msg, args);
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
    texture_t *tex;
    tex = texture_new_image(path);
    if (!tex) return;
    image_history_push(goxel->image);
    layer = image_add_layer(goxel->image);
    sprintf(layer->name, "img");
    layer->image = tex;
    mat4_iscale(&layer->mat, layer->image->w, layer->image->h, 1);
}

static layer_t *cut_as_new_layer(image_t *img, layer_t *layer, box_t *box)
{
    layer_t *new_layer;
    painter_t painter;

    img = img ?: goxel->image;
    layer = layer ?: img->active_layer;
    box = box ?: &goxel->selection;

    new_layer = image_duplicate_layer(img, layer);
    painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_INTERSECT,
        .color = uvec4b(255, 255, 255, 255),
    };
    mesh_op(new_layer->mesh, &painter, box);
    painter.mode = MODE_SUB;
    mesh_op(layer->mesh, &painter, box);
    return new_layer;
}

ACTION_REGISTER(cut_as_new_layer,
    .help = "Cut into a new layer",
    .cfunc = cut_as_new_layer,
    .csig = "vppp",
    .flags = ACTION_TOUCH_IMAGE,
)

static void clear_selection(void)
{
    if (goxel->tool == TOOL_SELECTION)
        tool_cancel(goxel, goxel->tool, goxel->tool_state);
    goxel->selection = box_null;
}

ACTION_REGISTER(clear_selection,
    .help = "Clear the selection",
    .cfunc = clear_selection,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
)

static void fill_selection(layer_t *layer)
{
    if (box_is_null(goxel->selection)) return;
    mesh_op(layer->mesh, &goxel->painter, &goxel->selection);
    goxel_update_meshes(goxel, -1);
}

ACTION_REGISTER(fill_selection,
    .help = "Fill the selection with the current paint settings",
    .cfunc = fill_selection,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
)

static int show_grid_action(const action_t *a, astack_t *s)
{
    if (stack_type(s, 0) == 'b')
        goxel->plane_hidden = !stack_get_b(s, 0);
    stack_push_b(s, !goxel->plane_hidden);
    return 0;
}

ACTION_REGISTER(grid_visible,
    .help = "Show the grid",
    .func = show_grid_action,
    .shortcut = "#",
    .flags = ACTION_TOGGLE,
)

#define HS2 0.7071067811865476


static int view_set(const action_t *a, astack_t *s)
{
    goxel->camera.rot = *((quat_t*)a->data);
    goxel_update_meshes(goxel, -1);
    return 0;
}

ACTION_REGISTER(view_left,
    .help = "Set camera view to left",
    .func = view_set,
    .data = &QUAT(-0.5, 0.5, 0.5, 0.5),
    .shortcut = "Ctrl 3",
)

ACTION_REGISTER(view_right,
    .help = "Set camera view to right",
    .func = view_set,
    .data = &QUAT(0.5, 0.5, 0.5, -0.5),
    .shortcut = "3",
)

ACTION_REGISTER(view_top,
    .help = "Set camera view to top",
    .func = view_set,
    .data = &QUAT(0, 0, 0, 1),
    .shortcut = "7",
)

ACTION_REGISTER(view_front,
    .help = "Set camera view to front",
    .func = view_set,
    .data = &QUAT(-HS2, 0, 0, HS2),
    .shortcut = "1",
)
