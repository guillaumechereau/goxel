/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "path.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

char *path_dirname(const char *path, char *out, size_t size)
{
    char *sep;
    int len;

    assert(path);
    assert(path != out);
    sep = strrchr(path, '/');
#ifdef WIN32
    if (!sep) {
        sep = strrchr(path, '\\');
    }
#endif
    if (sep == NULL) {
        out[0] = '\0';
        return NULL;
    }
    len = (int)(sep - path);
    snprintf(out, size, "%.*s", len, path);
    return out;
}

char *path_basename(const char *path, char *out, size_t size)
{
    char *sep;
    int len;

    assert(path);
    assert(path != out);
    sep = strrchr(path, '/');
#ifdef WIN32
    if (!sep) {
        sep = strrchr(path, '\\');
    }
#endif
    if (sep == NULL) {
        snprintf(out, size, "%s", path);
        return out;
    }
    len = (int)(sep - path);
    snprintf(out, size, "%s", path + len + 1);
    return out;
}

bool path_normalize(char *path)
{
    int i;
    for (i = 0; i < strlen(path); i++) {
        if (path[i] == '\\') path[i] = '/';
    }
    return true;
}
