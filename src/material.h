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

#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>

typedef struct material material_t;
struct material {
    char  name[128];  // 127 chars max.
    float metallic;
    float roughness;
    float base_color[4];
    float emission[3];
    material_t *next, *prev; // List of materials in an image.
};

#define MATERIAL_DEFAULT (material_t){ \
    .name = {}, \
    .metallic = 0.2, \
    .roughness = 0.5, \
    .base_color = {1, 1, 1, 1}}

material_t *material_new(const char *name);
void material_delete(material_t *m);
material_t *material_copy(const material_t *mat);
uint32_t material_get_hash(const material_t *m);

#endif // MATERIAL_H
