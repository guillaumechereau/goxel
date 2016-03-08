/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

typedef struct {
    const char      *path;
    const uint8_t   *data;
    int             size;
} asset_t;

static asset_t ASSETS[]; // Defined in assets.inl

const void *assets_get(const char *url, int *size)
{
    int i;
    for (i = 0; ASSETS[i].path; i++) {
        if (str_equ(ASSETS[i].path, url)) {
            if (size) *size = ASSETS[i].size;
            return ASSETS[i].data;
        }
    }
    return NULL;
}

int assets_list(const char *url, void *user,
                int (*f)(int i, const char *path, void *user))
{
    int i, j = 0;
    for (i = 0; ASSETS[i].path; i++) {
        if (str_startswith(ASSETS[i].path, url)) {
            if (!f || f(j, ASSETS[i].path, user) == 0) j++;
        }
    }
    return j;
}


#include "assets.inl"
