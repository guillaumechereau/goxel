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

#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

void hsl_to_rgb(const uint8_t hsl[3], uint8_t rgb[3]);
void rgb_to_hsl(const uint8_t rgb[3], uint8_t hsl[3]);

void hsl_to_rgb_f(const float hsl[3], float rgb[3]);
void rgb_to_hsl_f(const float rgb[3], float hsl[3]);

#endif // COLOR_H
