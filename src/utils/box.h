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

#ifndef BOX_H
#define BOX_H

#include "utils/vec.h"

#include <assert.h>

/*
 * A Box is represented as the 4x4 matrix that transforms the unit cube (
 * extending from (-1, -1, -1), to (+1, +1, +1) into the box.
 *
 * A BBox is a box that contains no rotation, and so can be represented by
 * two min and max positions.
 */

/**
 * Check whether a box is a bbox (ie, is has no rotation).
 */
bool box_is_bbox(const float b[4][4]);

void bbox_from_extents(
        float box[4][4], const float pos[3], float hw, float hh, float hd);

static inline bool box_is_null(const float b[4][4])
{
    return b[3][3] == 0;
}

void bbox_from_aabb(float box[4][4], const int aabb[2][3]);

void bbox_to_aabb(const float b[4][4], int aabb[2][3]);

// XXX: remove?
void bbox_from_points(float box[4][4], const float a[3], const float b[3]);

void bbox_from_npoints(float box[4][4], int n, const float (*points)[3]);

bool bbox_contains(const float a[4][4], const float b[4][4]);

bool box_contains(const float a[4][4], const float b[4][4]);

bool bbox_contains_vec(const float b[4][4], const float v[3]);

void box_get_bbox(const float b[4][4], float out[4][4]);

void bbox_grow(const float b[4][4], float x, float y, float z, float out[4][4]);

void box_get_size(const float b[4][4], float out[3]);

void box_swap_axis(const float b[4][4], int x, int y, int z, float out[4][4]);

// Create a new box with the 4 points opposit to the face f and the
// new point.
void box_move_face(
        const float b[4][4], int f, const float p[3], float out[4][4]);

float box_get_volume(const float box[4][4]);

void box_get_vertices(const float box[4][4], float vertices[8][3]);

bool box_intersect_box(const float b1[4][4], const float b2[4][4]);

bool box_intersect_aabb(const float box[4][4], const int aabb[2][3]);

/*
 * Function: box_union
 * Return a box that constains two other ones.
 */
void box_union(const float a[4][4], const float b[4][4], float out[4][4]);

void box_get_aabb(const float box[4][4], int aabb[2][3]);

/*
 * box_extends_from_points
 * Extends a bbox to contain some extra points.
 */
void bbox_extends_from_points(
        const float b[4][4], int n, const float (*points)[3], float out[4][4]);

#endif // BOX_H
