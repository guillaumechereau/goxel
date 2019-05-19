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

#include <zlib.h> // For crc32.

camera_t *camera_new(const char *name)
{
    camera_t *cam = calloc(1, sizeof(*cam));
    strncpy(cam->name, name, sizeof(cam->name) - 1);
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
    quat_copy(other->rot, cam->rot);
    vec3_copy(other->ofs, cam->ofs);
}

static void compute_clip(const float view_mat[4][4], float *near_, float *far_)
{
    int bpos[3];
    float p[3];
    float n = FLT_MAX, f = 256;
    int i;
    const int margin = 8 * BLOCK_SIZE;
    float vertices[8][3];
    const mesh_t *mesh = goxel_get_layers_mesh();
    mesh_iterator_t iter;

    if (!box_is_null(goxel.image->box)) {
        box_get_vertices(goxel.image->box, vertices);
        for (i = 0; i < 8; i++) {
            mat4_mul_vec3(view_mat, vertices[i], p);
            if (p[2] < 0) {
                n = min(n, -p[2] - margin);
                f = max(f, -p[2] + margin);
            }
        }
    }

    iter = mesh_get_iterator(mesh, MESH_ITER_BLOCKS);
    while (mesh_iter(&iter, bpos)) {
        vec3_set(p, bpos[0], bpos[1], bpos[2]);
        mat4_mul_vec3(view_mat, p, p);
        if (p[2] < 0) {
            n = min(n, -p[2] - margin);
            f = max(f, -p[2] + margin);
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
    mat4_set_identity(camera->view_mat);
    mat4_itranslate(camera->view_mat, 0, 0, -camera->dist);
    mat4_imul_quat(camera->view_mat, camera->rot);
    mat4_itranslate(camera->view_mat,
           camera->ofs[0], camera->ofs[1], camera->ofs[2]);

    compute_clip(camera->view_mat, &clip_near, &clip_far);
    if (camera->ortho) {
        size = camera->dist;
        mat4_ortho(camera->proj_mat,
                -size, +size,
                -size / camera->aspect, +size / camera->aspect,
                clip_near, clip_far);
    } else {
        mat4_perspective(camera->proj_mat,
                camera->fovy, camera->aspect, clip_near, clip_far);
    }
}

// Get the raytracing ray of the camera at a given screen position.
void camera_get_ray(const camera_t *camera, const float win[2],
                    const float viewport[4], float o[3], float d[3])
{
    float o1[3], o2[3], p[3];
    vec3_set(p, win[0], win[1], 0);
    unproject(p, camera->view_mat, camera->proj_mat, viewport, o1);
    vec3_set(p, win[0], win[1], 1);
    unproject(p, camera->view_mat, camera->proj_mat, viewport, o2);
    vec3_copy(o1, o);
    vec3_sub(o2, o1, d);
    vec3_normalize(d, d);
}

// Adjust the camera settings so that the rotation works for a given
// position.
void camera_set_target(camera_t *cam, const float pos[3])
{
    // Adjust the offset z coordinate (in the rotated referential) to put
    // it in the xy plan intersecting the target point.  Then adjust the
    // distance so that the final view matrix stays the same.
    float u[4], v[4];
    float d;
    float roti[4] = {cam->rot[0], -cam->rot[1], -cam->rot[2], -cam->rot[3]};
    quat_mul_vec4(roti, VEC(0, 0, 1, 0), u);
    vec3_add(pos, cam->ofs, v);
    d = vec3_dot(v, u);
    vec3_iaddk(cam->ofs, u, -d);
    cam->dist -= d;
}

/*
 * Function: camera_fit_box
 * Move a camera so that a given box is entirely visible.
 */
void camera_fit_box(camera_t *cam, const float box[4][4])
{
    float size[3];
    if (box_is_null(box)) {
        cam->dist = 128;
        cam->aspect = 1;
        return;
    }
    box_get_size(box, size);
    mat4_mul_vec3(box, VEC(0, 0, 0), cam->ofs);
    vec3_imul(cam->ofs, -1);
    // XXX: not the proper way to compute the distance.
    cam->dist = max3(size[0], size[1], size[2]) * 8;
}

/*
 * Function: camera_get_key
 * Return a value that is guarantied to change when the camera change.
 */
uint32_t camera_get_key(const camera_t *cam)
{
    uint32_t key = 0;
    key = crc32(key, (void*)&cam->name, sizeof(cam->name));
    key = crc32(key, (void*)&cam->ortho, sizeof(cam->ortho));
    key = crc32(key, (void*)&cam->dist, sizeof(cam->dist));
    key = crc32(key, (void*)&cam->rot, sizeof(cam->rot));
    key = crc32(key, (void*)&cam->ofs, sizeof(cam->ofs));
    return key;
}
