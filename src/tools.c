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

enum {
    STATE_IDLE = 0,
    STATE_CANCEL,
    STATE_SNAPED,
    STATE_PAINT,
    STATE_PAINT2,
    STATE_WAIT_UP,
    STATE_WAIT_KEY_UP,
    // For selection tool:
    STATE_SNAPED_FACE,
    STATE_MOVE_FACE,
};

static box_t get_box(const vec3_t *p0, const vec3_t *p1, const vec3_t *n,
                     float r, const plane_t *plane)
{
    mat4_t rot;
    box_t box;
    if (p1 == NULL) {
        box = bbox_from_extents(*p0, r, r, r);
        box = box_swap_axis(box, 2, 0, 1);
        return box;
    }
    if (r == 0) {
        box = bbox_grow(bbox_from_points(*p0, *p1), 0.5, 0.5, 0.5);
        // Apply the plane rotation.
        rot = plane->mat;
        rot.vecs[3] = vec4(0, 0, 0, 1);
        mat4_imul(&box.mat, rot);
        return box;
    }

    // Create a box for a line:
    int i;
    const vec3_t AXES[] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};

    box.mat = mat4_identity;
    box.p = vec3_mix(*p0, *p1, 0.5);
    box.d = vec3_sub(*p1, box.p);
    for (i = 0; i < 3; i++) {
        box.w = vec3_cross(box.d, AXES[i]);
        if (vec3_norm2(box.w) > 0) break;
    }
    if (i == 3) return box;
    box.w = vec3_mul(vec3_normalized(box.w), r);
    box.h = vec3_mul(vec3_normalized(vec3_cross(box.d, box.w)), r);
    return box;
}

static bool check_can_skip(goxel_t *goxel, vec3_t pos, bool pressed, int op)
{
    if (    pressed == goxel->tool_last_op.pressed &&
            op == goxel->tool_last_op.op &&
            vec3_equal(pos, goxel->tool_last_op.pos))
        return true;
    goxel->tool_last_op.pressed = pressed;
    goxel->tool_last_op.op = op;
    goxel->tool_last_op.pos = pos;
    return false;
}

static void set_snap_hint(goxel_t *goxel, int snap)
{
    if (snap == SNAP_MESH)
        goxel_set_hint_text(goxel, "[Snapped to mesh]");
    if (snap == SNAP_PLANE)
        goxel_set_hint_text(goxel, "[Snapped to plane]");
    if (snap == SNAP_SELECTION_IN || snap == SNAP_SELECTION_OUT)
        goxel_set_hint_text(goxel, "[Snapped to selection]");
}

static int tool_shape_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                          const vec2_t *view_size, bool inside)
{
    const bool down = inputs->mouse_down[0];
    const bool up = !down;
    int snaped = 0;
    vec3_t pos, normal;
    box_t box;
    uvec4b_t box_color = HEXCOLOR(0xffff00ff);
    mesh_t *mesh = goxel->image->active_layer->mesh;

    if (inside)
        snaped = goxel_unproject(goxel, view_size, &inputs->mouse_pos,
                                 &pos, &normal);
    set_snap_hint(goxel, snaped);
    if (snaped) {
        if (    snaped == SNAP_MESH && goxel->painter.op == OP_ADD &&
                !goxel->snap_offset)
            vec3_iadd(&pos, normal);
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }
    if (state == STATE_IDLE) {
        goxel->tool_t = 0;
        if (snaped) state = STATE_SNAPED;
    }
    if (state == STATE_SNAPED) {
        if (goxel->tool_t == 0) {
            goxel->tool_t = 1;
            mesh_set(&goxel->tool_origin_mesh, mesh);
        }
        if (!snaped) return STATE_CANCEL;
        goxel_set_help_text(goxel, "Click and drag to draw.");
        goxel->tool_start_pos = pos;
        box = get_box(&goxel->tool_start_pos, &pos, &normal, 0,
                      &goxel->plane);
        mesh_set(&mesh, goxel->tool_origin_mesh);
        render_box(&goxel->rend, &box, false, &box_color, false);
        if (down) {
            state = STATE_PAINT;
            goxel->painting = true;
            image_history_push(goxel->image);
        }
    }
    if (state == STATE_PAINT) {
        goxel_set_help_text(goxel, "Drag.");
        if (!snaped || !inside) return state;
        box = get_box(&goxel->tool_start_pos, &pos, &normal, 0, &goxel->plane);
        render_box(&goxel->rend, &box, false, &box_color, false);
        mesh_set(&mesh, goxel->tool_origin_mesh);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, false);
        if (up) {
            state = STATE_PAINT2;
            goxel->tool_plane = plane_from_normal(pos, goxel->plane.u);

            if (!goxel->tool_shape_two_steps) {
                goxel_update_meshes(goxel, true);
                goxel->painting = false;
                state = STATE_IDLE;
            }
        }
    }
    if (state == STATE_PAINT2) {
        goxel_set_help_text(goxel, "Adjust height.");
        if (!snaped || !inside) return state;
        render_plane(&goxel->rend, &goxel->tool_plane, &goxel->grid_color);
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p),
                                 goxel->plane.n));
        box = get_box(&goxel->tool_start_pos, &pos, &normal, 0,
                      &goxel->plane);
        render_box(&goxel->rend, &box, false, &box_color, false);
        mesh_set(&mesh, goxel->tool_origin_mesh);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, false);
        if (down) {
            mesh_set(&mesh, goxel->tool_origin_mesh);
            mesh_op(mesh, &goxel->painter, &box);
            goxel_update_meshes(goxel, true);
            goxel->painting = false;
            return STATE_WAIT_UP;
        }
    }
    if (state == STATE_WAIT_UP) {
        goxel->tool_plane = plane_null;
        if (up) state = STATE_IDLE;
    }
    return state;
}

// XXX: this is very close to tool_shape_iter.
static int tool_selection_iter(goxel_t *goxel, const inputs_t *inputs,
                               int state, const vec2_t *view_size,
                               bool inside)
{
    extern const mat4_t FACES_MATS[6];
    const bool down = inputs->mouse_down[0];
    const bool up = !down;
    int snaped = 0;
    int face = -1;
    vec3_t pos, normal;
    plane_t face_plane;
    box_t box;
    uvec4b_t box_color = HEXCOLOR(0xffff00ff);

    // See if we can snap on a selection face.
    if (inside && !box_is_null(goxel->selection) &&
            IS_IN(state, STATE_IDLE, STATE_SNAPED, STATE_SNAPED_FACE)) {
        goxel->tool_snape_face = -1;
        if (goxel_unproject_on_box(goxel, view_size, &inputs->mouse_pos,
                               &goxel->selection, false,
                               &pos, &normal, &face)) {
            goxel->tool_snape_face = face;
            state = STATE_SNAPED_FACE;
        }
    }
    if (!box_is_null(goxel->selection) && goxel->tool_snape_face != -1)
        face_plane.mat = mat4_mul(goxel->selection.mat,
                                  FACES_MATS[goxel->tool_snape_face]);

    if (inside && face == -1)
        snaped = goxel_unproject(goxel, view_size, &inputs->mouse_pos,
                                 &pos, &normal);
    if (snaped) {
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }

    if (state == STATE_IDLE) {
        goxel->tool_t = 0;
        goxel->tool_snape_face = -1;
        if (snaped) state = STATE_SNAPED;
    }
    if (state == STATE_SNAPED) {
        if (!snaped) return STATE_CANCEL;
        goxel_set_help_text(goxel, "Click and drag to set selection.");
        goxel->tool_start_pos = pos;
        box = get_box(&goxel->tool_start_pos, &pos, &normal, 0,
                      &goxel->plane);
        render_box(&goxel->rend, &box, false, &box_color, false);
        if (down) {
            state = STATE_PAINT;
            goxel->painting = true;
        }
    }
    if (state == STATE_PAINT) {
        goxel_set_help_text(goxel, "Drag.");
        if (!snaped || !inside) return state;
        goxel->selection = get_box(&goxel->tool_start_pos, &pos, &normal, 0,
                                   &goxel->plane);
        if (up) {
            state = STATE_PAINT2;
            goxel->tool_plane = plane_from_normal(pos, goxel->plane.u);
        }
    }
    if (state == STATE_PAINT2) {
        goxel_set_help_text(goxel, "Adjust height.");
        if (!snaped || !inside) return state;
        render_plane(&goxel->rend, &goxel->tool_plane, &goxel->grid_color);
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p),
                                 goxel->plane.n));
        goxel->selection = get_box(&goxel->tool_start_pos, &pos, &normal, 0,
                                   &goxel->plane);
        if (down) {
            goxel->painting = false;
            return STATE_WAIT_UP;
        }
    }
    if (state == STATE_WAIT_UP) {
        goxel->tool_plane = plane_null;
        goxel->selection = box_get_bbox(goxel->selection);
        return up ? STATE_IDLE : STATE_WAIT_UP;
    }
    if (IS_IN(state, STATE_SNAPED_FACE, STATE_MOVE_FACE))
        goxel_set_help_text(goxel, "Drag to move face");
    if (state == STATE_SNAPED_FACE) {
        if (face == -1) return STATE_IDLE;
        render_img(&goxel->rend, NULL, &face_plane.mat);
        if (down) {
            state = STATE_MOVE_FACE;
            goxel->tool_plane = plane(pos, normal,
                                      vec3_normalized(face_plane.u));
        }
    }
    if (state == STATE_MOVE_FACE) {
        if (up) return STATE_IDLE;
        goxel_unproject_on_plane(goxel, view_size, &inputs->mouse_pos,
                                 &goxel->tool_plane, &pos, &normal);
        pos = vec3_add(goxel->tool_plane.p,
                    vec3_project(vec3_sub(pos, goxel->tool_plane.p),
                                 vec3_normalized(face_plane.n)));
        pos.x = round(pos.x);
        pos.y = round(pos.y);
        pos.z = round(pos.z);
        goxel->selection = box_move_face(goxel->selection,
                                         goxel->tool_snape_face, pos);
    }
    return state;
}

static int tool_brush_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                           const vec2_t *view_size, bool inside)
{
    const bool down = inputs->mouse_down[0];
    const bool pressed = down && !goxel->painting;
    const bool released = !down && goxel->painting;
    int snaped = 0;
    vec3_t pos, normal;
    box_t box;
    painter_t painter2;
    mesh_t *mesh = goxel->image->active_layer->mesh;

    if (inside)
        snaped = goxel_unproject(goxel, view_size, &inputs->mouse_pos,
                                 &pos, &normal);
    goxel_set_help_text(goxel, "Brush: use shift to draw lines, "
                               "ctrl to pick color");
    set_snap_hint(goxel, snaped);
    if (snaped) {
        if (    snaped == SNAP_MESH && goxel->painter.op == OP_ADD &&
                !goxel->snap_offset)
            vec3_iadd(&pos, normal);
        if (goxel->tool == TOOL_BRUSH && goxel->snap_offset)
            vec3_iaddk(&pos, normal, goxel->snap_offset * goxel->tool_radius);
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }
    if (state == STATE_IDLE) {
        goxel->tool_t = 0;
        if (snaped) state = STATE_SNAPED;
    }
    if (state == STATE_SNAPED) {
        if (goxel->tool_t == 0) {
            goxel->tool_t = 1;
            mesh_set(&goxel->tool_origin_mesh, mesh);
            mesh_set(&goxel->pick_mesh, goxel->layers_mesh);
            goxel->tool_last_op.op = 0; // Discard last op.
        }
        if (!snaped) return STATE_CANCEL;
        if (inputs->keys[KEY_SHIFT])
            render_line(&goxel->rend, &goxel->tool_start_pos, &pos, NULL);
        if (check_can_skip(goxel, pos, down, goxel->painter.op))
            return state;
        box = get_box(&pos, NULL, &normal, goxel->tool_radius, NULL);

        mesh_set(&mesh, goxel->tool_origin_mesh);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, false);

        if (inputs->keys[KEY_SHIFT]) {
            render_line(&goxel->rend, &goxel->tool_start_pos, &pos, NULL);
            if (pressed) {
                painter2 = goxel->painter;
                painter2.shape = &shape_cylinder;
                box = get_box(&goxel->tool_start_pos, &pos, &normal,
                              goxel->tool_radius, NULL);
                mesh_op(mesh, &painter2, &box);
                goxel_update_meshes(goxel, false);
                goxel->tool_start_pos = pos;
                mesh_set(&goxel->tool_origin_mesh, mesh);
            }
        }
        if (pressed) {
            mesh_set(&mesh, goxel->tool_origin_mesh);
            state = STATE_PAINT;
            goxel->tool_last_op.op = 0;
            goxel->painting = true;
            image_history_push(goxel->image);
        }
    }
    if (state == STATE_PAINT) {
        if (!snaped) return state;
        if (check_can_skip(goxel, pos, down, goxel->painter.op))
            return state;
        if (released) {
            goxel->painting = false;
            goxel->camera.target = pos;
            if (inputs->keys[KEY_SHIFT])
                return STATE_WAIT_KEY_UP;
            mesh_set(&goxel->pick_mesh, goxel->layers_mesh);
            return STATE_IDLE;
        }
        box = get_box(&pos, NULL, &normal, goxel->tool_radius, NULL);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, false);
        goxel->tool_start_pos = pos;
    }
    if (state == STATE_WAIT_KEY_UP) {
        goxel->tool_t = 0;
        if (!inputs->keys[KEY_SHIFT]) state = STATE_IDLE;
        if (snaped) state = STATE_SNAPED;
    }
    return state;
}

static int tool_laser_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                           const vec2_t *view_size, bool inside)
{
    vec3_t pos, normal;
    box_t box;
    painter_t painter = goxel->painter;
    mesh_t *mesh = goxel->image->active_layer->mesh;
    const bool down = inputs->mouse_down[0];
    // XXX: would be nice if we got the vec4_t view instead of view_size,
    // and why input->pos is not already in win pos?
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);
    vec2_t win = inputs->mouse_pos;
    win.y = view_size->y - win.y;

    painter.op = OP_SUB;
    painter.shape = &shape_cylinder;
    // Create the tool box from the camera along the visible ray.
    camera_get_ray(&goxel->camera, &win, &view, &pos, &normal);
    box.mat = mat4_identity;
    box.w = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(1, 0, 0, 0)).xyz;
    box.h = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(0, 1, 0, 0)).xyz;
    box.d = mat4_mul_vec(mat4_inverted(goxel->camera.view_mat),
                     vec4(0, 0, 1, 0)).xyz;
    box.d = vec3_neg(normal);
    box.p = pos;
    // Just a large value for the size of the laser box.
    mat4_itranslate(&box.mat, 0, 0, -1024);
    mat4_iscale(&box.mat, goxel->tool_radius, goxel->tool_radius, 1024);
    render_box(&goxel->rend, &box, false, NULL, false);
    if (state == STATE_IDLE) {
        if (down) {
            state = STATE_PAINT;
            image_history_push(goxel->image);
        }
    }
    if (state == STATE_PAINT) {
        if (!down) {
            return STATE_IDLE;
        }
        mesh_op(mesh, &painter, &box);
        goxel_update_meshes(goxel, false);
    }
    return state;
}

static int tool_set_plane_iter(goxel_t *goxel, const inputs_t *inputs,
                               int state, const vec2_t *view_size,
                               bool inside)
{
    bool snaped;
    vec3_t pos, normal;
    mesh_t *mesh = goxel->layers_mesh;
    const bool pressed = inputs->mouse_down[0];
    goxel_set_help_text(goxel, "Click on the mesh to set plane.");
    snaped = inside && goxel_unproject_on_mesh(goxel, view_size,
                            &inputs->mouse_pos, mesh, &pos, &normal);
    if (snaped && pressed) {
        vec3_iadd(&pos, normal);
        goxel->plane = plane_from_normal(pos, normal);
    }
    return 0;
}

static int tool_pick_color_iter(goxel_t *goxel, const inputs_t *inputs,
                                int state, const vec2_t *view_size,
                                bool inside)
{
    bool snaped;
    vec3_t pos, normal;
    uvec4b_t color;
    mesh_t *mesh = goxel->layers_mesh;
    const bool pressed = inputs->mouse_down[0];
    goxel_set_help_text(goxel, "Click on a voxel to pick the color");
    snaped = inside && goxel_unproject_on_mesh(goxel, view_size,
                            &inputs->mouse_pos, mesh, &pos, &normal);
    if (!snaped) return 0;
    color = mesh_get_at(mesh, &pos);
    color.a = 255;
    goxel_set_help_text(goxel, "%d %d %d", color.r, color.g, color.b);
    if (pressed) goxel->painter.color = color;
    return 0;
}

static int tool_dummy_iter(goxel_t *goxel, const inputs_t *inputs,
                           int state, const vec2_t *view_size,
                           bool inside)
{
    return 0;
}

static int tool_procedural_iter(goxel_t *goxel, const inputs_t *inputs,
                                int state, const vec2_t *view_size,
                                bool inside)
{
    int snaped = 0;
    vec3_t pos, normal;
    box_t box;
    gox_proc_t *proc = &goxel->proc;
    const bool down = inputs->mouse_down[0];

    if (proc->state == PROC_PARSE_ERROR) return 0;

    // XXX: duplicate code with tool_brush_iter.
    if (inside)
        snaped = goxel_unproject(goxel, view_size, &inputs->mouse_pos,
                                 &pos, &normal);
    if (snaped) {
        if (    snaped == SNAP_MESH && goxel->painter.op == OP_ADD &&
                !goxel->snap_offset)
            vec3_iadd(&pos, normal);
        if (goxel->tool == TOOL_BRUSH && goxel->snap_offset)
            vec3_iaddk(&pos, normal, goxel->snap_offset * goxel->tool_radius);
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
        box = bbox_from_extents(pos, 0.5, 0.5, 0.5);
        render_box(&goxel->rend, &box, false, NULL, false);
    }
    if (state == STATE_IDLE) {
        if (snaped) state = STATE_SNAPED;
    }
    if (state == STATE_SNAPED) {
        if (!snaped) return STATE_IDLE;
        if (down) {
            image_history_push(goxel->image);
            proc_stop(proc);
            proc_start(proc, &box);
            state = STATE_PAINT;
        }
    }
    if (state == STATE_PAINT) {
        if (!down) state = STATE_IDLE;
    }
    return state;
}

int tool_iter(goxel_t *goxel, int tool, const inputs_t *inputs, int state,
              const vec2_t *view_size, bool inside)
{
    int ret;

    typedef int (*tool_func_t)(goxel_t *goxel, const inputs_t *inputs,
                               int state, const vec2_t *view_size,
                               bool inside);
    static const tool_func_t FUNCS[] = {
        [TOOL_SHAPE]        = tool_shape_iter,
        [TOOL_BRUSH]        = tool_brush_iter,
        [TOOL_LASER]        = tool_laser_iter,
        [TOOL_SET_PLANE]    = tool_set_plane_iter,
        [TOOL_MOVE]         = tool_dummy_iter,
        [TOOL_PICK_COLOR]   = tool_pick_color_iter,
        [TOOL_SELECTION]    = tool_selection_iter,
        [TOOL_PROCEDURAL]   = tool_procedural_iter,
    };

    assert(tool >= 0 && tool < ARRAY_SIZE(FUNCS));
    assert(FUNCS[tool]);
    ret = FUNCS[tool](goxel, inputs, state, view_size, inside);
    if (ret == STATE_CANCEL && goxel->tool_origin_mesh) {
        mesh_set(&goxel->image->active_layer->mesh, goxel->tool_origin_mesh);
        goxel_update_meshes(goxel, true);
    }
    if (ret == STATE_CANCEL) ret = 0;
    if (ret == 0) {
        goxel->tool_plane = plane_null;
        if (goxel->tool_origin_mesh) {
            mesh_delete(goxel->tool_origin_mesh);
            goxel->tool_origin_mesh = NULL;
        }
    }
    return ret;
}

void tool_cancel(goxel_t *goxel, int tool, int state)
{
    if (state == 0) return;
    if (goxel->tool_origin_mesh) {
        mesh_set(&goxel->image->active_layer->mesh, goxel->tool_origin_mesh);
        goxel_update_meshes(goxel, true);
        mesh_delete(goxel->tool_origin_mesh);
        goxel->tool_origin_mesh = NULL;
    }
    goxel->tool_plane = plane_null;
    goxel->tool_state = 0;
}
