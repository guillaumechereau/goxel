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
    int             size;
    const void      *data __attribute__((aligned(4)));
} asset_t;

static asset_t ASSETS[]; // Defined in assets.inl

const void *assets_get(const char *url, int *size)
{
    int i;
    if (str_startswith(url, "asset://")) url += 8; // Skip asset://
    for (i = 0; ASSETS[i].path; i++) {
        if (strcmp(ASSETS[i].path, url) == 0) {
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

static asset_t ASSETS[] = {
#include "assets/fonts.inl"
#include "assets/icons.inl"
#include "assets/images.inl"
#include "assets/other.inl"
#include "assets/palettes.inl"
#include "assets/progs.inl"
#include "assets/shaders.inl"
#include "assets/sounds.inl"
#include "assets/themes.inl"
#ifdef ASSETS_EXTRA
#   include ASSETS_EXTRA // Allow to add custom assets at build time.
#endif
{}, // NULL asset at the end of the list.
};
