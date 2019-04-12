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

/*
 * Function: b64_decode
 * Decode a base64 string
 *
 * Parameters:
 *   src  - A base 64 encoded string.
 *   dest - Buffer that will receive the decoded value or NULL.  If set to
 *          NULL the function just returns the size of the decoded data.
 *
 * Return:
 *   The size of the decoded data.
 */
int b64_decode(const char *src, void *dest);
