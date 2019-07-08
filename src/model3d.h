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
 * Section: Model3d
 *
 * Functions to render 3d vertex models, like cube, plane, etc.
 */

#ifndef MODEL3D_H
#define MODEL3D_H

#include "utils/texture.h"

/*
 * Type: model_vertex_t
 * Container for a single vertex shader data.
 */
typedef struct {
     float    pos[3]    __attribute__((aligned(4)));
     float    normal[3] __attribute__((aligned(4)));
     uint8_t  color[4]  __attribute__((aligned(4)));
     float    uv[2]     __attribute__((aligned(4)));
} model_vertex_t;

/*
 * Type: model3d_t
 * Define a 3d model.
 */
typedef struct {
    int              nb_vertices;
    model_vertex_t   *vertices;
    bool             solid;
    bool             cull;

    // Rendering buffers.
    // XXX: move this into the renderer, like for block_t
    uint32_t vertex_buffer;
    int      nb_lines;
    bool     dirty;
} model3d_t;

void model3d_release_graphics(void);

/*
 * Function: model3d_delete
 * Delete a 3d model
 */
void model3d_delete(model3d_t *model);

/*
 * Function: model3d_cube
 * Create a 3d cube from (0, 0, 0) to (1, 1, 1)
 */
model3d_t *model3d_cube(void);

/*
 * Function: model3d_wire_cube
 * Create a 3d wire cube from (0, 0, 0) to (1, 1, 1)
 */
model3d_t *model3d_wire_cube(void);

/*
 * Function: model3d_sphere
 * Create a sphere of radius 1, centered at (0, 0).
 */
model3d_t *model3d_sphere(int slices, int stacks);

/*
 * Function: model3d_grid
 * Create a grid plane.
 */
model3d_t *model3d_grid(int nx, int ny);

/*
 * Function: model3d_line
 * Create a line from (-0.5, 0, 0) to (+0.5, 0, 0).
 */
model3d_t *model3d_line(void);

/*
 * Function: model3d_line
 * Create a 2d rect on xy plane, of size 1x1 centered on the origin.
 */
model3d_t *model3d_rect(void);

/*
 * Function: model3d_line
 * Create a 2d rect on xy plane, of size 1x1 centered on the origin.
 */
model3d_t *model3d_wire_rect(void);

/*
 * Function: model3d_render
 * Render a 3d model using OpenGL calls.
 */
void model3d_render(model3d_t *model3d,
                    const float model[4][4],
                    const float view[4][4],
                    const float proj[4][4],
                    const uint8_t color[4],
                    const texture_t *tex,
                    const float light[3],
                    const float clip_box[4][4],
                    int   effects);

#endif // MODEL3D_H
