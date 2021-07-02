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
#include "xxhash.h"
#include "file_format.h"

#include "shader_cache.h"

#include <stdarg.h>

// The global goxel instance.
goxel_t goxel = {};

texture_t *texture_new_image(const char *path, int flags)
{
    char *data;
    uint8_t *img;
    bool need_to_free = false;
    int size;
    int w, h, bpp = 0;
    texture_t *tex;

    if (str_startswith(path, "asset://")) {
        data = (char*)assets_get(path, &size);
    } else {
        data = read_file(path, &size);
        need_to_free = true;
    }
    img = img_read_from_mem(data, size, &w, &h, &bpp);
    tex = texture_new_from_buf(img, w, h, bpp, flags);
    tex->path = strdup(path);
    free(img);
    if (need_to_free) free(data);
    return tex;
}

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

// Conveniance function to add a char in the inputs.
void inputs_insert_char(inputs_t *inputs, uint32_t c)
{
    int i;
    if (c > 0 && c < 0x10000) {
        for (i = 0; i < ARRAY_SIZE(inputs->chars); i++) {
            if (!inputs->chars[i]) {
                inputs->chars[i] = c;
                break;
            }
        }
    }
}

static camera_t *get_camera(void)
{
    if (!goxel.image->cameras)
        image_add_camera(goxel.image, NULL);
    if (goxel.image->active_camera)
        return goxel.image->active_camera;
    return goxel.image->cameras;
}

// XXX: lot of cleanup to do here.
static bool goxel_unproject_on_plane(
        const float viewport[4], const float pos[2], const float plane[4][4],
        float out[3], float normal[3])
{
    // If the angle between the screen and the plane is close to 90 deg,
    // the projection fails.  This prevents projecting too far away.
    const float min_angle_cos = 0.1;
    // XXX: pos should already be in windows coordinates.
    float wpos[3] = {pos[0], pos[1], 0};
    float opos[3], onorm[3];
    camera_t *cam = get_camera();

    camera_get_ray(cam, wpos, viewport, opos, onorm);
    if (fabs(vec3_dot(onorm, plane[2])) <= min_angle_cos)
        return false;

    if (!plane_line_intersection(plane, opos, onorm, out))
        return false;
    mat4_mul_vec3(plane, out, out);
    vec3_copy(plane[2], normal);
    return true;
}

static bool goxel_unproject_on_box(
        const float viewport[4], const float pos[2],
        const float box[4][4], bool inside, float out[3],
        float normal[3], int *face)
{
    int f;
    float wpos[3] = {pos[0], pos[1], 0};
    float opos[3], onorm[3];
    float plane[4][4];
    camera_t *cam = get_camera();

    if (box_is_null(box)) return false;
    camera_get_ray(cam, wpos, viewport, opos, onorm);
    for (f = 0; f < 6; f++) {
        mat4_copy(box, plane);
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

static bool goxel_unproject_on_mesh(
        const float view[4], const float pos[2], const mesh_t *mesh,
        float out[3], float normal[3])
{
    int view_size[2] = {view[2], view[3]};
    // XXX: No need to render the fbo if it is not dirty.
    if (goxel.pick_fbo && (goxel.pick_fbo->w != view_size[0] ||
                           goxel.pick_fbo->h != view_size[1])) {
        texture_delete(goxel.pick_fbo);
        goxel.pick_fbo = NULL;
    }

    if (!goxel.pick_fbo) {
        goxel.pick_fbo = texture_new_buffer(
                view_size[0], view_size[1], TF_DEPTH);
    }

    renderer_t rend = {.settings = goxel.rend.settings};
    mat4_copy(goxel.rend.view_mat, rend.view_mat);
    mat4_copy(goxel.rend.proj_mat, rend.proj_mat);

    uint32_t pixel;
    int voxel_pos[3];
    int face, block_id, block_pos[3];
    int x, y;
    float rect[4] = {0, 0, view_size[0], view_size[1]};
    uint8_t clear_color[4] = {0, 0, 0, 0};

    rend.settings.shadow = 0;
    rend.fbo = goxel.pick_fbo->framebuffer;
    rend.scale = 1;
    render_mesh(&rend, mesh, NULL, EFFECT_RENDER_POS);
    render_submit(&rend, rect, clear_color);

    x = round(pos[0] - view[0]);
    y = round(pos[1] - view[1]);
    GL(glViewport(0, 0, goxel.pick_fbo->w, goxel.pick_fbo->h));
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


int goxel_unproject(const float viewport[4],
                    const float pos[2], int snap_mask, float offset,
                    float out[3], float normal[3])
{
    int i, ret = 0;
    bool r = false;
    float dist, best = INFINITY;
    float v[3], p[3] = {}, n[3] = {}, box[4][4];
    camera_t *cam = get_camera();

    // If tool_plane is set, we specifically use it.
    if (!plane_is_null(goxel.tool_plane)) {
        r = goxel_unproject_on_plane(viewport, pos,
                                     goxel.tool_plane, out, normal);
        ret = r ? SNAP_PLANE : 0;
        goto end;
    }

    for (i = 0; i < 7; i++) {
        if (!(snap_mask & (1 << i))) continue;
        if ((1 << i) == SNAP_MESH) {
            r = goxel_unproject_on_mesh(viewport, pos,
                            goxel_get_layers_mesh(goxel.image), p, n);
        }
        if ((1 << i) == SNAP_PLANE)
            r = goxel_unproject_on_plane(viewport, pos,
                                         goxel.plane, p, n);
        if ((1 << i) == SNAP_SELECTION_IN)
            r = goxel_unproject_on_box(viewport, pos,
                                       goxel.selection, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_SELECTION_OUT)
            r = goxel_unproject_on_box(viewport, pos,
                                       goxel.selection, false,
                                       p, n, NULL);
        if ((1 << i) == SNAP_LAYER_OUT) {
            mesh_get_box(goxel.image->active_layer->mesh, true, box);
            r = goxel_unproject_on_box(viewport, pos, box, false,
                                       p, n, NULL);
        }
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(viewport, pos,
                                       goxel.image->box, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_IMAGE_BOX)
            r = goxel_unproject_on_box(viewport, pos,
                                       goxel.image->box, true,
                                       p, n, NULL);
        if ((1 << i) == SNAP_CAMERA) {
            camera_get_ray(cam, pos, viewport, p, n);
            r = true;
        }

        if (!r)
            continue;

        mat4_mul_vec3(cam->view_mat, p, v);
        dist = -v[2];
        if (dist < 0 || dist > best) continue;

        vec3_copy(p, out);
        vec3_copy(n, normal);
        ret = 1 << i;
        best = dist;
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
static int on_zoom(const gesture_t *gest, void *user);
static int on_rotate(const gesture_t *gest, void *user);
static int on_hover(const gesture_t *gest, void *user);

static void goxel_init_sound()
{
#ifdef SOUND
    sound_init();
    sound_register("build", assets_get("asset://data/sounds/build.wav", NULL));
    sound_register("click", assets_get("asset://data/sounds/click.wav", NULL));
#endif
}

void goxel_add_gesture(int type, int button,
                       int (*fn)(const gesture_t *gest, void *user))
{
    goxel.gestures[goxel.gestures_count] = calloc(1, sizeof(gesture_t));
    *goxel.gestures[goxel.gestures_count] = (gesture_t) {
        .type = type,
        .button = button,
        .callback = fn,
    };
    goxel.gestures_count++;
}

KEEPALIVE
void goxel_init(void)
{
    shapes_init();
    goxel_init_sound();

    // Load and set default palette.
    palette_load_all(&goxel.palettes);
    DL_FOREACH(goxel.palettes, goxel.palette) {
        if (strcmp(goxel.palette->name, "DB32") == 0)
            break;
    }
    goxel.palette = goxel.palette ?: goxel.palettes;

    goxel_add_gesture(GESTURE_DRAG, GESTURE_LMB, on_drag);
    goxel_add_gesture(GESTURE_DRAG, GESTURE_RMB, on_pan);
    goxel_add_gesture(GESTURE_DRAG, GESTURE_MMB | GESTURE_SHIFT, on_pan);
    goxel_add_gesture(GESTURE_DRAG, GESTURE_MMB | GESTURE_CTRL, on_zoom);
    goxel_add_gesture(GESTURE_DRAG, GESTURE_MMB, on_rotate);
    goxel_add_gesture(GESTURE_HOVER, 0, on_hover);

    goxel_reset();
}

void goxel_reset(void)
{
    image_delete(goxel.image);
    goxel.image = image_new();
    settings_load();

    // Put plane horizontal at the origin.
    plane_from_vectors(goxel.plane,
            VEC(0, 0, 0), VEC(1, 0, 0), VEC(0, 1, 0));

    vec4_set(goxel.back_color, 70, 70, 70, 255);
    vec4_set(goxel.grid_color, 255, 255, 255, 127);
    vec4_set(goxel.image_box_color, 204, 204, 255, 255);

    action_exec2(ACTION_tool_set_brush);
    goxel.tool_radius = 0.5;
    goxel.painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_OVER,
        .smoothness = 0,
        .color = {255, 255, 255, 255},
    };

    // Set symmetry origin to the center of the image.
    mat4_mul_vec3(goxel.image->box, VEC(0, 0, 0),
                  goxel.painter.symmetry_origin);

    goxel.rend.light = (typeof(goxel.rend.light)) {
        .pitch = 20 * DD2R,
        .yaw = 120 * DD2R,
        .intensity = 2.0,
    };
    goxel.rend.settings = (render_settings_t) {
        .occlusion_strength = 0.4,
        .ambient = 0.3,
        .shadow = 0.3,
    };
    if (DEFINED(NO_SHADOW))
        goxel.rend.settings.shadow = 0;

    goxel.snap_mask = SNAP_MESH | SNAP_IMAGE_BOX;

    goxel.pathtracer = (pathtracer_t) {
        .num_samples = 512,
        .world = {
            .type = PT_WORLD_UNIFORM,
            .energy = 1,
            .color = {127, 127, 127, 255}
        },
        .floor = {
            .color = {157, 172, 157, 255},
            .size = {64, 64},
        },
    };

    #ifdef AFTER_RESET_FUNC
    AFTER_RESET_FUNC();
    #endif
}

void goxel_release(void)
{
    pathtracer_stop(&goxel.pathtracer);
    gui_release();
}

/*
 * Function: goxel_create_graphics
 * Called after the graphics context has been created.
 */
void goxel_create_graphics(void)
{
    render_init();
    goxel.graphics_initialized = true;
}

/*
 * Function: goxel_release_graphics
 * Called before the graphics context gets destroyed.
 */
void goxel_release_graphics(void)
{
    render_deinit();
    model3d_release_graphics();
    gui_release_graphics();
    shaders_release_all();
    texture_delete(goxel.pick_fbo);
    goxel.pick_fbo = NULL;
    goxel.graphics_initialized = false;
}

static void update_window_title(void)
{
    char buf[1024];
    sprintf(buf, "Goxel %s%s %s", GOXEL_VERSION_STR, DEBUG ? " (debug)" : "",
            goxel.image->path ?: "");
    sys_set_window_title(buf);
}

KEEPALIVE
int goxel_iter(inputs_t *inputs)
{
    double time = sys_get_time();
    uint64_t mesh_key;
    float pitch;
    camera_t *camera = get_camera();

    if (!goxel.graphics_initialized)
        goxel_create_graphics();

    goxel.delta_time = time - goxel.frame_time;
    goxel.fps = mix(goxel.fps, 1.0 / goxel.delta_time, 0.1);
    goxel.frame_time = time;
    goxel_set_help_text(NULL);
    goxel_set_hint_text(NULL);
    goxel.screen_size[0] = inputs->window_size[0];
    goxel.screen_size[1] = inputs->window_size[1];
    goxel.screen_scale = inputs->scale;
    goxel.rend.fbo = inputs->framebuffer;
    goxel.rend.scale = inputs->scale;
    camera_update(camera);
    mat4_copy(camera->view_mat, goxel.rend.view_mat);
    mat4_copy(camera->proj_mat, goxel.rend.proj_mat);
    gui_iter(inputs);

    if (DEFINED(SOUND) && time - goxel.last_click_time > 0.1) {
        mesh_key = mesh_get_key(goxel_get_render_mesh(goxel.image));
        if (goxel.last_mesh_key != mesh_key) {
            if (goxel.last_mesh_key) {
                pitch = goxel.painter.mode == MODE_OVER ? 1.0 :
                        goxel.painter.mode == MODE_SUB ? 0.8 : 1.2;
                sound_play("build", 0.2, pitch);
                goxel.last_click_time = time;
            }
            goxel.last_mesh_key = mesh_key;
        }
    }

    sound_iter();
    update_window_title();

    goxel.frame_count++;

    if (goxel.request_test_graphic_release) {
        goxel_release_graphics();
        goxel_create_graphics();
        goxel.request_test_graphic_release = false;
    }

    return goxel.quit ? 1 : 0;
}

static void set_cursor_hint(cursor_t *curs)
{
    const char *snap_str = NULL;
    if (!curs->snaped) {
        goxel_set_hint_text(NULL);
        return;
    }
    if (curs->snaped == SNAP_MESH) snap_str = "mesh";
    if (curs->snaped == SNAP_PLANE) snap_str = "plane";
    if (curs->snaped == SNAP_IMAGE_BOX) snap_str = "bounding box";
    if (    curs->snaped == SNAP_SELECTION_IN ||
            curs->snaped == SNAP_SELECTION_OUT) snap_str = "selection";

    goxel_set_hint_text("[%.0f %.0f %.0f] (%s)",
            curs->pos[0] - 0.5, curs->pos[1] - 0.5, curs->pos[2] - 0.5,
            snap_str);
}

static int on_drag(const gesture_t *gest, void *user)
{
    cursor_t *c = &goxel.cursor;
    if (gest->state == GESTURE_BEGIN)
        c->flags |= CURSOR_PRESSED;
    if (gest->state == GESTURE_END)
        c->flags &= ~CURSOR_PRESSED;

    c->snaped = goxel_unproject(
            gest->viewport, gest->pos, c->snap_mask,
            c->snap_offset, c->pos, c->normal);

    // Set some default values.  The tools can override them.
    // XXX: would be better to reset the cursor when we change tool!
    c->snap_mask = goxel.snap_mask;
    set_flag(&c->snap_mask, SNAP_ROUNDED, goxel.painter.smoothness == 0);
    c->snap_offset = 0;
    return 0;
}

// XXX: can we merge this with unproject?
static bool unproject_delta(const float win[3], const float model[4][4],
                            const float proj[4][4], const float viewport[4],
                            float out[3])
{
    float inv[4][4], norm_pos[4];

    mat4_mul(proj, model, inv);
    if (!mat4_invert(inv, inv)) {
        vec3_copy(vec3_zero, out);
        return false;
    }
    vec4_set(norm_pos, win[0] / viewport[2], win[1] / viewport[3], 0, 0);
    mat4_mul_vec4(inv, norm_pos, norm_pos);
    vec3_copy(norm_pos, out);
    return true;
}

static int on_pan(const gesture_t *gest, void *user)
{
    camera_t *camera = get_camera();
    if (gest->state == GESTURE_BEGIN) {
        mat4_copy(camera->mat, goxel.move_origin.camera_mat);
        vec2_copy(gest->pos, goxel.move_origin.pos);
    }
    float wpos[3] = {gest->pos[0], gest->pos[1], 0};
    float worigin_pos[3], wdelta[3], odelta[3];
    vec3_set(worigin_pos, goxel.move_origin.pos[0],
                          goxel.move_origin.pos[1], 0);
    vec3_sub(wpos, worigin_pos, wdelta);

    unproject_delta(wdelta, mat4_identity,
                    camera->proj_mat, gest->viewport, odelta);
    vec3_imul(odelta, 2); // XXX: why do I need that?
    if (!camera->ortho)
        vec3_imul(odelta, camera->dist);
    mat4_translate(goxel.move_origin.camera_mat, -odelta[0], -odelta[1], 0,
                   camera->mat);
    return 0;
}

static int on_rotate(const gesture_t *gest, void *user)
{
    float x1, y1, x2, y2, x_rot, z_rot;
    camera_t *camera = get_camera();

    if (gest->state == GESTURE_BEGIN) {
        mat4_copy(camera->mat, goxel.move_origin.camera_mat);
        vec2_copy(gest->pos, goxel.move_origin.pos);
    }

    x1 = goxel.move_origin.pos[0] / gest->viewport[2];
    y1 = goxel.move_origin.pos[1] / gest->viewport[3];
    x2 = gest->pos[0] / gest->viewport[2];
    y2 = gest->pos[1] / gest->viewport[3];
    z_rot = (x1 - x2) * 2 * M_PI;
    x_rot = (y2 - y1) * 2 * M_PI;

    mat4_copy(goxel.move_origin.camera_mat, camera->mat);
    camera_turntable(camera, z_rot, x_rot);
    return 0;
}

static int on_zoom(const gesture_t *gest, void *user)
{
    float p[3], n[3];
    double zoom;
    camera_t *camera = get_camera();

    zoom = (gest->pos[1] - gest->last_pos[1]) / 10.0;
    mat4_itranslate(camera->mat, 0, 0,
            -camera->dist * (1 - pow(1.1, -zoom)));
    camera->dist *= pow(1.1, -zoom);
    // Auto adjust the camera rotation position.
    if (goxel_unproject_on_mesh(gest->viewport, gest->pos,
                                goxel_get_layers_mesh(goxel.image), p, n)) {
        camera_set_target(camera, p);
    }
    return 0;
}

static int on_hover(const gesture_t *gest, void *user)
{
    cursor_t *c = &goxel.cursor;
    c->snaped = goxel_unproject(gest->viewport, gest->pos, c->snap_mask,
                                c->snap_offset, c->pos, c->normal);
    set_cursor_hint(c);
    c->flags &= ~CURSOR_PRESSED;
    // Set some default values.  The tools can override them.
    // XXX: would be better to reset the cursor when we change tool!
    c->snap_mask = goxel.snap_mask;
    set_flag(&c->snap_mask, SNAP_ROUNDED, goxel.painter.smoothness == 0);
    c->snap_offset = 0;
    set_flag(&c->flags, CURSOR_OUT, gest->state == GESTURE_END);
    return 0;
}

// XXX: Cleanup this.
void goxel_mouse_in_view(const float viewport[4], const inputs_t *inputs,
                         bool capture_keys)
{
    float p[3], n[3];
    camera_t *camera = get_camera();

    painter_t painter = goxel.painter;
    gesture_update(goxel.gestures_count, goxel.gestures,
                   inputs, viewport, NULL);
    set_flag(&goxel.cursor.flags, CURSOR_SHIFT, inputs->keys[KEY_LEFT_SHIFT]);
    set_flag(&goxel.cursor.flags, CURSOR_CTRL, inputs->keys[KEY_CONTROL]);

    // Need to set the cursor snap mask to default because the tool might
    // change it.
    goxel.cursor.snap_mask = goxel.snap_mask;
    painter.box = !box_is_null(goxel.image->active_layer->box) ?
                         &goxel.image->active_layer->box :
                         !box_is_null(goxel.image->box) ?
                         &goxel.image->box : NULL;
    // Only paint mode support alpha.
    if (painter.mode != MODE_PAINT) painter.color[3] = 255;
    // Swap OVER/SUB modes.
    if (inputs->keys[' ']) {
        if (goxel.painter.mode == MODE_SUB) painter.mode = MODE_OVER;
        if (goxel.painter.mode == MODE_OVER) painter.mode = MODE_SUB;
    }
    // Only apply smoothness for paint mode, that is we don't support
    // semi 'transparent' voxels anymore.
    if (painter.mode != MODE_PAINT) painter.smoothness = 0;

    if (!goxel.no_edit) {
        tool_iter(goxel.tool, &painter, viewport);
    }

    if (inputs->mouse_wheel) {
        mat4_itranslate(camera->mat, 0, 0,
                -camera->dist * (1 - pow(1.1, -inputs->mouse_wheel)));
        camera->dist *= pow(1.1, -inputs->mouse_wheel);
        // Auto adjust the camera rotation position.
        if (goxel_unproject_on_mesh(viewport, inputs->touches[0].pos,
                                goxel_get_layers_mesh(goxel.image), p, n)) {
            camera_set_target(camera, p);
        }
        return;
    }

    // handle keyboard rotations
    if (!capture_keys) return;

    if (inputs->keys[KEY_LEFT]) {
        camera_turntable(camera, +0.05, 0);
    }
    if (inputs->keys[KEY_RIGHT]) {
        camera_turntable(camera, -0.05, 0);
    }
    if (inputs->keys[KEY_UP]) {
        camera_turntable(camera, 0, +0.05);
    }
    if (inputs->keys[KEY_DOWN]) {
        camera_turntable(camera, 0, -0.05);
    }

    // C: recenter the view:
    // XXX: this should be an action!
    if (inputs->keys['C']) {
        if (goxel_unproject_on_mesh(viewport, inputs->touches[0].pos,
                                goxel_get_layers_mesh(goxel.image), p, n)) {
            camera_set_target(camera, p);
        }
    }
}

KEEPALIVE
void goxel_render(void)
{
    uint8_t color[4];
    theme_get_color(THEME_GROUP_BASE, THEME_COLOR_BACKGROUND, false, color);
    GL(glViewport(0, 0, goxel.screen_size[0] * goxel.screen_scale,
                        goxel.screen_size[1] * goxel.screen_scale));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, goxel.rend.fbo));
    GL(glClearColor(color[0] / 255., color[1] / 255., color[2] / 255., 1));
    GL(glStencilMask(0xFF));
    GL(glDisable(GL_SCISSOR_TEST));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
               GL_STENCIL_BUFFER_BIT));
    gui_render();
}

static void render_export_viewport(const float viewport[4])
{
    // Render the export viewport.
    int w = goxel.image->export_width;
    int h = goxel.image->export_height;
    float aspect = (float)w/h;
    float plane[4][4];
    camera_t *camera = get_camera();

    mat4_set_identity(plane);
    mat4_iscale(plane, viewport[2], viewport[3], 1);
    mat4_itranslate(plane, 0.5, 0.5, 0);
    if (aspect < camera->aspect) {
        mat4_iscale(plane, aspect / camera->aspect, 1, 1);
    } else {
        mat4_iscale(plane, 1, camera->aspect / aspect, 1);
    }
    render_rect(&goxel.rend, plane, EFFECT_STRIP);
}

static void render_pathtrace_view(const float viewport[4])
{
    pathtracer_t *pt = &goxel.pathtracer;
    float a, mat[4][4];
    // Recreate the buffer if needed.
    if (    !pt->buf ||
            pt->w != goxel.image->export_width ||
            pt->h != goxel.image->export_height) {
        free(pt->buf);
        pt->w = goxel.image->export_width;
        pt->h = goxel.image->export_height;
        pt->buf = calloc(pt->w * pt->h, 4);
        texture_delete(pt->texture);
        pt->texture = texture_new_surface(pt->w, pt->h, 0);
    }
    pathtracer_iter(pt, viewport);

    // Render the buffer.
    mat4_set_identity(mat);
    a = 1.0 * pt->w / pt->h / viewport[2] * viewport[3];
    mat4_iscale(mat, viewport[2], viewport[3], 1);
    mat4_itranslate(mat, 0.5, 0.5, 0);
    mat4_iscale(mat, min(a, 1.f), min(1.f / a, 1.f), 1);
    texture_set_data(pt->texture, pt->buf, pt->w, pt->h, 4);
    render_img(&goxel.rend, pt->texture, mat,
               EFFECT_NO_SHADING | EFFECT_PROJ_SCREEN | EFFECT_ANTIALIASING);
    render_submit(&goxel.rend, viewport, goxel.back_color);
}

static void render_axis_arrows(const float viewport[4])
{
    float rot[4][4], a[3], b[3], l[3];
    const float AXIS[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    uint8_t color[4];
    int i;
    const float size = 40;

    mat4_copy(get_camera()->mat, rot);
    vec3_set(rot[3], 0, 0, 0);
    mat4_invert(rot, rot);

    vec3_set(a, 50, 50, 0); // Origin pos (viewport coordinates)
    for (i = 0; i < 3; i++) {
        vec4_set(color, AXIS[i][0] * 255,
                        AXIS[i][1] * 255,
                        AXIS[i][2] * 255, 255);
        vec3_mul(AXIS[i], size, b);
        mat4_mul_vec3(rot, b, b);
        vec3_add(a, b, b);
        render_line(&goxel.rend, a, b, color, EFFECT_PROJ_SCREEN);

        vec3_mul(AXIS[i], size * 0.4, l);
        mat4_mul_vec3(rot, l, l);
        vec3_add(b, l, l);

        // X letter
        if (i == 0) {
            float t1[3], t2[3];

            vec3_set(t1, l[0] - 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0] + 3, l[1] - 7, l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);

            vec3_set(t1, l[0] + 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0] - 3, l[1] - 7, l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);
        }

        // Y letter
        if (i == 1) {
            float t1[3], t2[3];

            vec3_set(t1, l[0] - 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0], l[1], l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);

            vec3_set(t1, l[0] + 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0], l[1], l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);

            vec3_set(t1, l[0], l[1] - 7, l[2]);
            vec3_set(t2, l[0], l[1], l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);
        }

        // Z letter
        if (i == 2) {
            float t1[3], t2[3];

            vec3_set(t1, l[0] - 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0] + 3, l[1] + 7, l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);

            vec3_set(t1, l[0] + 3, l[1] + 7, l[2]);
            vec3_set(t2, l[0] - 3, l[1] - 7, l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);

            vec3_set(t1, l[0] - 3, l[1] - 7, l[2]);
            vec3_set(t2, l[0] + 3, l[1] - 7, l[2]);
            render_line(&goxel.rend, t1, t2, color, EFFECT_PROJ_SCREEN);
        }
    }
}

static bool is_box_face_visible(const float box[4][4], int f)
{
    float mat[4][4], n[4];
    camera_t *cam = get_camera();

    mat4_mul(box, FACES_MATS[f], mat);
    mat4_mul_vec4(cam->view_mat, mat[2], n);
    return (n[2] < 0);
}


static void render_symmetry_axis(
        const float box[4][4], int sym, const float sym_o[3])
{
    int i, f;
    float plane[4][4], vertices[8][3], triangles[2][3][3], seg[2][3], n[3];
    uint8_t color[4];
    box_get_vertices(box, vertices);

    for (i = 0; i < 3; i++) {
        if (!(sym & (1 << i))) continue;
        memset(n, 0, sizeof(n));
        n[i] = 1;
        plane_from_normal(plane, sym_o, n);
        memset(color, 0, sizeof(color));
        color[i] = 255;
        color[3] = 255;
        for (f = 0; f < 6; f++) {
            if (!is_box_face_visible(box, f)) continue;
            vec3_copy(vertices[FACES_VERTICES[f][0]], triangles[0][0]);
            vec3_copy(vertices[FACES_VERTICES[f][1]], triangles[0][1]);
            vec3_copy(vertices[FACES_VERTICES[f][2]], triangles[0][2]);
            vec3_copy(vertices[FACES_VERTICES[f][2]], triangles[1][0]);
            vec3_copy(vertices[FACES_VERTICES[f][3]], triangles[1][1]);
            vec3_copy(vertices[FACES_VERTICES[f][0]], triangles[1][2]);
            if (plane_triangle_intersection(plane, triangles[0], seg))
                render_line(&goxel.rend, seg[0], seg[1], color, 0);
            if (plane_triangle_intersection(plane, triangles[1], seg))
                render_line(&goxel.rend, seg[0], seg[1], color, 0);
        }
    }
}

void goxel_render_view(const float viewport[4], bool render_mode)
{
    const layer_t *layer;
    renderer_t *rend = &goxel.rend;
    const uint8_t layer_box_color[4] = {128, 128, 255, 255};
    int effects = 0;
    camera_t *camera = get_camera();

    if (render_mode) {
        render_pathtrace_view(viewport);
        return;
    }

    camera->aspect = viewport[2] / viewport[3];
    camera_update(camera);
    mat4_copy(camera->view_mat, goxel.rend.view_mat);
    mat4_copy(camera->proj_mat, goxel.rend.proj_mat);

    effects |= goxel.view_effects;

    for (layer = goxel_get_render_layers(true); layer; layer = layer->next) {
        if (layer->visible && layer->mesh)
            render_mesh(rend, layer->mesh, layer->material, effects);
    }

    if (!box_is_null(goxel.image->active_layer->box))
        render_box(rend, goxel.image->active_layer->box,
                   layer_box_color, EFFECT_WIREFRAME);

    // Render all the image layers.
    DL_FOREACH(goxel.image->layers, layer) {
        if (layer->visible && layer->image)
            render_img(rend, layer->image, layer->mat, EFFECT_NO_SHADING);
    }

    render_box(rend, goxel.selection, NULL, EFFECT_STRIP | EFFECT_WIREFRAME);

    // Debug: show the current layer mesh blocks.
    if ((0)) {
        mesh_iterator_t iter;
        mesh_t *mesh = goxel.image->active_layer->mesh;
        iter = mesh_get_iterator(mesh, MESH_ITER_BLOCKS);
        int p[3], aabb[2][3];
        float box[4][4];
        while (mesh_iter(&iter, p)) {
            mesh_get_block_aabb(p, aabb);
            bbox_from_aabb(box, aabb);
            render_box(rend, box, NULL, EFFECT_WIREFRAME);
        }
    }

    // XXX: make a toggle for debug informations.
    if ((0)) {
        float b[4][4];
        uint8_t c[4];
        vec4_set(c, 0, 255, 0, 80);
        mesh_get_box(goxel_get_layers_mesh(goxel.image), true, b);
        render_box(rend, b, c, EFFECT_WIREFRAME);
        vec4_set(c, 0, 255, 255, 80);
        mesh_get_box(goxel_get_layers_mesh(goxel.image), false, b);
        render_box(rend, b, c, EFFECT_WIREFRAME);
    }
    if (goxel.snap_mask & SNAP_PLANE)
        render_grid(rend, goxel.plane, goxel.grid_color, goxel.image->box);

    if (!box_is_null(goxel.image->box) && !goxel.hide_box) {
        render_box(rend, goxel.image->box, goxel.image_box_color,
                   EFFECT_SEE_BACK | EFFECT_GRID);
        render_symmetry_axis(goxel.image->box, goxel.painter.symmetry,
                             goxel.painter.symmetry_origin);
    }
    if (goxel.show_export_viewport)
        render_export_viewport(viewport);

    render_axis_arrows(viewport);
    render_submit(&goxel.rend, viewport, goxel.back_color);
}

void image_update(image_t *img);

const mesh_t *goxel_get_layers_mesh(const image_t *img)
{
    uint32_t key = 0, k;
    layer_t *layer;

    image_update((image_t*)img);
    DL_FOREACH(img->layers, layer) {
        if (!layer->visible) continue;
        if (!layer->mesh) continue;
        k = layer_get_key(layer);
        key = XXH32(&k, sizeof(k), key);
    }
    if (key != goxel.layers_mesh_hash) {
        goxel.layers_mesh_hash = key;
        if (!goxel.layers_mesh_) goxel.layers_mesh_ = mesh_new();
        mesh_clear(goxel.layers_mesh_);
        DL_FOREACH(img->layers, layer) {
            if (!layer->visible) continue;
            mesh_merge(goxel.layers_mesh_, layer->mesh, MODE_OVER, NULL);
        }
    }
    return goxel.layers_mesh_;
}

const mesh_t *goxel_get_render_mesh(const image_t *img)
{
    uint32_t key, k;
    const mesh_t *mesh;
    layer_t *layer;

    if (!goxel.tool_mesh)
        return goxel_get_layers_mesh(img);

    key = mesh_get_key(goxel_get_layers_mesh(img));
    k = mesh_get_key(goxel.tool_mesh);
    key = XXH32(&k, sizeof(k), key);
    if (key != goxel.render_mesh_hash) {
        image_update(goxel.image);
        goxel.render_mesh_hash = key;
        if (!goxel.render_mesh_) goxel.render_mesh_ = mesh_new();
        mesh_clear(goxel.render_mesh_);
        DL_FOREACH(goxel.image->layers, layer) {
            if (!layer->visible) continue;
            mesh = layer->mesh;
            if (mesh == goxel.image->active_layer->mesh)
                mesh = goxel.tool_mesh;
            mesh_merge(goxel.render_mesh_, mesh, MODE_OVER, NULL);
        }
    }
    return goxel.render_mesh_;
}

const layer_t *goxel_get_render_layers(bool with_tool_preview)
{
    uint32_t hash, k;
    layer_t *l, *layer, *tmp;

    hash = image_get_key(goxel.image);
    if (with_tool_preview && goxel.tool_mesh) {
        k = mesh_get_key(goxel.tool_mesh);
        hash = XXH32(&k, sizeof(k), hash);
    }

    if (hash != goxel.render_layers_hash) {
        goxel.render_layers_hash = hash;
        image_update(goxel.image);

        DL_FOREACH_SAFE(goxel.render_layers, layer, tmp) {
            DL_DELETE(goxel.render_layers, layer);
            layer_delete(layer);
        }

        DL_FOREACH(goxel.image->layers, l) {
            if (!l->visible) continue;
            if (!l->mesh) continue;
            layer = layer_copy(l);
            if (    with_tool_preview && goxel.tool_mesh &&
                    l->mesh == goxel.image->active_layer->mesh)
            {
                mesh_set(layer->mesh, goxel.tool_mesh);
            }

            if (    goxel.render_layers &&
                    goxel.render_layers->prev->material == layer->material)
            {
                mesh_merge(goxel.render_layers->prev->mesh, layer->mesh,
                           MODE_OVER, NULL);
                layer_delete(layer);
            } else {
                DL_APPEND(goxel.render_layers, layer);
            }
        }
    }
    return goxel.render_layers;
}

// Render the view into an RGB[A] buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h, int bpp)
{
    camera_t *camera = get_camera();
    const mesh_t *mesh;
    texture_t *fbo;
    renderer_t rend = goxel.rend;
    float rect[4] = {0, 0, w * 2, h * 2};
    uint8_t *tmp_buf;

    camera->aspect = (float)w / h;
    camera_update(camera);

    mesh = goxel_get_layers_mesh(goxel.image);
    fbo = texture_new_buffer(w * 2, h * 2, TF_DEPTH);

    mat4_copy(camera->view_mat, rend.view_mat);
    mat4_copy(camera->proj_mat, rend.proj_mat);
    rend.fbo = fbo->framebuffer;
    rend.scale = 1.0;

    // XXX: use goxel_get_render_layers!
    render_mesh(&rend, mesh, NULL, 0);
    render_submit(&rend, rect, (bpp == 3) ? goxel.back_color : NULL);
    tmp_buf = calloc(w * h * 4, bpp);
    texture_get_data(fbo, w * 2, h * 2, bpp, tmp_buf);
    img_downsample(tmp_buf, w * 2, h * 2, bpp, buf);
    free(tmp_buf);
    texture_delete(fbo);
}

// XXX: we could merge all the set_xxx_text function into a single one.
void goxel_set_help_text(const char *msg, ...)
{
    va_list args;
    free(goxel.help_text);
    goxel.help_text = NULL;
    if (!msg) return;
    va_start(args, msg);
    vasprintf(&goxel.help_text, msg, args);
    va_end(args);
}

void goxel_set_hint_text(const char *msg, ...)
{
    va_list args;
    free(goxel.hint_text);
    goxel.hint_text = NULL;
    if (!msg) return;
    va_start(args, msg);
    vasprintf(&goxel.hint_text, msg, args);
    va_end(args);
}

void goxel_import_image_plane(const char *path)
{
    layer_t *layer;
    texture_t *tex;
    tex = texture_new_image(path, TF_NEAREST);
    if (!tex) return;
    image_history_push(goxel.image);
    layer = image_add_layer(goxel.image, NULL);
    sprintf(layer->name, "img");
    layer->image = tex;
    mat4_iscale(layer->mat, layer->image->w, layer->image->h, 1);
}

void goxel_on_low_memory(void)
{
    render_on_low_memory(&goxel.rend);
}

int goxel_import_file(const char *path, const char *format)
{
    const file_format_t *f;
    int err;

    if (str_endswith(path, ".gox")) {
        return load_from_file(path);
    }
    f = file_format_for_path(path, format, "r");
    if (!f) return -1;
    if (!path) {
        path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, f->ext, NULL, NULL);
        if (!path) return -1;
    }
    err = f->import_func(goxel.image, path);
    if (err) return err;
    return 0;
}

int goxel_export_to_file(const char *path, const char *format)
{
    const file_format_t *f;
    char name[128];
    int err;
    f = file_format_for_path(path, format, "w");
    if (!f) return -1;
    if (!path) {
        snprintf(name, sizeof(name), "Untitled%s", f->ext + strlen(f->ext) + 2);
        path = sys_get_save_path(f->ext, name);
        if (!path) return -1;
    }
    err = f->export_func(goxel.image, path);
    if (err) return err;
    sys_on_saved(path);
    return 0;
}

static void a_cut_as_new_layer(void)
{
    layer_t *new_layer;
    painter_t painter;

    image_t *img = goxel.image;
    layer_t *layer = img->active_layer;
    const float (*box)[4][4] = &goxel.selection;

    new_layer = image_duplicate_layer(img, layer);
    painter = (painter_t) {
        .shape = &shape_cube,
        .mode = MODE_INTERSECT,
        .color = {255, 255, 255, 255},
    };
    mesh_op(new_layer->mesh, &painter, *box);
    painter.mode = MODE_SUB;
    mesh_op(layer->mesh, &painter, *box);
}

ACTION_REGISTER(cut_as_new_layer,
    .help = "Cut into a new layer",
    .cfunc = a_cut_as_new_layer,
    .flags = ACTION_TOUCH_IMAGE,
)

static void a_reset_selection(void)
{
    mat4_copy(mat4_zero, goxel.selection);
}

ACTION_REGISTER(reset_selection,
    .help = "Reset the selection",
    .cfunc = a_reset_selection,
)

static void a_fill_selection(void)
{
    layer_t *layer = goxel.image->active_layer;
    if (box_is_null(goxel.selection)) return;
    mesh_op(layer->mesh, &goxel.painter, goxel.selection);
}

ACTION_REGISTER(fill_selection,
    .help = "Fill the selection with the current paint settings",
    .cfunc = a_fill_selection,
    .flags = ACTION_TOUCH_IMAGE,
)

static void copy_action(void)
{
    painter_t painter;
    mesh_delete(goxel.clipboard.mesh);
    mat4_copy(goxel.selection, goxel.clipboard.box);
    goxel.clipboard.mesh = mesh_copy(goxel.image->active_layer->mesh);
    if (!box_is_null(goxel.selection)) {
        painter = (painter_t) {
            .shape = &shape_cube,
            .mode = MODE_INTERSECT,
            .color = {255, 255, 255, 255},
        };
        mesh_op(goxel.clipboard.mesh, &painter, goxel.selection);
    }
}

static void past_action(void)
{
    mesh_t *mesh = goxel.image->active_layer->mesh;
    mesh_t *tmp;
    float p1[3], p2[3], mat[4][4];

    mat4_set_identity(mat);
    if (!goxel.clipboard.mesh) return;

    tmp = mesh_copy(goxel.clipboard.mesh);
    if (    !box_is_null(goxel.selection) &&
            !box_is_null(goxel.clipboard.box)) {
        vec3_copy(goxel.selection[3], p1);
        vec3_copy(goxel.clipboard.box[3], p2);
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
    .default_shortcut = "Ctrl C",
    .flags = 0,
)

ACTION_REGISTER(past,
    .help = "Past",
    .cfunc = past_action,
    .default_shortcut = "Ctrl V",
    .flags = ACTION_TOUCH_IMAGE,
)

#define HS2 (M_SQRT2 / 2.0)


static void a_view_default(void)
{
    camera_t *camera = get_camera();
    float dist = vec3_norm(camera->mat[3]);
    mat4_set_identity(camera->mat);
    mat4_irotate(camera->mat, M_PI / 4, 1, 0, 0);
    mat4_itranslate(camera->mat, 0, 0, dist);
}

static void a_view_set(void *data)
{
    float rz = ((float*)data)[0] / 180 * M_PI;
    float rx = ((float*)data)[1] / 180 * M_PI;
    camera_t *camera = get_camera();

    mat4_set_identity(camera->mat);
    mat4_itranslate(camera->mat, 0, 0, camera->dist);
    camera_turntable(camera, rz, rx);
}

ACTION_REGISTER(view_left,
    .help = "Set camera view to left",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc_data = a_view_set,
    .data = (float[]){90, 90},
    .default_shortcut = "Ctrl 3",
)

ACTION_REGISTER(view_right,
    .help = "Set camera view to right",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc_data = a_view_set,
    .data = (float[]){-90, 90},
    .default_shortcut = "3",
)

ACTION_REGISTER(view_top,
    .help = "Set camera view to top",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc_data = a_view_set,
    .data = (float[]){0, 0},
    .default_shortcut = "7",
)

ACTION_REGISTER(view_default,
    .help = "Set camera view to default",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc = a_view_default,
    .default_shortcut = "5",
)

ACTION_REGISTER(view_front,
    .help = "Set camera view to front",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc_data = a_view_set,
    .data = (float[]){0, 90},
    .default_shortcut = "1",
)

static void quit(void)
{
    gui_query_quit();
}
ACTION_REGISTER(quit,
    .help = "Quit the application",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc = quit,
    .default_shortcut = "Ctrl Q",
)

static void undo(void) { image_undo(goxel.image); }
static void redo(void) { image_redo(goxel.image); }

ACTION_REGISTER(undo,
    .help = "Undo",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc = undo,
    .default_shortcut = "Ctrl Z",
    .icon = ICON_ARROW_BACK,
)

ACTION_REGISTER(redo,
    .help = "Redo",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc = redo,
    .default_shortcut = "Ctrl Y",
    .icon = ICON_ARROW_FORWARD,
)

static void toggle_mode(void)
{
    int mode = goxel.painter.mode;
    switch (mode) {
    case MODE_OVER:     mode = MODE_SUB; break;
    case MODE_SUB:      mode = MODE_PAINT; break;
    case MODE_PAINT:    mode = MODE_OVER; break;
    }
    goxel.painter.mode = mode;
}

ACTION_REGISTER(toggle_mode,
    .help = "Toggle the tool mode (add, sub, paint)",
    .flags = ACTION_CAN_EDIT_SHORTCUT,
    .cfunc = toggle_mode,
)
