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
 * MODE_INTERSECT_FILL - Like intersect but use the color of the source.
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
    MODE_INTERSECT_FILL,
    MODE_MULT_ALPHA,
    MODE_REPLACE,
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
void volume_get_box(const volume_t *volume, bool exact, float box[4][4]);

/* Function: volume_op
 * Apply a paint operation to a volume.
 * This function render geometrical 3d shapes into a volume.
 * The shape, mode and color are defined in the painter argument.
 *
 * Parameters:
 *   volume    - The volume we paint into.
 *   painter - Defines the paint operation to apply.
 *   box     - Defines the position and size of the shape as the
 *             transformation matrix from the zero centered unit box.
 *
 * See Also:
 *   <painter_t>
 */
void volume_op(volume_t *volume, const painter_t *painter,
               const float box[4][4]);

// XXX: to cleanup.
void volume_extrude(volume_t *volume,
                  const float plane[4][4],
                  const float box[4][4]);

/* Function: volume_blit
 *
 * Blit voxel data into a volume.
 * This is the fastest way to quickly put data into a volume.
 *
 * Parameters:
 *   volume - The volume we blit into.
 *   data - Pointer to voxel data (RGBA values, in xyz order).
 *   x    - X pos.
 *   y    - Y pos.
 *   z    - Z pos.
 *   w    - Width of the data.
 *   h    - Height of the data.
 *   d    - Depth of the data.
 *   iter - Optional iterator for optimized access.
 */
void volume_blit(volume_t *volume, const uint8_t *data,
               int x, int y, int z, int w, int h, int d,
               volume_iterator_t *iter);

void volume_move(volume_t *volume, const float mat[4][4]);

void volume_shift_alpha(volume_t *volume, int v);

// Compute the selection mask for a given condition.
int volume_select(const volume_t *volume,
                const int start_pos[3],
                int (*cond)(void *user, const volume_t *volume,
                            const int base_pos[3],
                            const int new_pos[3],
                            volume_accessor_t *volume_accessor),
                void *user, volume_t *selection);

/*
 * Function: volume_merge
 * Merge a volume into an other using a given blending function.
 *
 * Parameters:
 *   volume   - The destination volume we merge into.
 *   other  - The source volume we merge.  Unchanged by this function.
 *   mode   - The blending function used.  One of the <MODE> enum values.
 *   color  - A color to apply to the source volume before merging.  Can be
 *            set to NULL.
 */
void volume_merge(volume_t *volume, const volume_t *other, int mode,
                const uint8_t color[4]);

/*
 * Function: volume_generate_vertices
 * Generate a vertice array for rendering a volume block.
 *
 * Parameters:
 *   volume       - Input volume.
 *   block_pos  - Position of the volume block to render.
 *   effects    - Effect flags.
 *   out        - Output array.
 *   size       - Output the size of a single face.
 *                4 for quads and 3 for triangles.  Normal volume uses quad
 *                but marching cube effect return triangle arrays.
 *   subdivide  - Ouput the number of subdivisions used for a voxel.  Normal
 *                render uses 1 unit per voxel, but marching cube rendering
 *                can use more.
 */
int volume_generate_vertices(const volume_t *volume, const int block_pos[3],
                           int effects, voxel_vertex_t *out,
                           int *size, int *subdivide);

// XXX: use int[2][3] for the box?
void volume_crop(volume_t *volume, const float box[4][4]);

/* Function: volume_crc32
 * Compute the crc32 of the volume data as an array of xyz rgba values.
 *
 * This is only used in the tests, to make sure that we can still open
 * old file formats.
 */
uint32_t volume_crc32(const volume_t *volume);

#endif // VOLUME_UTILS_H
