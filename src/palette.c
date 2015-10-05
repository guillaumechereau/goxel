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

#include "goxel.h"

static const uvec4b_t TANGO[] = {
    IVEC(0,     0,   0, 255),
    IVEC(252, 233,  79, 255),
    IVEC(237, 212,   0, 255),
    IVEC(196, 160,   0, 255),
    IVEC(138, 226,  52, 255),
    IVEC(115, 210,  22, 255),
    IVEC( 78, 154,   6, 255),
    IVEC(252, 175,  62, 255),
    IVEC(245, 121,   0, 255),
    IVEC(206 , 92,   0, 255),
    IVEC(114, 159, 207, 255),
    IVEC( 52, 101, 164, 255),
    IVEC( 32 , 74, 135, 255),
    IVEC(173, 127, 168, 255),
    IVEC(117 , 80, 123, 255),
    IVEC( 92 , 53, 102, 255),
    IVEC(233, 185, 110, 255),
    IVEC(193, 125,  17, 255),
    IVEC(143 , 89,   2, 255),
    IVEC(239 , 41,  41, 255),
    IVEC(204  , 0,   0, 255),
    IVEC(164  , 0,   0, 255),
    IVEC(238, 238, 236, 255),
    IVEC(211, 215, 207, 255),
    IVEC(186, 189, 182, 255),
    IVEC(136, 138, 133, 255),
    IVEC( 85 , 87,  83, 255),
    IVEC( 46 , 52,  54, 255),
};


palette_t *palette_get()
{
    static palette_t *ret = NULL;
    int i;
    if (!ret) {
        ret = calloc(1, sizeof(*ret));
        ret->size = ARRAY_SIZE(TANGO);
        ret->values = calloc(ret->size, sizeof(*ret->values));
        for (i = 0; i < ret->size; i++) {
            ret->values[i] = TANGO[i];
        }
    }
    return ret;
}
