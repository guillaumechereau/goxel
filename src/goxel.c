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
bool goxel_unproject_on_plane(goxel_t *goxel, const vec4_t *view,
                              const vec2_t *pos, const plane_t *plane,
                              vec3_t *out, vec3_t *normal)
{
    // If the angle between the screen and the plane is close to 90 deg,
    // the projection fails.  This prevents projecting too far away.
    const float min_angle_cos = 0.1;
    // XXX: pos should already be in windows coordinates.
    vec3_t wpos = vec3(pos->x, pos->y, 0);
    vec3_t opos;
    vec3_t onorm;

    camera_get_ray(&goxel->camera, &wpos.xy, view, &opos, &onorm);
    if (fabs(vec3_dot(onorm.v, plane->n.v)) <= min_angle_cos)
        return false;

    if (!plane_line_intersection(*plane, opos, onorm, out))
        return false;
    *out = mat4_mul_vec3(plane->mat, *out);
    *normal = plane->n;
    return true;
}

bool goxel_unproject_on_box(goxel_t *goxel, const vec4_t *view,
                     const vec2_t *pos, const box_t *box, bool inside,
                     vec3_t *out, vec3_t *normal,
                     int *face)
{
    int f;
    vec3_t wpos = vec3(pos->x, pos->y, 0);
    vec3_t opos;
    vec3_t onorm;
    plane_t plane;

    if (box_is_null(*box)) return false;
    camera_get_ray(&goxel->camera, &wpos.xy, view, &opos, &onorm);
    for (f = 0; f < 6; f++) {
        plane.mat = box->mat;
        mat4_imul(&plane.mat, FACES_MATS[f]);

        if (!inside && vec3_dot(plane.n.v, onorm.v) >= 0)
            continue;
        if (inside && vec3_dot(plane.n.v, onorm.v) <= 0)
            continue;
        if (!plane_line_intersection(plane, opos, onorm, out))
            continue;
        if (!(out->x >= -1 && out->x < 1 && out->y >= -1 && out->y < 1))
            continue;
        if (face) *face = f;
        *out = mat4_mul_vec3(plane.mat, *out);
        vec3_normalize(plane.n.v, normal->v);
        if (inside) vec3_imul(normal->v, -1);
        return true;
    }
    return false;
}

bool goxel_unproject_on_mesh(goxel_t *goxel, const vec4_t *view,
                             const vec2_t *pos, mesh_t *mesh,
                             vec3_t *out, vec3_t *normal)
{
    vec2_t view_size = view->zw;
    // XXX: No need to render the fbo if it is not dirty.
    if (goxel->pick_fbo && !vec2_equal(
                vec2(goxel->pick_fbo->w, goxel->pick_fbo->h).v, view_size.v)) {
        texture_delete(goxel->pick_fbo);
        goxel->pick_fbo = NULL;
    }

    if (!goxel->pick_fbo) {
        goxel->pick_fbo = texture_new_buffer(
                view_size.x, view_size.y, TF_DEPTH);
    }

    renderer_t rend = {
        .view_mat = goxel->rend.view_mat,
        .proj_mat = goxel->rend.proj_mat,
        .settings = goxel->rend.settings,
    };
    uint32_t pixel;
    int voxel_pos[3];
    int face, block_id, block_pos[3];
    int x, y;
    int rect[4] = {0, 0, view_size.x, view_size.y};
    uint8_t clear_color[4] = {0, 0, 0, 0};

    rend.settings.shadow = 0;
    rend.fbo = goxel->pick_fbo->framebuffer;
    rend.scale = 1;
    render_mesh(&rend, mesh, EFFECT_RENDER_POS);
    render_submit(&rend, rect, clear_color);

    x = round(pos->x - view->x);
    y = round(pos->y - view->y);
    GL(glViewport(0, 0, goxel->pick_fbo->w, goxel->pick_fbo->h));
    if (x < 0 || x >= view_size.x ||
        y < 0 || y >= view_size.y) return false;
    GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel));

    unpack_pos_data(pixel, voxel_pos, &face, &block_id);
    if (!block_id) return false;
    render_get_block_pos(&rend, mesh, block_id, block_pos);
    *out = vec3(block_pos[0] + voxel_pos[0] + 0.5,
                block_pos[1] + voxel_pos[1] + 0.5,
                block_pos[2] + voxel_pos[2] + 0.5);
    normal->x = FACES_NORMALS[face][0];
    normal->y = FACES_NORMALS[face][1];
    normal->z = FACES_NORMALS[face][2];
    vec3_iaddk(out->v, normal->v, 0.5);
    return true;
}


int goxel_unproject(goxel_t *goxel, const vec4_t *view,
                    const vec2_t *pos, int snap_mask, float offset,
                    vec3_t *out, vec3_t *normal)
{
    int i, ret = 0;
    vec3_t p = vec3_zero, n = vec3_zero;
    bool r = false;
    float dist, best = INFINITY;

    // If tool_plane is set, we specifically use it.
    if (!plane_is_null(goxel->tool_plane)) {
        r = goxel_unproject_on_plane(goxel, view, pos,
                                     &goxel->tool_plane, out, normal);
        ret = r ? SNAP_PLANE : 0;
        goto end;
    }

    for (i = 0; i < 7; i++) {
        if (!(snap_mask & (1 << i))) continue;
        if ((1 << i) == SNAP_MESH) {
            r = goxel_unproject_on_mesh(goxel, view, pos,
                                        goxel->layers_mesh, &p, &n);
        }
        if ((1 << i) == SNAP_PLANE)
            r = goxel_unproject_on_plane(goxel, view, pos,
                                         &goxel->plane, &p, &n);
        if ((1 << i) == SNAP_SELECTION_IN)
            r = goxel_unproject_on_box(goxel, view, pos,
                                       &goxel->selection, true,
                                       &p, &n, NULL);
        if ((1 << i) == SNAP_SELECTION_OUT)
            r = goxel_unproject_on_box(goxel, view, pos,
                                       &goxel->selection, false,
                                       &p, &n, NULL);
        if ((1 << i) == SNAP_LAYER_OUT) {
            box_t box = mesh_get_box(goxel->image->active_layer->mesh, true);
            r = goxel_unproject_on_box(goxel, view, pos,
                                       &box, false,
                                       &p, &n, NULL);
        }
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(goxel, view, pos,
                                       &goxel->image->box, true,
                                       &p, &n, NULL);
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(goxel, view, pos,
                                       &goxel->image->box, true,
                                       &p, &n, NULL);
        if ((1 << i) == SNAP_CAMERA) {
            camera_get_ray(&goxel->camera, pos, view, &p, &n);
            r = true;
        }

        if (!r)
            continue;

        dist = -mat4_mul_vec3(goxel->camera.view_mat, p).z;
        if (dist < 0 || dist > best) continue;

        *out = p;
        *normal = n;
        ret = 1 << i;

        if ((1 << i) == SNAP_IMAGE_BOX) {
            best = dist;
            continue;
        }

        break;
    }
end:
    if (ret && offset)
        vec3_iaddk(out->v, normal->v, offset);
    if (ret && (snap_mask & SNAP_ROUNDED)) {
        out->x = round(out->x - 0.5) + 0.5;
        out->y = round(out->y - 0.5) + 0.5;
        out->z = round(out->z - 0.5) + 0.5;
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
    goxel->camera.ofs = vec3_zero;
    goxel->camera.rot = quat_identity;
    goxel->camera.dist = 128;
    goxel->camera.aspect = 1;
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 1, 0, 0);
    quat_irotate(&goxel->camera.rot, -M_PI / 4, 0, 0, 1);

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
    goxel->plane = plane(vec3_zero, vec3(1, 0, 0), vec3(0, 1, 0));
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
    goxel->rend.view_mat = goxel->camera.view_mat;
    goxel->rend.proj_mat = goxel->camera.proj_mat;
    gui_iter(goxel, inputs);
    sound_iter();
    goxel->frame_count++;
}

static quat_t compute_view_rotation(const quat_t *rot,
        const vec2_t *start_pos, const vec2_t *end_pos,
        const vec4_t *view)
{
    quat_t q1, q2;
    float x1, y1, x2, y2, x_rot, z_rot;
    vec4_t x_axis;
    x1 = start_pos->x / view->z;
    y1 = start_pos->y / view->w;
    x2 =   end_pos->x / view->z;
    y2 =   end_pos->y / view->w;
    z_rot = (x2 - x1) * 2 * M_PI;
    x_rot = -(y2 - y1) * 2 * M_PI;
    q1 = quat_from_axis(z_rot, 0, 0, 1);
    x_axis = quat_mul_vec4(quat_conjugate(quat_mul(*rot, q1)),
                           vec4(1, 0, 0, 0));
    q2 = quat_from_axis(x_rot, x_axis.x, x_axis.y, x_axis.z);
    return quat_mul(q1, q2);
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
            curs->pos.x - 0.5, curs->pos.y - 0.5, curs->pos.z - 0.5,
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
            goxel, &gest->view, &gest->pos, c->snap_mask,
            c->snap_offset, &c->pos, &c->normal);

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
        goxel->move_origin.camera_ofs = goxel->camera.ofs;
        goxel->move_origin.pos = gest->pos;
    }
    vec3_t wpos = vec3(gest->pos.x, gest->pos.y, 0);
    vec3_t worigin_pos = vec3(goxel->move_origin.pos.x,
                              goxel->move_origin.pos.y, 0);
    vec3_t wdelta;
    vec3_sub(wpos.v, worigin_pos.v, wdelta.v);
    vec3_t odelta = unproject_delta(&wdelta, &goxel->camera.view_mat,
                                    &goxel->camera.proj_mat, &gest->view);
    vec3_imul(odelta.v, 2); // XXX: why do I need that?
    if (!goxel->camera.ortho)
        vec3_imul(odelta.v, goxel->camera.dist);
    vec3_add(goxel->move_origin.camera_ofs.v, odelta.v, goxel->camera.ofs.v);
    return 0;
}

static int on_rotate(const gesture_t *gest, void *user)
{
    if (gest->state == GESTURE_BEGIN) {
        goxel->move_origin.rotation = goxel->camera.rot;
        goxel->move_origin.pos = gest->pos;
    }
    goxel->camera.rot = quat_mul(goxel->move_origin.rotation,
            compute_view_rotation(&goxel->move_origin.rotation,
                &goxel->move_origin.pos, &gest->pos,
                &gest->view));

    return 0;
}

static int on_hover(const gesture_t *gest, void *user)
{
    cursor_t *c = &goxel->cursor;
    c->snaped = goxel_unproject(
                    goxel, &gest->view, &gest->pos, c->snap_mask,
                    c->snap_offset, &c->pos, &c->normal);
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
void goxel_mouse_in_view(goxel_t *goxel, const vec4_t *view,
                         const inputs_t *inputs)
{
    vec4_t x_axis;
    gesture_t *gests[] = {&goxel->gestures.drag,
                          &goxel->gestures.pan,
                          &goxel->gestures.rotate,
                          &goxel->gestures.hover};

    gesture_update(4, gests, inputs, view, NULL);
    set_flag(&goxel->cursor.flags, CURSOR_SHIFT, inputs->keys[KEY_LEFT_SHIFT]);
    set_flag(&goxel->cursor.flags, CURSOR_CTRL, inputs->keys[KEY_CONTROL]);
    goxel->painter.box = !box_is_null(goxel->image->active_layer->box) ?
                         &goxel->image->active_layer->box :
                         !box_is_null(goxel->image->box) ?
                         &goxel->image->box : NULL;

    tool_iter(goxel->tool, view);

    if (inputs->mouse_wheel) {
        goxel->camera.dist /= pow(1.1, inputs->mouse_wheel);

        // Auto adjust the camera rotation position.
        vec3_t p, n;
        if (goxel_unproject_on_mesh(goxel, view, &inputs->touches[0].pos,
                                    goxel->layers_mesh, &p, &n)) {
            camera_set_target(&goxel->camera, &p);
        }
        return;
    }

    // handle keyboard rotations
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
    if (inputs->keys['C']) {
        vec3_t p, n;
        if (goxel_unproject_on_mesh(goxel, view, &inputs->touches[0].pos,
                                    goxel->layers_mesh, &p, &n)) {
            camera_set_target(&goxel->camera, &p);
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

static void render_export_viewport(goxel_t *goxel, const vec4_t *view)
{
    // Render the export viewport.
    int w = goxel->image->export_width;
    int h = goxel->image->export_height;
    float aspect = (float)w/h;
    plane_t plane;
    plane.mat = mat4_identity;
    if (aspect < goxel->camera.aspect) {
        mat4_iscale(&plane.mat, aspect / goxel->camera.aspect, 1, 1);
    } else {
        mat4_iscale(&plane.mat, 1, goxel->camera.aspect / aspect, 1);
    }
    render_rect(&goxel->rend, &plane, EFFECT_STRIP);
}

void goxel_render_view(goxel_t *goxel, const vec4_t *rect)
{
    layer_t *layer;
    renderer_t *rend = &goxel->rend;
    const uint8_t layer_box_color[4] = {128, 128, 255, 255};

    goxel->camera.aspect = rect->z / rect->w;
    camera_update(&goxel->camera);

    render_mesh(rend, goxel->render_mesh, 0);
    if (!box_is_null(goxel->image->active_layer->box))
        render_box(rend, &goxel->image->active_layer->box,
                   layer_box_color, EFFECT_WIREFRAME);

    // Render all the image layers.
    DL_FOREACH(goxel->image->layers, layer) {
        if (layer->visible && layer->image)
            render_img(rend, layer->image, &layer->mat, EFFECT_NO_SHADING);
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
        render_plane(rend, &goxel->plane, goxel->grid_color);
    if (!box_is_null(goxel->image->box))
        render_box(rend, &goxel->image->box, goxel->image_box_color,
                   EFFECT_SEE_BACK);
    if (goxel->show_export_viewport)
        render_export_viewport(goxel, rect);
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
    rend.view_mat = camera.view_mat;
    rend.proj_mat = camera.proj_mat;
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
    mat4_iscale(&layer->mat, layer->image->w, layer->image->h, 1);
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
    if (box_is_null(goxel->selection)) return;
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
    if (!box_is_null(goxel->selection)) {
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
    vec3_t p1, p2;
    mat4_t mat = mat4_identity;
    if (!goxel->clipboard.mesh) return;

    tmp = mesh_copy(goxel->clipboard.mesh);
    if (    !box_is_null(goxel->selection) &&
            !box_is_null(goxel->clipboard.box)) {
        p1 = goxel->selection.p;
        p2 = goxel->clipboard.box.p;
        mat4_itranslate(&mat, +p1.x, +p1.y, +p1.z);
        mat4_itranslate(&mat, -p2.x, -p2.y, -p2.z);
        mesh_move(tmp, &mat);
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
	goxel->camera.rot = quat_identity;
	quat_irotate(&goxel->camera.rot, -M_PI / 4, 1, 0, 0);
	quat_irotate(&goxel->camera.rot, -M_PI / 4, 0, 0, 1);
	goxel_update_meshes(goxel, -1);
	
	return 0;
}

static int view_set(const action_t *a, astack_t *s)
{
    goxel->camera.rot = *((quat_t*)a->data);
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
