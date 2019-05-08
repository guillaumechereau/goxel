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

/*
 * Here is the convention I used for the cube vertices, edges and faces:
 *
 *           v4 +----------e4---------+ v5
 *             /.                    /|
 *            / .                   / |
 *          e7  .                 e5  |                    +-----------+
 *          /   .                 /   |                   /           /|
 *         /    .                /    |                  /   f1      / |  <f2
 *     v7 +----------e6---------+ v6  |                 +-----------+  |
 *        |     .               |     e9            f5> |           |f4|
 *        |     e8              |     |                 |           |  |
 *        |     .               |     |                 |    f3     |  +
 *        |     .               |     |                 |           | /
 *        |  v0 . . . .e0 . . . | . . + v1              |           |/
 *       e11   .                |    /                  +-----------+
 *        |   .                e10  /                         ^
 *        |  e3                 |  e1                         f0
 *        | .                   | /
 *        |.                    |/
 *     v3 +---------e2----------+ v2
 *
 */

#include "utils/vec.h"

// face index -> [vertex0, vertex1, vertex2, vertex3]
static const int FACES_VERTICES[6][4] = {
    {0, 1, 2, 3},
    {5, 4, 7, 6},
    {0, 4, 5, 1},
    {2, 6, 7, 3},
    {1, 5, 6, 2},
    {0, 3, 7, 4}
};

static const int FACES_OPPOSITES[6] = {
    1, 0, 3, 2, 5, 4
};

// face index + edge -> neighbor face index.
static const int FACES_NEIGHBORS[6][4] = {
    {4, 3, 5, 2},
    {5, 3, 4, 2},
    {1, 4, 0, 5},
    {1, 5, 0, 4},
    {1, 3, 0, 2},
    {3, 1, 2, 0},
};

// vertex index -> vertex position
static const int VERTICES_POSITIONS[8][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {1, 0, 1},
    {0, 0, 1},
    {0, 1, 0},
    {1, 1, 0},
    {1, 1, 1},
    {0, 1, 1}
};

static const int VERTICE_UV[4][2] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1},
};

static const int FACES_NORMALS[6][3] = {
    { 0, -1,  0},
    { 0, +1,  0},
    { 0,  0, -1},
    { 0,  0, +1},
    { 1,  0,  0},
    {-1,  0,  0},
};

static const int FACES_TANGENTS[6][3] = {
    {+1,  0,  0},
    {-1,  0,  0},
    { 0, +1,  0},
    { 0, +1,  0},
    { 0, +1,  0},
    { 0,  0, +1},
};

// Matrices of transformation: unity plane => cube face plane.
static const float FACES_MATS[6][4][4] = {
    {{ 1, 0, 0, 0}, { 0, 0, 1, 0}, { 0, -1,  0, 0}, { 0, -1,  0, 1}},
    {{-1, 0, 0, 0}, { 0, 0, 1, 0}, { 0,  1,  0, 0}, { 0,  1,  0, 1}},
    {{ 0, 1, 0, 0}, { 1, 0, 0, 0}, { 0,  0, -1, 0}, { 0,  0, -1, 1}},
    {{ 0, 1, 0, 0}, {-1, 0, 0, 0}, { 0,  0,  1, 0}, { 0,  0,  1, 1}},
    {{ 0, 1, 0, 0}, { 0, 0, 1, 0}, { 1,  0,  0, 0}, { 1,  0,  0, 1}},
    {{ 0, 0, 1, 0}, { 0, 1, 0, 0}, {-1,  0,  0, 0}, {-1,  0,  0, 1}},
};

static const int EDGES_VERTICES[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

