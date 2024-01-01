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

typedef struct {
    tool_t tool;
    volume_t *volume_orig;
    volume_t *volume;
    int    snap_face;
    int    last_delta;
    struct {
        gesture3d_t drag;
    } gestures;
} tool_extrude_t;

static int select_cond(void *user, const volume_t *volume,
                       const int base_pos[3],
                       const int new_pos[3],
                       volume_accessor_t *volume_accessor)
{
    int snap_face = *((int*)user);
    int p[3], n[3];

    // Only consider voxel in the snap plane.
    memcpy(n, FACES_NORMALS[snap_face], sizeof(n));
    p[0] = new_pos[0] - base_pos[0];
    p[1] = new_pos[1] - base_pos[1];
    p[2] = new_pos[2] - base_pos[2];
    if (p[0] * n[0] + p[1] * n[1] + p[2] * n[2]) return 0;

    // Also ignore if the face is not visible.
    p[0] = new_pos[0] + FACES_NORMALS[snap_face][0];
    p[1] = new_pos[1] + FACES_NORMALS[snap_face][1];
    p[2] = new_pos[2] + FACES_NORMALS[snap_face][2];
    if (volume_get_alpha_at(volume, volume_accessor, p)) return 0;

    return 255;
}

// Get the face index from the normal.
// XXX: used in a few other places!
static int get_face(const float n[3])
{
    int f;
    const int *n2;
    for (f = 0; f < 6; f++) {
        n2 = FACES_NORMALS[f];
        if (vec3_dot(n, VEC(n2[0], n2[1], n2[2])) > 0.5)
            return f;
    }
    return -1;
}

// XXX: this code is just too ugly.  Needs a lot of cleanup.
// The problem is that we should use some generic functions to handle
// box resize, since we do it a lot, and the code is never very clear.
static int on_drag(gesture3d_t *gest, void *user)
{
    tool_extrude_t *tool = (tool_extrude_t*)user;
    volume_t *volume = goxel.image->active_layer->volume;
    volume_t *tmp_volume;
    cursor_t *curs = gest->cursor;
    float face_plane[4][4];
    float n[3], pos[3], v[3], box[4][4];
    int pi[3];
    float delta;

    if (gest->state == GESTURE_BEGIN) {
        tool->snap_face = get_face(curs->normal);

        tmp_volume = volume_new();
        tool->volume = volume_copy(volume);
        pi[0] = floor(curs->pos[0]);
        pi[1] = floor(curs->pos[1]);
        pi[2] = floor(curs->pos[2]);
        volume_select(volume, pi, select_cond, &tool->snap_face, tmp_volume);
        volume_merge(tool->volume, tmp_volume, MODE_MULT_ALPHA, NULL);
        volume_delete(tmp_volume);

        volume_set(tool->volume_orig, volume);
        image_history_push(goxel.image);

        // XXX: to remove: this is duplicated from selection tool.
        volume_get_box(tool->volume, true, box);
        mat4_mul(box, FACES_MATS[tool->snap_face], face_plane);
        vec3_normalize(face_plane[0], v);
        plane_from_vectors(goxel.tool_plane, curs->pos, curs->normal, v);
        tool->last_delta = 0;
    }

    volume_get_box(tool->volume, true, box);

    // XXX: have some generic way to resize boxes, since we use it all the
    // time!
    mat4_mul(box, FACES_MATS[tool->snap_face], face_plane);
    vec3_normalize(face_plane[2], n);
    // XXX: Is there a better way to compute the delta??
    vec3_sub(curs->pos, goxel.tool_plane[3], v);
    vec3_project(v, n, v);
    delta = vec3_dot(n, v);
    // render_box(&goxel.rend, &box, NULL, EFFECT_WIREFRAME);

    // Skip if we didn't move.
    if (round(delta) == tool->last_delta) goto end;
    tool->last_delta = round(delta);

    vec3_sub(curs->pos, goxel.tool_plane[3], v);
    vec3_project(v, n, v);
    vec3_add(goxel.tool_plane[3], v, pos);
    pos[0] = round(pos[0]);
    pos[1] = round(pos[1]);
    pos[2] = round(pos[2]);

    volume_set(volume, tool->volume_orig);
    tmp_volume = volume_copy(tool->volume);

    if (delta >= 1) {
        vec3_iaddk(face_plane[3], n, -0.5);
        box_move_face(box, tool->snap_face, pos, box);
        volume_extrude(tmp_volume, face_plane, box);
        volume_merge(volume, tmp_volume, MODE_OVER, NULL);
    }
    if (delta < 0.5) {
        box_move_face(box, FACES_OPPOSITES[tool->snap_face], pos, box);
        vec3_imul(face_plane[2], -1.0);
        vec3_iaddk(face_plane[3], n, -0.5);
        volume_extrude(tmp_volume, face_plane, box);
        volume_merge(volume, tmp_volume, MODE_SUB, NULL);
    }
    volume_delete(tmp_volume);

end:
    if (gest->state == GESTURE_END) {
        volume_delete(tool->volume);
        mat4_copy(plane_null, goxel.tool_plane);
    }
    return 0;
}

static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    tool_extrude_t *tool = (tool_extrude_t*)tool_;
    cursor_t *curs = &goxel.cursor;
    curs->snap_offset = -0.5;
    curs->snap_mask &= ~SNAP_ROUNDED;

    if (!tool->volume_orig) tool->volume_orig = volume_new();
    if (!tool->gestures.drag.type) {
        tool->gestures.drag = (gesture3d_t) {
            .type = GESTURE_DRAG,
            .callback = on_drag,
        };
    }
    gesture3d(&tool->gestures.drag, curs, tool);
    return 0;
}

static int gui(tool_t *tool)
{
    goxel_set_help_text("Drag any surface to extrude it.");
    return 0;
}

TOOL_REGISTER(TOOL_EXTRUDE, extrude, tool_extrude_t,
              .name = "Extrude",
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT,
              .default_shortcut = "F"
)
