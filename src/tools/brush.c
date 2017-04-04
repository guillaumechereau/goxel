/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,

    STATE_SNAPED,
    STATE_PAINT,
    STATE_WAIT_KEY_UP,

    STATE_ENTER = 0x0100,
};

static bool check_can_skip(goxel_t *goxel, vec3_t pos, bool pressed, int mode)
{
    if (    pressed == goxel->tool_last_op.pressed &&
            mode == goxel->tool_last_op.mode &&
            vec3_equal(pos, goxel->tool_last_op.pos))
        return true;
    goxel->tool_last_op.pressed = pressed;
    goxel->tool_last_op.mode = mode;
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

int tool_brush_iter(goxel_t *goxel, const inputs_t *inputs, int state,
                    const vec2_t *view_size, bool inside)
{
    const bool down = inputs->mouse_down[0];
    const bool pressed = down && !goxel->painting;
    const bool released = !down && goxel->painting;
    int snaped = 0;
    vec3_t pos = vec3_zero, normal = vec3_zero;
    box_t box;
    painter_t painter2;
    mesh_t *mesh = goxel->image->active_layer->mesh;

    if (inside)
        snaped = goxel_unproject(
                goxel, view_size, &inputs->mouse_pos,
                goxel->painter.mode == MODE_ADD && !goxel->snap_offset,
                &pos, &normal);
    goxel_set_help_text(goxel, "Brush: use shift to draw lines, "
                               "ctrl to pick color");
    set_snap_hint(goxel, snaped);
    if (snaped) {
        if (goxel->snap_offset)
            vec3_iaddk(&pos, normal, goxel->snap_offset * goxel->tool_radius);
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
    }
    switch (state) {
    case STATE_IDLE:
        if (snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED | STATE_ENTER:
        mesh_set(goxel->tool_mesh_orig, mesh);
        goxel->tool_last_op.mode = 0; // Discard last op.
        break;

    case STATE_SNAPED:
        if (!snaped) return STATE_CANCEL;
        if (inputs->keys[KEY_LEFT_SHIFT])
            render_line(&goxel->rend, &goxel->tool_start_pos, &pos, NULL);
        if (check_can_skip(goxel, pos, down, goxel->painter.mode))
            return state;
        box = get_box(&pos, NULL, &normal, goxel->tool_radius, NULL);

        mesh_set(mesh, goxel->tool_mesh_orig);
        mesh_op(mesh, &goxel->painter, &box);
        goxel_update_meshes(goxel, MESH_LAYERS);

        if (inputs->keys[KEY_LEFT_SHIFT]) {
            render_line(&goxel->rend, &goxel->tool_start_pos, &pos, NULL);
            if (pressed) {
                painter2 = goxel->painter;
                painter2.shape = &shape_cylinder;
                box = get_box(&goxel->tool_start_pos, &pos, &normal,
                              goxel->tool_radius, NULL);
                mesh_set(mesh, goxel->tool_mesh_orig);
                mesh_op(mesh, &painter2, &box);
                mesh_set(goxel->tool_mesh_orig, mesh);
                goxel_update_meshes(goxel, MESH_LAYERS);
                goxel->tool_start_pos = pos;
            }
        }
        if (pressed) {
            state = STATE_PAINT;
            goxel->tool_last_op.mode = 0;
            goxel->painting = true;
            mesh_set(mesh, goxel->tool_mesh_orig);
            image_history_push(goxel->image);
            mesh_clear(goxel->tool_mesh);
        }
        break;

    case STATE_PAINT:
        if (!snaped) return state;
        if (check_can_skip(goxel, pos, down, goxel->painter.mode))
            return state;
        if (released) {
            goxel->painting = false;
            goxel->camera.target = pos;
            if (inputs->keys[KEY_LEFT_SHIFT])
                return STATE_WAIT_KEY_UP;
            mesh_set(goxel->pick_mesh, goxel->layers_mesh);
            return STATE_IDLE;
        }
        box = get_box(&pos, NULL, &normal, goxel->tool_radius, NULL);
        painter2 = goxel->painter;
        painter2.mode = MODE_MAX;
        mesh_op(goxel->tool_mesh, &painter2, &box);
        mesh_set(mesh, goxel->tool_mesh_orig);
        mesh_merge(mesh, goxel->tool_mesh, goxel->painter.mode);
        goxel_update_meshes(goxel, MESH_LAYERS);
        goxel->tool_start_pos = pos;
        break;

    case STATE_WAIT_KEY_UP:
        if (!inputs->keys[KEY_LEFT_SHIFT]) state = STATE_IDLE;
        if (snaped) state = STATE_SNAPED;
        break;
    }
    return state;
}

