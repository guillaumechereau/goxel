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

// Basic mustache templates support
// Check povray.c for an example of usage.

#ifndef MUSTACHE_H
#define MUSTACHE_H

typedef struct mustache mustache_t;
mustache_t *mustache_root(void);
mustache_t *mustache_add_dict(mustache_t *m, const char *key);
mustache_t *mustache_add_list(mustache_t *m, const char *key);
void mustache_add_str(mustache_t *m, const char *key, const char *fmt, ...);
int mustache_render(const mustache_t *m, const char *templ, char *out);
void mustache_free(mustache_t *m);

#endif // MUSTACHE_H
