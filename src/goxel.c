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

static void unpack_pos_data(uint32_t v, int pos[3], int *face,
                            int *cube_id)
{
    assert(BLOCK_SIZE == 16);
    int x, y, z, f, i;
    x = v >> 28;
    y = (v >> 24) & 0x0f;
    z = (v >> 20) & 0x0f;
    f = (v >> 16) & 0x0f;
    i = v & 0xffff;
    assert(f < 6);
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    *face = f;
    *cube_id = i;
}

// XXX: can we merge this with unproject?
static bool unproject_delta(const float win[3], const float model[4][4],
                            const float proj[4][4], const float viewport[4],
                            float out[3])
{
    float inv[4][4], norm_pos[4];

    mat4_mul(proj, model, inv);
    if (mat4_invert(inv, inv)) return false;
    vec4_set(norm_pos, win[0] / viewport[2], win[1] / viewport[3], 0, 0);
    mat4_mul_vec4(inv, norm_pos, norm_pos);
    vec3_copy(norm_pos, out);
    return true;
}

// XXX: lot of cleanup to do here.
bool goxel_unproject_on_plane(goxel_t *goxel, const float viewport[4],
                              const float pos[2], const float plane[4][4],
                              float out[3], float normal[3])
{
    // If the angle between the screen and the plane is close to 90 deg,
    // the projection fails.  This prevents projecting too far away.
    const float min_angle_cos = 0.1;
    // XXX: pos should already be in windows coordinates.
    float wpos[3] = {pos[0], pos[1], 0};
    float opos[3], onorm[3];

    camera_get_ray(&goxel->camera, wpos, viewport, opos, onorm);
    if (fabs(vec3_dot(onorm, plane[2])) <= min_angle_cos)
        return false;

    if (!plane_line_intersection(plane, opos, onorm, out))
        return false;
    mat4_mul_vec3(plane, out, out);
    vec3_copy(plane[2], normal);
    return true;
}

bool goxel_unproject_on_box(goxel_t *goxel, const float viewport[4],
                     const float pos[2], const box_t *box, bool inside,
                     float out[3], float normal[3], int *face)
{
    int f;
    float wpos[3] = {pos[0], pos[1], 0};
    float opos[3], onorm[3];
    float plane[4][4];

    if (box_is_null(box->mat)) return false;
    camera_get_ray(&goxel->camera, wpos, viewport, opos, onorm);
    for (f = 0; f < 6; f++) {
        mat4_copy(box->mat, plane);
        mat4_imul(plane, FACES_MATS[f]);

        if (!inside && vec3_dot(plane[2], onorm) >= 0)
            continue;
        if (inside && vec3_dot(plane[2], onorm) <= 0)
            continue;
        if (!plane_line_intersection(plane, opos, onorm, out))
            continue;
        if (!(out[0] >= -1 && out[0] < 1 && out[1] >= -1 && out[1] < 1))
            continue;
        if (face) *face = f;
        mat4_mul_vec3(plane, out, out);
        vec3_normalize(plane[2], normal);
        if (inside) vec3_imul(normal, -1);
        return true;
    }
    return false;
}

bool goxel_unproject_on_mesh(goxel_t *goxel, const float view[4],
                             const float pos[2], mesh_t *mesh,
                             float out[3], float normal[3])
{
    float view_size[2] = {view[2], view[3]};
    // XXX: No need to render the fbo if it is not dirty.
    if (goxel->pick_fbo && !vec2_equal(
                VEC(goxel->pick_fbo->w, goxel->pick_fbo->h), view_size)) {
        texture_delete(goxel->pick_fbo);
        goxel->pick_fbo = NULL;
    }

    if (!goxel->pick_fbo) {
        goxel->pick_fbo = texture_new_buffer(
                view_size[0], view_size[1], TF_DEPTH);
    }

    renderer_t rend = {.settings = goxel->rend.settings};
    mat4_copy(goxel->rend.view_mat, rend.view_mat);
    mat4_copy(goxel->rend.proj_mat, rend.proj_mat);

    uint32_t pixel;
    int voxel_pos[3];
    int face, block_id, block_pos[3];
    int x, y;
    int rect[4] = {0, 0, view_size[0], view_size[1]};
    uint8_t clear_color[4] = {0, 0, 0, 0};

    rend.settings.shadow = 0;
    rend.fbo = goxel->pick_fbo->framebuffer;
    rend.scale = 1;
    render_mesh(&rend, mesh, EFFECT_RENDER_POS);
    render_submit(&rend, rect, clear_color);

    x = round(pos[0] - view[0]);
    y = round(pos[1] - view[1]);
    GL(glViewport(0, 0, goxel->pick_fbo->w, goxel->pick_fbo->h));
    if (x < 0 || x >= view_size[0] ||
        y < 0 || y >= view_size[1]) return false;
    GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel));

    unpack_pos_data(pixel, voxel_pos, &face, &block_id);
    if (!block_id) return false;
    render_get_block_pos(&rend, mesh, block_id, block_pos);
    out[0] = block_pos[0] + voxel_pos[0] + 0.5;
    out[1] = block_pos[1] + voxel_pos[1] + 0.5;
    out[2] = block_pos[2] + voxel_pos[2] + 0.5;
    normal[0] = FACES_NORMALS[face][0];
    normal[1] = FACES_NORMALS[face][1];
    normal[2] = FACES_NORMALS[face][2];
    vec3_iaddk(out, normal, 0.5);
    return true;
}


int goxel_unproject(goxel_t *goxel, const float viewport[4],
                    const float pos[2], int snap_mask, float offset,
                    float out[3], float normal[3])
{
    int i, ret = 0;
    bool r = false;
    float dist, best = INFINITY;
    float v[3], p[3] = {}, n[3] = {};

    // If tool_plane is set, we specifically use it.
    if (!plane_is_null(goxel->tool_plane)) {
        r = goxel_unproject_on_plane(goxel, viewport, pos,
                                     goxel->tool_plane, out, normal);
        ret = r ? SNAP_PLANE : 0;
        goto end;
    }

    for (i = 0; i < 7; i++) {
        if (!(snap_mask & (1 << i))) continue;
        if ((1 << i) == SNAP_MESH) {
            r = goxel_unproject_on_mesh(goxel, viewport, pos,
                                        goxel->layers_mesh, p, n);
        }
        if ((1 << i) == SNAP_PLANE)
            r = goxel_unproject_on_plane(goxel, viewport, pos,
                                         goxel->plane, p, n);
        if ((1 << i) == SNAP_SELECTION_IN)
            r = goxel_unproject_on_box(goxel, viewport, pos,
                                       &goxel->selection, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_SELECTION_OUT)
            r = goxel_unproject_on_box(goxel, viewport, pos,
                                       &goxel->selection, false,
                                       p, n, NULL);
        if ((1 << i) == SNAP_LAYER_OUT) {
            box_t box = mesh_get_box(goxel->image->active_layer->mesh, true);
            r = goxel_unproject_on_box(goxel, viewport, pos,
                                       &box, false,
                                       p, n, NULL);
        }
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(goxel, viewport, pos,
                                       &goxel->image->box, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(goxel, viewport, pos,
                                       &goxel->image->box, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_CAMERA) {
            camera_get_ray(&goxel->camera, pos, viewport, p, n);
            r = true;
        }

        if (!r)
            continue;

        mat4_mul_vec3(goxel->camera.view_mat, p, v);
        dist = -v[2];
        if (dist < 0 || dist > best) continue;

        vec3_copy(p, out);
        vec3_copy(n, normal);
        ret = 1 << i;

        if ((1 << i) == SNAP_IMAGE_BOX) {
            best = dist;
            continue;
        }

        break;
    }
end:
    if (ret && offset)
        vec3_iaddk(out, normal, offset);
    if (ret && (snap_mask & SNAP_ROUNDED)) {
        out[0] = round(out[0] - 0.5) + 0.5;
        out[1] = round(out[1] - 0.5) + 0.5;
        out[2] = round(out[2] - 0.5) + 0.5;
    }
    return ret;
}

static int on_drag(const gesture_t *gest, void *user);
static int on_pan(const gesture_t *gest, void *user);
static int on_rotate(const gesture_t *gest, void *user);
static int on_hover(const gesture_t *gest, void *user);

void goxel_init(goxel_t *gox)
{
    goxel = gox;
    memset(goxel, 0, sizeof(*goxel));

    render_init();
    shapes_init();
    sound_init();
    quat_set_identity(goxel->camera.rot);
    goxel->camera.dist = 128;
    goxel->camera.aspect = 1;
    quat_irotate(goxel->camera.rot, -M_PI / 4, 1, 0, 0);
    quat_irotate(goxel->camera.rot, -M_PI / 4, 0, 0, 1);

    goxel->image = image_new();

    goxel->layers_mesh = mesh_new();
    goxel->render_mesh = mesh_new();
    goxel_update_meshes(goxel, -1);
    goxel->selection = box_null;

    vec4_set(goxel->back_color, 70, 70, 70, 255);
    vec4_set(goxel->grid_color, 19, 19, 19, 255);
    vec4_set(goxel->image_box_color, 204, 204, 255, 255);

    // Load and set default palette.
    palette_load_all(&goxel->palettes);
    DL_FOREACH(goxel->palettes, goxel->palette) {
        if (strcmp(goxel->palette->name, "DB32") == 0)
            break;
    }
    goxel->palette = goxel->palette ?: goxel->palettes;

    action_exec2("tool_set_brush", "");
    goxel->tool_radius = 0.5;
    goxel->painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_OVER,
        .smoothness = 0,
        .color = {255, 255, 255, 255},
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
    plane_from_vectors(goxel->plane, vec3_zero, VEC(1, 0, 0), VEC(0, 1, 0));
    goxel->snap_mask = SNAP_PLANE | SNAP_MESH | SNAP_IMAGE_BOX;

    goxel->gestures.drag = (gesture_t) {
        .type = GESTURE_DRAG,
        .button = 0,
        .callback = on_drag,
    };
    goxel->gestures.pan = (gesture_t) {
        .type = GESTURE_DRAG,
        .button = 2,
        .callback = on_pan,
    };
    goxel->gestures.rotate = (gesture_t) {
        .type = GESTURE_DRAG,
        .button = 1,
        .callback = on_rotate,
    };
    goxel->gestures.hover = (gesture_t) {
        .type = GESTURE_HOVER,
        .callback = on_hover,
    };

    action_exec2("settings_load", "");
}

void goxel_release(goxel_t *goxel)
{
    proc_release(&goxel->proc);
    gui_release();
}

void goxel_iter(goxel_t *goxel, inputs_t *inputs)
{
    double time = sys_get_time();
    goxel->fps = mix(goxel->fps, 1.0 / (time - goxel->frame_time), 0.1);
    goxel->frame_time = time;
    goxel_set_help_text(goxel, NULL);
    goxel_set_hint_text(goxel, NULL);
    goxel->screen_size[0] = inputs->window_size[0];
    goxel->screen_size[1] = inputs->window_size[1];
    goxel->screen_scale = inputs->scale;
    goxel->rend.fbo = inputs->framebuffer;
    goxel->rend.scale = inputs->scale;
    camera_update(&goxel->camera);
    if (goxel->image->active_camera)
        camera_set(goxel->image->active_camera, &goxel->camera);
    mat4_copy(goxel->camera.view_mat, goxel->rend.view_mat);
    mat4_copy(goxel->camera.proj_mat, goxel->rend.proj_mat);
    gui_iter(goxel, inputs);
    sound_iter();
    goxel->frame_count++;
}

static void compute_view_rotation(const float rot[4],
        const float start_pos[2], const float end_pos[2],
        const float viewport[4], float out[4])
{
    float x1, y1, x2, y2, x_rot, z_rot, q1[4], q2[4], q[4], x_axis[4];
    x1 = start_pos[0] / viewport[2];
    y1 = start_pos[1] / viewport[3];
    x2 =   end_pos[0] / viewport[2];
    y2 =   end_pos[1] / viewport[3];
    z_rot = (x2 - x1) * 2 * M_PI;
    x_rot = -(y2 - y1) * 2 * M_PI;
    quat_from_axis(q1, z_rot, 0, 0, 1);
    quat_mul(rot, q1, q);
    quat_conjugate(q, q);
    quat_mul_vec4(q, VEC(1, 0, 0, 0), x_axis);
    quat_from_axis(q2, x_rot, x_axis[0], x_axis[1], x_axis[2]);
    quat_mul(q1, q2, out);
}

static void set_cursor_hint(cursor_t *curs)
{
    const char *snap_str = NULL;
    if (!curs->snaped) {
        goxel_set_hint_text(goxel, NULL);
        return;
    }
    if (curs->snaped == SNAP_MESH) snap_str = "mesh";
    if (curs->snaped == SNAP_PLANE) snap_str = "plane";
    if (IS_IN(curs->snaped, SNAP_SELECTION_IN, SNAP_SELECTION_OUT))
        snap_str = "selection";

    goxel_set_hint_text(goxel, "[%.0f %.0f %.0f] (%s)",
            curs->pos[0] - 0.5, curs->pos[1] - 0.5, curs->pos[2] - 0.5,
            snap_str);
}

static int on_drag(const gesture_t *gest, void *user)
{
    cursor_t *c = &goxel->cursor;
    if (gest->state == GESTURE_BEGIN)
        c->flags |= CURSOR_PRESSED;
    if (gest->state == GESTURE_END)
        c->flags &= ~CURSOR_PRESSED;

    c->snaped = goxel_unproject(
            goxel, gest->viewport, gest->pos, c->snap_mask,
            c->snap_offset, c->pos, c->normal);

    // Set some default values.  The tools can override them.
    // XXX: would be better to reset the cursor when we change tool!
    c->snap_mask = goxel->snap_mask;
    set_flag(&c->snap_mask, SNAP_ROUNDED, goxel->painter.smoothness == 0);
    c->snap_offset = 0;
    return 0;
}

static int on_pan(const gesture_t *gest, void *user)
{
    if (gest->state == GESTURE_BEGIN) {
        quat_copy(goxel->camera.ofs, goxel->move_origin.camera_ofs);
        vec2_copy(gest->pos, goxel->move_origin.pos);
    }
    float wpos[3] = {gest->pos[0], gest->pos[1], 0};
    float worigin_pos[3], wdelta[3], odelta[3];

    vec3_set(worigin_pos, goxel->move_origin.pos[0],
                          goxel->move_origin.pos[1], 0);
    vec3_sub(wpos, worigin_pos, wdelta);

    unproject_delta(wdelta, goxel->camera.view_mat,
                    goxel->camera.proj_mat, gest->viewport, odelta);
    vec3_imul(odelta, 2); // XXX: why do I need that?
    if (!goxel->camera.ortho)
        vec3_imul(odelta, goxel->camera.dist);
    vec3_add(goxel->move_origin.camera_ofs, odelta, goxel->camera.ofs);
    return 0;
}

static int on_rotate(const gesture_t *gest, void *user)
{
    float view_rot[4];
    if (gest->state == GESTURE_BEGIN) {
        quat_copy(goxel->camera.rot, goxel->move_origin.rotation);
        vec2_copy(gest->pos, goxel->move_origin.pos);
    }

    compute_view_rotation(goxel->move_origin.rotation,
                          goxel->move_origin.pos, gest->pos,
                          gest->viewport, view_rot);
    quat_mul(goxel->move_origin.rotation, view_rot, goxel->camera.rot);
    return 0;
}

static int on_hover(const gesture_t *gest, void *user)
{
    cursor_t *c = &goxel->cursor;
    c->snaped = goxel_unproject(
                    goxel, gest->viewport, gest->pos, c->snap_mask,
                    c->snap_offset, c->pos, c->normal);
    set_cursor_hint(c);
    c->flags &= ~CURSOR_PRESSED;
    // Set some default values.  The tools can override them.
    // XXX: would be better to reset the cursor when we change tool!
    c->snap_mask = goxel->snap_mask;
    set_flag(&c->snap_mask, SNAP_ROUNDED, goxel->painter.smoothness == 0);
    c->snap_offset = 0;
    return 0;
}


// XXX: Cleanup this.
void goxel_mouse_in_view(goxel_t *goxel, const float viewport[4],
                         const inputs_t *inputs)
{
    float x_axis[4], q[4], p[3], n[3];
    gesture_t *gests[] = {&goxel->gestures.drag,
                          &goxel->gestures.pan,
                          &goxel->gestures.rotate,
                          &goxel->gestures.hover};

    gesture_update(4, gests, inputs, viewport, NULL);
    set_flag(&goxel->cursor.flags, CURSOR_SHIFT, inputs->keys[KEY_LEFT_SHIFT]);
    set_flag(&goxel->cursor.flags, CURSOR_CTRL, inputs->keys[KEY_CONTROL]);
    goxel->painter.box = !box_is_null(goxel->image->active_layer->box.mat) ?
                         &goxel->image->active_layer->box :
                         !box_is_null(goxel->image->box.mat) ?
                         &goxel->image->box : NULL;

    tool_iter(goxel->tool, viewport);

    if (inputs->mouse_wheel) {
        goxel->camera.dist /= pow(1.1, inputs->mouse_wheel);

        // Auto adjust the camera rotation position.
        if (goxel_unproject_on_mesh(goxel, viewport, inputs->touches[0].pos,
                                    goxel->layers_mesh, p, n)) {
            camera_set_target(&goxel->camera, p);
        }
        return;
    }

    // handle keyboard rotations
    if (inputs->keys[KEY_LEFT])
        quat_irotate(goxel->camera.rot, 0.05, 0, 0, +1);
    if (inputs->keys[KEY_RIGHT])
        quat_irotate(goxel->camera.rot, 0.05, 0, 0, -1);
    if (inputs->keys[KEY_UP]) {
        quat_conjugate(goxel->camera.rot, q);
        quat_mul_vec4(q, VEC(1, 0, 0, 0), x_axis);
        quat_irotate(goxel->camera.rot, -0.05,
                     x_axis[0], x_axis[1], x_axis[2]);
    }
    if (inputs->keys[KEY_DOWN]) {
        quat_conjugate(goxel->camera.rot, q);
        quat_mul_vec4(q, VEC(1, 0, 0, 0), x_axis);
        quat_irotate(goxel->camera.rot, +0.05,
                     x_axis[0], x_axis[1], x_axis[2]);
    }
    // C: recenter the view:
    if (inputs->keys['C']) {
        if (goxel_unproject_on_mesh(goxel, viewport, inputs->touches[0].pos,
                                    goxel->layers_mesh, p, n)) {
            camera_set_target(&goxel->camera, p);
        }
    }
}

void goxel_render(goxel_t *goxel)
{
    GL(glViewport(0, 0, goxel->screen_size[0] * goxel->screen_scale,
                        goxel->screen_size[1] * goxel->screen_scale));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, goxel->rend.fbo));
    GL(glClearColor(0, 0, 0, 1));
    GL(glStencilMask(0xFF));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));
    gui_render();
}

static void render_export_viewport(goxel_t *goxel, const float viewport[4])
{
    // Render the export viewport.
    int w = goxel->image->export_width;
    int h = goxel->image->export_height;
    float aspect = (float)w/h;
    float plane[4][4];
    mat4_set_identity(plane);
    if (aspect < goxel->camera.aspect) {
        mat4_iscale(plane, aspect / goxel->camera.aspect, 1, 1);
    } else {
        mat4_iscale(plane, 1, goxel->camera.aspect / aspect, 1);
    }
    render_rect(&goxel->rend, plane, EFFECT_STRIP);
}

void goxel_render_view(goxel_t *goxel, const float viewport[4])
{
    layer_t *layer;
    renderer_t *rend = &goxel->rend;
    const uint8_t layer_box_color[4] = {128, 128, 255, 255};

    goxel->camera.aspect = viewport[2] / viewport[3];
    camera_update(&goxel->camera);

    render_mesh(rend, goxel->render_mesh, 0);
    if (!box_is_null(goxel->image->active_layer->box.mat))
        render_box(rend, &goxel->image->active_layer->box,
                   layer_box_color, EFFECT_WIREFRAME);

    // Render all the image layers.
    DL_FOREACH(goxel->image->layers, layer) {
        if (layer->visible && layer->image)
            render_img(rend, layer->image, layer->mat, EFFECT_NO_SHADING);
    }

    if (goxel->tool->id == TOOL_SELECTION)
        render_box(rend, &goxel->selection, NULL,
                   EFFECT_STRIP | EFFECT_WIREFRAME);

    // XXX: make a toggle for debug informations.
    if ((0)) {
        box_t b;
        uint8_t c[4];
        vec4_set(c, 0, 255, 0, 80);
        b = mesh_get_box(goxel->layers_mesh, true);
        render_box(rend, &b, c, EFFECT_WIREFRAME);
        vec4_set(c, 0, 255, 255, 80);
        b = mesh_get_box(goxel->layers_mesh, false);
        render_box(rend, &b, c, EFFECT_WIREFRAME);
    }
    if (!goxel->plane_hidden)
        render_plane(rend, goxel->plane, goxel->grid_color);
    if (!box_is_null(goxel->image->box.mat))
        render_box(rend, &goxel->image->box, goxel->image_box_color,
                   EFFECT_SEE_BACK);
    if (goxel->show_export_viewport)
        render_export_viewport(goxel, viewport);
}

void image_update(image_t *img);
void goxel_update_meshes(goxel_t *goxel, int mask)
{
    layer_t *layer;
    mesh_t *mesh;

    image_update(goxel->image);

    if (    (mask & MESH_LAYERS) || (mask & MESH_PICK) ||
            ((mask & MESH_RENDER) && !goxel->tool_mesh)) {
        mesh_clear(goxel->layers_mesh);
        DL_FOREACH(goxel->image->layers, layer) {
            if (!layer->visible) continue;
            mesh_merge(goxel->layers_mesh, layer->mesh, MODE_OVER, NULL);
        }
    }

    if ((mask & MESH_RENDER) && goxel->tool_mesh) {
        mesh_clear(goxel->render_mesh);
        DL_FOREACH(goxel->image->layers, layer) {
            if (!layer->visible) continue;
            mesh = layer->mesh;
            if (mesh == goxel->image->active_layer->mesh)
                mesh = goxel->tool_mesh;
            mesh_merge(goxel->render_mesh, mesh, MODE_OVER, NULL);
        }
    }
    if ((mask & MESH_RENDER) && !goxel->tool_mesh)
        mesh_set(goxel->render_mesh, goxel->layers_mesh);
}

// Render the view into an RGBA buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h)
{
    camera_t camera = goxel->camera;
    mesh_t *mesh;
    texture_t *fbo;
    renderer_t rend = goxel->rend;
    int rect[4] = {0, 0, w * 2, h * 2};
    uint8_t *tmp_buf;
    uint8_t clear_color[4] = {0, 0, 0, 0};

    camera.aspect = (float)w / h;
    mesh = goxel->layers_mesh;
    fbo = texture_new_buffer(w * 2, h * 2, TF_DEPTH);

    camera_update(&camera);
    mat4_copy(camera.view_mat, rend.view_mat);
    mat4_copy(camera.proj_mat, rend.proj_mat);
    rend.fbo = fbo->framebuffer;

    render_mesh(&rend, mesh, 0);
    render_submit(&rend, rect, clear_color);
    tmp_buf = calloc(w * h * 4 , 4);
    texture_get_data(fbo, w * 2, h * 2, 4, tmp_buf);
    img_downsample(tmp_buf, w * 2, h * 2, 4, buf);
    free(tmp_buf);
    texture_delete(fbo);
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

void goxel_import_image_plane(goxel_t *goxel, const char *path)
{
    layer_t *layer;
    texture_t *tex;
    tex = texture_new_image(path, TF_NEAREST);
    if (!tex) return;
    image_history_push(goxel->image);
    layer = image_add_layer(goxel->image);
    sprintf(layer->name, "img");
    layer->image = tex;
    mat4_iscale(layer->mat, layer->image->w, layer->image->h, 1);
}

static int search_action_for_format_cb(const action_t *a, void *user)
{
    const char *path = USER_GET(user, 0);
    const char *type = USER_GET(user, 1);
    const action_t **ret = USER_GET(user, 2);
    if (!a->file_format.ext) return 0;
    if (!str_startswith(a->id, type)) return 0;
    if (!str_endswith(path, a->file_format.ext + 1)) return 0;
    *ret = a;
    return 1;
}

static int goxel_import_file(const char *path)
{
    const action_t *a = NULL;
    if (str_endswith(path, ".gox")) {
        load_from_file(goxel, path);
        return 0;
    }
    actions_iter(search_action_for_format_cb, USER_PASS(path, "import_", &a));
    if (!a) return -1;
    action_exec(a, "p", path);
    return 0;
}

ACTION_REGISTER(import,
    .help = "Import a file",
    .cfunc = goxel_import_file,
    .csig = "vp",
    .flags = ACTION_TOUCH_IMAGE,
)

static int goxel_export_to_file(const char *path)
{
    const action_t *a = NULL;
    actions_iter(search_action_for_format_cb, USER_PASS(path, "export_", &a));
    if (!a) return -1;
    return action_exec(a, "p", path);
}

ACTION_REGISTER(export,
    .help = "Export to a file",
    .cfunc = goxel_export_to_file,
    .csig = "vp",
)

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
        .color = {255, 255, 255, 255},
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

static void reset_selection(void)
{
    goxel->selection = box_null;
}

ACTION_REGISTER(reset_selection,
    .help = "Reset the selection",
    .cfunc = reset_selection,
    .csig = "vp",
)

static void fill_selection(layer_t *layer)
{
    if (box_is_null(goxel->selection.mat)) return;
    layer = layer ?: goxel->image->active_layer;
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

static void copy_action(void)
{
    painter_t painter;
    mesh_delete(goxel->clipboard.mesh);
    goxel->clipboard.box = goxel->selection;
    goxel->clipboard.mesh = mesh_copy(goxel->image->active_layer->mesh);
    if (!box_is_null(goxel->selection.mat)) {
        painter = (painter_t) {
            .shape = &shape_cube,
            .mode = MODE_INTERSECT,
            .color = {255, 255, 255, 255},
        };
        mesh_op(goxel->clipboard.mesh, &painter, &goxel->selection);
    }
}

static void past_action(void)
{
    mesh_t *mesh = goxel->image->active_layer->mesh;
    mesh_t *tmp;
    float p1[3], p2[3], mat[4][4];

    mat4_set_identity(mat);
    if (!goxel->clipboard.mesh) return;

    tmp = mesh_copy(goxel->clipboard.mesh);
    if (    !box_is_null(goxel->selection.mat) &&
            !box_is_null(goxel->clipboard.box.mat)) {
        vec3_copy(goxel->selection.p, p1);
        vec3_copy(goxel->clipboard.box.p, p2);
        mat4_itranslate(mat, +p1[0], +p1[1], +p1[2]);
        mat4_itranslate(mat, -p2[0], -p2[1], -p2[2]);
        mesh_move(tmp, mat);
    }
    mesh_merge(mesh, tmp, MODE_OVER, NULL);
    mesh_delete(tmp);
}

ACTION_REGISTER(copy,
    .help = "Copy",
    .cfunc = copy_action,
    .csig = "v",
    .shortcut = "Ctrl C",
    .flags = 0,
)

ACTION_REGISTER(past,
    .help = "Past",
    .cfunc = past_action,
    .csig = "v",
    .shortcut = "Ctrl V",
    .flags = ACTION_TOUCH_IMAGE,
)

#define HS2 (M_SQRT2 / 2.0)


static int view_default(const action_t *a, astack_t *s)
{
    quat_set_identity(goxel->camera.rot);
    quat_irotate(goxel->camera.rot, -M_PI / 4, 1, 0, 0);
    quat_irotate(goxel->camera.rot, -M_PI / 4, 0, 0, 1);
    goxel_update_meshes(goxel, -1);
    return 0;
}

static int view_set(const action_t *a, astack_t *s)
{
    quat_copy(a->data, goxel->camera.rot);
    goxel_update_meshes(goxel, -1);
    return 0;
}

ACTION_REGISTER(view_left,
    .help = "Set camera view to left",
    .func = view_set,
    .data = &QUAT(0.5, -0.5, 0.5, 0.5),
    .shortcut = "Ctrl 3",
)

ACTION_REGISTER(view_right,
    .help = "Set camera view to right",
    .func = view_set,
    .data = &QUAT(-0.5, 0.5, 0.5, 0.5),
    .shortcut = "3",
)

ACTION_REGISTER(view_top,
    .help = "Set camera view to top",
    .func = view_set,
    .data = &QUAT(1, 0, 0, 0),
    .shortcut = "7",
)

ACTION_REGISTER(view_default,
    .help = "Set camera view to default",
    .func = view_default,
    .shortcut = "5",
)

ACTION_REGISTER(view_front,
    .help = "Set camera view to front",
    .func = view_set,
    .data = &QUAT(HS2, -HS2, 0, 0),
    .shortcut = "1",
)

static void quit(void)
{
    goxel->quit = true;
}
ACTION_REGISTER(quit,
    .help = "Quit the application",
    .cfunc = quit,
    .csig = "v",
    .shortcut = "Ctrl Q",
)

static void undo(void) { image_undo(goxel->image); }
static void redo(void) { image_redo(goxel->image); }

ACTION_REGISTER(undo,
    .help = "Undo",
    .cfunc = undo,
    .csig = "v",
    .shortcut = "Ctrl Z",
)

ACTION_REGISTER(redo,
    .help = "Redo",
    .cfunc = redo,
    .csig = "v",
    .shortcut = "Ctrl Y",
)
