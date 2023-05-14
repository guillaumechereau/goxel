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

#include "color.h"

#include <math.h>

void srgba8_to_rgba(const uint8_t srgba[4], float rgba[4])
{
    // https://en.wikipedia.org/wiki/SRGB
    float c;
    int i;
    for (i = 0; i < 3; i++) {
        c = srgba[i] / 255.f;
        c = (c <= 0.04045f) ? (c / 12.92f) : pow((c + 0.055) / 1.055, 2.4);
        rgba[i] = c;
    }
    rgba[3] = srgba[3] / 255.f;
}
