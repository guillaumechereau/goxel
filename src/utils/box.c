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

#include "utils/box.h"

static bool box_intersect_box_(const float b1[4][4], const float b2[4][4])
{
    float inv[4][4], box[4][4], vertices[8][3];
    int i, p;
    // The six planes equations:
    const int P[6][4] = {
        {-1, 0, 0, -1}, {1, 0, 0, -1},
        {0, -1, 0, -1}, {0, 1, 0, -1},
        {0, 0, -1, -1}, {0, 0, 1, -1}
    };
    if (!mat4_invert(b1, inv)) return false;
    mat4_mul(inv, b2, box);
    box_get_vertices(box, vertices);
    for (p = 0; p < 6; p++) {
        for (i = 0; i < 8; i++) {
            if (    P[p][0] * vertices[i][0] +
                    P[p][1] * vertices[i][1] +
                    P[p][2] * vertices[i][2] +
                    P[p][3] * 1 < 0) {
                break;
            }
        }
        if (i == 8) // All the points are outside a clipping plane.
            return false;
    }
    return true;
}

bool box_intersect_box(const float b1[4][4], const float b2[4][4])
{
    return box_intersect_box_(b1, b2) || box_intersect_box_(b2, b1);
}

void box_union(const float a[4][4], const float b[4][4], float out[4][4])
{
    float verts[16][3];

    if (box_is_null(a)) {
        mat4_copy(b, out);
        return;
    }
    if (box_is_null(b)) {
        mat4_copy(a, out);
        return;
    }

    box_get_vertices(a, verts + 0);
    box_get_vertices(b, verts + 8);
    bbox_from_npoints(out, 16, verts);
}
