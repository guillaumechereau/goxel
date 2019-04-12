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
 * Define the 3d shape we can use for the mesh operations.
 *
 * TODO: need to explain how that works.
 */

#ifndef SHAPE_H
#define SHAPE_H

typedef struct shape {
    const char *id;
    float (*func)(const float p[3], const float s[3], float smoothness);
} shape_t;

void shapes_init(void);
extern shape_t shape_sphere;
extern shape_t shape_cube;
extern shape_t shape_cylinder;

#endif // SHAPE_H
