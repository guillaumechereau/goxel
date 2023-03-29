/* Goxel 3D voxels editor
 *
 * copyright (c) 2022-present Guillaume Chereau <guillaume@noctua-software.com>
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

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define CONFIG_VERSION "2021-03-27"

#include "../ext_src/quickjs/cutils.c"
#include "../ext_src/quickjs/libregexp.c"
#include "../ext_src/quickjs/libunicode.c"
#include "../ext_src/quickjs/quickjs-libc.c"
#include "../ext_src/quickjs/repl.c"
#define is_digit is_digit2
#define compute_stack_size compute_stack_size2
#include "../ext_src/quickjs/quickjs.c"
