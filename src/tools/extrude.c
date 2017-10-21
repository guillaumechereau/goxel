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
    mesh_t *mesh_orig;
    mesh_t *mesh;
    int    snap_face;
    int    last_delta;
    struct {
        gesture3d_t drag;
    } gestures;
} tool_extrude_t;

static int select_cond(const uint8_t value[4],
                       const uint8_t neighboors[6][4],
                       const uint8_t mask[6],
                       void *user)
{
    int i, snap_face = *((int*)user);
    int opp_face = ((int[6]){1, 0, 3, 2, 5, 4})[snap_face];
    if (value[3] == 0) return 0;
    if (neighboors[snap_face][3]) return 0;
    for (i = 0; i < 6; i++) {
        if (i == snap_face || i == opp_face) continue;
        if (mask[i]) return 255;
    }
    return 0;
}

// Get the face index from the normal.
// XXX: used in a few other places!
static int get_face(vec3_t n)
{
    int f;
    const int *n2;
    for (f = 0; f < 6; f++) {
        n2 = FACES_NORMALS[f];
        if (vec3_dot(n, vec3(n2[0], n2[1], n2[2])) > 0.5)
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
    mesh_t *mesh = goxel->image->active_layer->mesh;
    mesh_t *tmp_mesh;
    cursor_t *curs = gest->cursor;
    box_t box;
    plane_t face_plane;
    vec3_t n, pos;
    int pi[3];
    float delta;

    if (gest->state == GESTURE_BEGIN) {
        tool->snap_face = get_face(curs->normal);

        tmp_mesh = mesh_new();
        tool->mesh = mesh_copy(mesh);
        pi[0] = floor(curs->pos.x);
        pi[1] = floor(curs->pos.y);
        pi[2] = floor(curs->pos.z);
        mesh_select(mesh, pi, select_cond, &tool->snap_face,
                    tmp_mesh);
        mesh_merge(tool->mesh, tmp_mesh, MODE_MULT_ALPHA);
        mesh_delete(tmp_mesh);

        mesh_set(tool->mesh_orig, mesh);
        image_history_push(goxel->image);

        // XXX: to remove: this is duplicated from selection tool.
        box = mesh_get_box(tool->mesh, true);
        face_plane.mat = mat4_mul(box.mat,
                                  FACES_MATS[tool->snap_face]);
        goxel->tool_plane = plane(curs->pos, curs->normal,
                                  vec3_normalized(face_plane.u));
        tool->last_delta = 0;
    }

    box = mesh_get_box(tool->mesh, true);

    // XXX: have some generic way to resize boxes, since we use it all the
    // time!
    face_plane.mat = mat4_mul(box.mat, FACES_MATS[tool->snap_face]);
    n = vec3_normalized(face_plane.n);
    // XXX: Is there a better way to compute the delta??
    delta = vec3_dot(n,
                vec3_project(vec3_sub(curs->pos, goxel->tool_plane.p), n));
    // render_box(&goxel->rend, &box, NULL, EFFECT_WIREFRAME);

    // Skip if we didn't move.
    if (round(delta) == tool->last_delta) goto end;
    tool->last_delta = round(delta);

    pos = vec3_add(goxel->tool_plane.p,
                   vec3_project(vec3_sub(curs->pos, goxel->tool_plane.p), n));
    pos.x = round(pos.x);
    pos.y = round(pos.y);
    pos.z = round(pos.z);

    mesh_set(mesh, tool->mesh_orig);
    tmp_mesh = mesh_copy(tool->mesh);

    if (delta >= 1) {
        vec3_iaddk(&face_plane.p, n, -0.5);
        box = box_move_face(box, tool->snap_face, pos);
        mesh_extrude(tmp_mesh, &face_plane, &box);
        mesh_merge(mesh, tmp_mesh, MODE_OVER);
    }
    if (delta < 0.5) {
        box = box_move_face(box, FACES_OPPOSITES[tool->snap_face], pos);
        vec3_imul(&face_plane.n, -1.0);
        vec3_iaddk(&face_plane.p, n, -0.5);
        mesh_extrude(tmp_mesh, &face_plane, &box);
        mesh_merge(mesh, tmp_mesh, MODE_SUB);
    }
    mesh_delete(tmp_mesh);
    goxel_update_meshes(goxel, MESH_RENDER);

end:
    if (gest->state == GESTURE_END) {
        mesh_delete(tool->mesh);
        goxel->tool_plane = plane_null;
        goxel_update_meshes(goxel, -1);
    }
    return 0;
}

static int iter(tool_t *tool_, const vec4_t *view)
{
    tool_extrude_t *tool = (tool_extrude_t*)tool_;
    cursor_t *curs = &goxel->cursor;
    curs->snap_offset = -0.5;
    curs->snap_mask &= ~SNAP_ROUNDED;

    if (!tool->mesh_orig) tool->mesh_orig = mesh_new();
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
    return 0;
}

TOOL_REGISTER(TOOL_EXTRUDE, extrude, tool_extrude_t,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT,
)
