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

#include "utils/texture.h"

enum {
    PT_WORLD_NONE = 0,
    PT_WORLD_UNIFORM,
    PT_WORLD_SKY,
};

enum {
    PT_FLOOR_NONE = 0,
    PT_FLOOR_PLANE,
};

enum {
    PT_STOPPED = 0,
    PT_RUNNING,
    PT_FINISHED,
};

typedef struct pathtracer_internal pathtracer_internal_t;

// Hold info about the cycles rendering task.
typedef struct {
    int status;
    uint8_t *buf;       // RGBA buffer.
    int w, h;           // Size of the buffer.
    float progress;
    bool force_restart;
    texture_t *texture;
    pathtracer_internal_t *p;
    int num_samples;
    struct {
        int type;
        float energy;
        uint8_t color[4];
    } world;
    struct {
        int type;
        uint8_t color[4];
        int size[2];
        material_t *material;
    } floor;
} pathtracer_t;

/*
 * Function: pathtracer_iter
 * Iter the rendering process of the current mesh.
 *
 * Parameters:
 *   pt       - A pathtracer instance.
 *   viewport - The full view viewport.
 */
void pathtracer_iter(pathtracer_t *pt, const float viewport[4]);

/*
 * Stop the pathtracer thread if it is running.
 */
void pathtracer_stop(pathtracer_t *pt);
