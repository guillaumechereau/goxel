/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * Section: Camera
 * Camera manipulation functions.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdbool.h>

/* Type: camera_t
 * Camera structure.
 *
 * Attributes:
 *   next, prev - Used for linked list of cameras in an image.
 *   name       - Name to show in the GUI.
 *   ortho      - Set to true for orthographic projection.
 *   dist       - Distance used to compute the position.
 *   mat        - Position of the camera (the camera points toward -z).
 *   fovy       - Field of view in y direction.
 *   aspect     - Aspect ratio.
 *   view_mat   - Modelview transformation matrix (auto computed).
 *   proj_mat   - Projection matrix (auto computed).
 */
typedef struct camera camera_t;
struct camera
{
    camera_t  *next, *prev; // List of camera in an image.
    char   name[128];  // 127 chars max.
    bool   ortho; // Set to true for orthographic projection.
    float  dist;  // Rotation distance.
    float  fovy;
    float  aspect;
    float  mat[4][4];

    // Auto computed from other values:
    float view_mat[4][4];    // Model to view transformation.
    float proj_mat[4][4];    // Proj transform from camera coordinates.
};

/*
 * Function: camera_new
 * Create a new camera.
 *
 * Parameters:
 *   name - The name of the camera.
 *
 * Return:
 *   A newly instanciated camera.
 */
camera_t *camera_new(const char *name);

/*
 * Function: camera_delete
 * Delete a camera
 */
void camera_delete(camera_t *camera);

camera_t *camera_copy(const camera_t *other);

/*
 * Function: camera_set
 * Set a camera position from an other camera.
 */
void camera_set(camera_t *camera, const camera_t *other);

/*
 * Function: camera_update
 * Update camera matrices.
 */
void camera_update(camera_t *camera);

/*
 * Function: camera_set_target
 * Adjust the camera settings so that the rotation works for a given
 * position.
 */
void camera_set_target(camera_t *camera, const float pos[3]);

/*
 * Function: camera_get_ray
 * Get the raytracing ray of the camera at a given screen position.
 *
 * Parameters:
 *   win   - Pixel position in screen coordinates.
 *   view  - Viewport rect: [min_x, min_y, max_x, max_y].
 *   o     - Output ray origin.
 *   d     - Output ray direction.
 */
void camera_get_ray(const camera_t *camera, const float win[2],
                    const float viewport[4], float o[3], float d[3]);

/*
 * Function: camera_fit_box
 * Move a camera so that a given box is entirely visible.
 */
void camera_fit_box(camera_t *camera, const float box[4][4]);

/*
 * Function: camera_get_key
 * Return a value that is guarantied to change when the camera change.
 */
uint32_t camera_get_key(const camera_t *camera);

void camera_turntable(camera_t *camera, float rz, float rx);

#endif // CAMERA_H
