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

#ifndef SHADER_CACHE_H
#define SHADER_CACHE_H

#include "goxel.h"

typedef struct {
    const char  *name;
    bool        set;
} shader_define_t;

/*
 * Function: shader_get
 * Retreive a cached shader
 *
 * Probably need to change this api soon.
 *
 * Properties:
 *   name       - Name of one of the shaders in the resources.
 *   defines    - Array of <shader_define_t>, terminated by an empty one.
 *                Can be NULL.
 *   attr_names - NULL terminated list of attribute names that will be binded.
 *   on_created - If set, called the first time the shader has been created.
 */
gl_shader_t *shader_get(const char *name, const shader_define_t *defines,
                        const char **attr_names,
                        void (*on_created)(gl_shader_t *s));

/*
 * Function: shaders_release_all
 * Remove all the shaders from the cache.
 */
void shaders_release_all(void);

#endif // SHADER_CACHE
