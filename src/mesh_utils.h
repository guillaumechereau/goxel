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

/* ######## Section: Mesh util ############################################
 * Some extra mesh functions, not part of the core mesh code.
 */

#ifndef MESH_UTILS_H
#define MESH_UTILS_H

#include "shape.h"

/*
 * Enum: MODE
 * Define how layers/brush are merged.  Each mode defines how to apply a
 * source voxel into a destination voxel.
 *
 * MODE_OVER        - New values replace old one.
 * MODE_SUB         - Substract source alpha from destination
 * MODE_SUB_CLAMP   - Set alpha to the minimum between the destination value
 *                    and one minus the source value.
 * MODE_PAINT       - Set the color of the destination using the source.
 * MODE_MAX         - Set alpha to the max of the source and destination.
 * MODE_INTERSECT   - Set alpha to the min of the source and destination.
 * MODE_MULT_ALPHA  - Multiply the source and dest using source alpha.
 */
enum {
    MODE_NULL,
    MODE_OVER,
    MODE_SUB,
    MODE_SUB_CLAMP,
    MODE_PAINT,
    MODE_MAX,
    MODE_INTERSECT,
    MODE_MULT_ALPHA,
};


// Structure used for the OpenGL array data of blocks.
// XXX: we can probably make it smaller.
typedef struct voxel_vertex
{
    uint8_t  pos[3]                     __attribute__((aligned(4)));
    int8_t   normal[3]                  __attribute__((aligned(4)));
    int8_t   tangent[3]                 __attribute__((aligned(4)));
    int8_t   gradient[3]                __attribute__((aligned(4)));
    uint8_t  color[4]                   __attribute__((aligned(4)));
    uint16_t pos_data                   __attribute__((aligned(4)));
    uint8_t  uv[2]                      __attribute__((aligned(4)));
    uint8_t  occlusion_uv[2]            __attribute__((aligned(4)));
    uint8_t  bump_uv[2]                 __attribute__((aligned(4)));
} voxel_vertex_t;


// Type: painter_t
// The painting context, including the tool, brush, mode, radius,
// color, etc...
//
// Attributes:
//   mode - Define how colors are applied.  One of the <MODE> enum value.
typedef struct painter {
    int             mode;
    const shape_t   *shape;
    uint8_t         color[4];
    float           smoothness;
    int             symmetry; // bitfield X Y Z
    float           symmetry_origin[3];
    float           (*box)[4][4];     // Clipping box (can be null)
} painter_t;


/* Function: mesh_get_box
 * Compute the bounding box of a mesh.  */
// XXX: remove this function!
void mesh_get_box(const mesh_t *mesh, bool exact, float box[4][4]);

/* Function: mesh_op
 * Apply a paint operation to a mesh.
 * This function render geometrical 3d shapes into a mesh.
 * The shape, mode and color are defined in the painter argument.
 *
 * Parameters:
 *   mesh    - The mesh we paint into.
 *   painter - Defines the paint operation to apply.
 *   box     - Defines the position and size of the shape as the
 *             transformation matrix from the zero centered unit box.
 *
 * See Also:
 *   <painter_t>
 */
void mesh_op(mesh_t *mesh, const painter_t *painter, const float box[4][4]);

// XXX: to cleanup.
void mesh_extrude(mesh_t *mesh,
                  const float plane[4][4],
                  const float box[4][4]);

/* Function: mesh_blit
 *
 * Blit voxel data into a mesh.
 * This is the fastest way to quickly put data into a mesh.
 *
 * Parameters:
 *   mesh - The mesh we blit into.
 *   data - Pointer to voxel data (RGBA values, in xyz order).
 *   x    - X pos.
 *   y    - Y pos.
 *   z    - Z pos.
 *   w    - Width of the data.
 *   h    - Height of the data.
 *   d    - Depth of the data.
 *   iter - Optional iterator for optimized access.
 */
void mesh_blit(mesh_t *mesh, const uint8_t *data,
               int x, int y, int z, int w, int h, int d,
               mesh_iterator_t *iter);

void mesh_move(mesh_t *mesh, const float mat[4][4]);

void mesh_shift_alpha(mesh_t *mesh, int v);

// Compute the selection mask for a given condition.
int mesh_select(const mesh_t *mesh,
                const int start_pos[3],
                int (*cond)(void *user, const mesh_t *mesh,
                            const int base_pos[3],
                            const int new_pos[3],
                            mesh_accessor_t *mesh_accessor),
                void *user, mesh_t *selection);

/*
 * Function: mesh_merge
 * Merge a mesh into an other using a given blending function.
 *
 * Parameters:
 *   mesh   - The destination mesh we merge into.
 *   other  - The source mesh we merge.  Unchanged by this function.
 *   mode   - The blending function used.  One of the <MODE> enum values.
 *   color  - A color to apply to the source mesh before merging.  Can be
 *            set to NULL.
 */
void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode,
                const uint8_t color[4]);

/*
 * Function: mesh_generate_vertices
 * Generate a vertice array for rendering a mesh block.
 *
 * Parameters:
 *   mesh       - Input mesh.
 *   block_pos  - Position of the mesh block to render.
 *   effects    - Effect flags.
 *   out        - Output array.
 *   size       - Output the size of a single face.
 *                4 for quads and 3 for triangles.  Normal mesh uses quad
 *                but marching cube effect return triangle arrays.
 *   subdivide  - Ouput the number of subdivisions used for a voxel.  Normal
 *                render uses 1 unit per voxel, but marching cube rendering
 *                can use more.
 */
int mesh_generate_vertices(const mesh_t *mesh, const int block_pos[3],
                           int effects, voxel_vertex_t *out,
                           int *size, int *subdivide);

// XXX: use int[2][3] for the box?
void mesh_crop(mesh_t *mesh, const float box[4][4]);

/* Function: mesh_crc32
 * Compute the crc32 of the mesh data as an array of xyz rgba values.
 *
 * This is only used in the tests, to make sure that we can still open
 * old file formats.
 */
uint32_t mesh_crc32(const mesh_t *mesh);

#endif // MESH_UTILS_H
