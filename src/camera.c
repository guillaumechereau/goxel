/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

camera_t *camera_new(const char *name)
{
    camera_t *cam = calloc(1, sizeof(*cam));
    strncpy(cam->name, name, sizeof(cam->name));
    return cam;
}

void camera_delete(camera_t *cam)
{
    free(cam);
}

void camera_set(camera_t *cam, const camera_t *other)
{
    cam->ortho = other->ortho;
    cam->dist = other->dist;
    cam->rot = other->rot;
    cam->ofs = other->ofs;
}

static void compute_clip(const mat4_t *view_mat, float *near_, float *far_)
{
    int bpos[3];
    vec3_t p;
    float n = FLT_MAX, f = 256;
    int i;
    const int margin = 8 * BLOCK_SIZE;
    vec3_t vertices[8];
    const mesh_t *mesh = goxel->layers_mesh;
    mesh_iterator_t iter;

    if (!box_is_null(goxel->image->box)) {
        box_get_vertices(goxel->image->box, vertices);
        for (i = 0; i < 8; i++) {
            mat4_mul_vec3(view_mat->v2, vertices[i].v, p.v);
            if (p.z < 0) {
                n = min(n, -p.z - margin);
                f = max(f, -p.z + margin);
            }
        }
    }

    iter = mesh_get_iterator(mesh, MESH_ITER_BLOCKS);
    while (mesh_iter(&iter, bpos)) {
        p = vec3(bpos[0], bpos[1], bpos[2]);
        mat4_mul_vec3(view_mat->v2, p.v, p.v);
        if (p.z < 0) {
            n = min(n, -p.z - margin);
            f = max(f, -p.z + margin);
        }
    }
    if (n >= f) n = 1;
    n = max(n, 1);
    *near_ = n;
    *far_ = f;
}

void camera_update(camera_t *camera)
{
    float size;
    float clip_near, clip_far;

    camera->fovy = 20.;
    // Update the camera mats
    camera->view_mat = mat4_identity;
    mat4_itranslate(camera->view_mat.v2, 0, 0, -camera->dist);
    mat4_imul_quat(&camera->view_mat, camera->rot);
    mat4_itranslate(camera->view_mat.v2,
           camera->ofs.x, camera->ofs.y, camera->ofs.z);

    compute_clip(&camera->view_mat, &clip_near, &clip_far);
    if (camera->ortho) {
        size = camera->dist;
        mat4_ortho(camera->proj_mat.v2,
                -size, +size,
                -size / camera->aspect, +size / camera->aspect,
                clip_near, clip_far);
    } else {
        mat4_perspective(camera->proj_mat.v2,
                camera->fovy, camera->aspect, clip_near, clip_far);
    }
}

// Get the raytracing ray of the camera at a given screen position.
void camera_get_ray(const camera_t *camera, const vec2_t *win,
                    const vec4_t *view, vec3_t *o, vec3_t *d)
{
    vec3_t o1, o2, p;
    p = vec3(win->x, win->y, 0);
    o1 = unproject(&p, &camera->view_mat, &camera->proj_mat, view);
    p = vec3(win->x, win->y, 1);
    o2 = unproject(&p, &camera->view_mat, &camera->proj_mat, view);
    *o = o1;
    vec3_sub(o2.v, o1.v, d->v);
    vec3_normalize(d->v, d->v);
}

// Adjust the camera settings so that the rotation works for a given
// position.
void camera_set_target(camera_t *cam, const vec3_t *pos)
{
    // Adjust the offset z coordinate (in the rotated referential) to put
    // it in the xy plan intersecting the target point.  Then adjust the
    // distance so that the final view matrix stays the same.
    float u[4], v[4];
    float d;
    quat_t roti = quat(cam->rot.w, -cam->rot.x, -cam->rot.y, -cam->rot.z);
    quat_mul_vec4(roti, vec4(0, 0, 1, 0).v, u);
    vec3_add(pos->v, cam->ofs.v, v);
    d = vec3_dot(v, u);
    vec3_iaddk(cam->ofs.v, u, -d);
    cam->dist -= d;
}
