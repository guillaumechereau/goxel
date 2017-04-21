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
    block_t *block;
    vec3_t p;
    float n = FLT_MAX, f = 256;
    const int margin = 8 * BLOCK_SIZE;
    MESH_ITER_BLOCKS(goxel->layers_mesh, block) {
        p = vec3(block->pos.x, block->pos.y, block->pos.z);
        p = mat4_mul_vec3(*view_mat, p);
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
    if (camera->move_to_target) {
        camera->move_to_target = !vec3_ilerp_const(
                &camera->ofs, vec3_neg(camera->target), camera->dist / 128);
    }
    // Update the camera mats
    camera->view_mat = mat4_identity;
    mat4_itranslate(&camera->view_mat, 0, 0, -camera->dist);
    mat4_imul_quat(&camera->view_mat, camera->rot);
    mat4_itranslate(&camera->view_mat,
           camera->ofs.x, camera->ofs.y, camera->ofs.z);

    compute_clip(&camera->view_mat, &clip_near, &clip_far);
    if (camera->ortho) {
        size = camera->dist;
        camera->proj_mat = mat4_ortho(
                -size, +size,
                -size / camera->aspect, +size / camera->aspect,
                clip_near, clip_far);
    } else {
        camera->proj_mat = mat4_perspective(
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
    *d = vec3_normalized(vec3_sub(o2, o1));
}
