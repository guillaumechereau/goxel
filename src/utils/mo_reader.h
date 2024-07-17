/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-Present Guillaume Chereau <guillaume@noctua-software.com>
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
 * Simple interface to read gettext mo files.
 */

#include <stdbool.h>

typedef struct mo_file mo_file_t;

mo_file_t *mo_open_from_data(void *data, int size, bool free_data);

void mo_close(mo_file_t *mo);

const char *mo_get(mo_file_t *mo, const char *str);
