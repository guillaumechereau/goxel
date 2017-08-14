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
    struct {
        gesture3d_t drag;
    } gestures;
} tool_extrude_t;

static int select_cond(uvec4b_t value,
                       const uvec4b_t neighboors[6],
                       const uint8_t mask[6],
                       void *user)
{
    int i, snap_face = *((int*)user);
    int opp_face = ((int[6]){1, 0, 3, 2, 5, 4})[snap_face];
    if (value.a == 0) return 0;
    if (neighboors[snap_face].a) return 0;
    for (i = 0; i < 6; i++) {
        if (i == snap_face || i == opp_face) continue;
        if (mask[i]) return 255;
    }
    return 0;
}

// Get the face index from the normal.
static int get_face(vec3_t n)
{
    int f;
    for (f = 0; f < 6; f++) {
        if (vec3_dot(n, vec3(VEC3_SPLIT(FACES_NORMALS[f]))) > 0.5)
            return f;
    }
    return -1;
}

static int on_drag(gesture3d_t *gest, void *user)
{
    tool_extrude_t *tool = (tool_extrude_t*)user;
    mesh_t *mesh = goxel->image->active_layer->mesh;
    mesh_t *tmp_mesh;
    cursor_t *curs = gest->cursor;
    box_t box;
    plane_t face_plane;
    vec3_t n, pos;

    if (gest->state == GESTURE_BEGIN) {
        tool->snap_face = get_face(curs->normal);
        tool->mesh = mesh_new();
        mesh_select(mesh, &curs->pos, select_cond, &tool->snap_face,
                    tool->mesh);
        mesh_set(tool->mesh_orig, mesh);
        image_history_push(goxel->image);

        // XXX: to remove: this is duplicated from selection tool.
        box = mesh_get_box(tool->mesh, true);
        face_plane.mat = mat4_mul(box.mat,
                                  FACES_MATS[tool->snap_face]);
        goxel->tool_plane = plane(curs->pos, curs->normal,
                                  vec3_normalized(face_plane.u));
    }

    box = mesh_get_box(tool->mesh, true);

    // XXX: have some generic way to resize boxes, since we use it all the
    // time!
    face_plane.mat = mat4_mul(box.mat, FACES_MATS[tool->snap_face]);
    n = vec3_normalized(face_plane.n);
    pos = vec3_add(goxel->tool_plane.p,
                   vec3_project(vec3_sub(curs->pos, goxel->tool_plane.p), n));
    pos.x = round(pos.x);
    pos.y = round(pos.y);
    pos.z = round(pos.z);
    box = box_move_face(box, tool->snap_face, pos);
    render_box(&goxel->rend, &box, NULL, EFFECT_WIREFRAME);

    mesh_set(mesh, tool->mesh_orig);
    tmp_mesh = mesh_copy(tool->mesh);
    mesh_op(tmp_mesh, &goxel->painter, &box);
    mesh_merge(mesh, tmp_mesh, MODE_OVER);
    mesh_delete(tmp_mesh);
    goxel_update_meshes(goxel, MESH_RENDER);

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
