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

/*
 * Convert from sRGB uint8 to linear RGB float.
 */
void srgb8_to_rgb(const uint8_t srgba[3], float rgba[3]);

/*
 * Convert from linear RGB float to sRGB uint8.
 */
void rgb_to_srgb8(const float rgb[3], uint8_t srgb[3]);

/*
 * Convert from sRGBA uint8 to linear RGBA float.
 */
void srgba8_to_rgba(const uint8_t srgba[4], float rgba[4]);

#endif // COLOR_H
