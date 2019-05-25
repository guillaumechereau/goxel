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
#include <string.h>

static float min3(float x, float y, float z)
{
    if (y < x) x = y;
    if (z < x) x = z;
    return x;
}

static float max3(float x, float y, float z)
{
    if (y > x) x = y;
    if (z > x) x = z;
    return x;
}

void hsl_to_rgb_f(const float hsl[3], float rgb[3])
{
    float r = 0, g = 0, b = 0, c, x, m;
    const float h = hsl[0] / 60, s = hsl[1], l = hsl[2];
    c = (1 - fabs(2 * l - 1)) * s;
    x = c * (1 - fabs(fmod(h, 2) - 1));
    if      (h < 1) {r = c; g = x; b = 0;}
    else if (h < 2) {r = x; g = c; b = 0;}
    else if (h < 3) {r = 0; g = c; b = x;}
    else if (h < 4) {r = 0; g = x; b = c;}
    else if (h < 5) {r = x; g = 0; b = c;}
    else if (h < 6) {r = c; g = 0; b = x;}
    m = l - 0.5 * c;
    rgb[0] = r + m;
    rgb[1] = g + m;
    rgb[2] = b + m;
}

void rgb_to_hsl_f(const float rgb[3], float hsl[3])
{
    float h = 0, s, v, m, c, l;
    const float r = rgb[0], g = rgb[1], b = rgb[2];
    v = max3(r, g, b);
    m = min3(r, g, b);
    l = (v + m) / 2;
    c = v - m;
    if (c == 0) {
        hsl[0] = 0;
        hsl[1] = 0;
        hsl[2] = l;
        return;
    }
    if      (v == r) {h = (g - b) / c + (g < b ? 6 : 0);}
    else if (v == g) {h = (b - r) / c + 2;}
    else if (v == b) {h = (r - g) / c + 4;}
    h *= 60;
    s = (l > 0.5) ? c / (2 - v - m) : c / (v + m);
    hsl[0] = h;
    hsl[1] = s;
    hsl[2] = l;
}

void hsl_to_rgb(const uint8_t hsl[3], uint8_t rgb[3])
{
    // XXX: use an optimized function that use int operations.
    float hsl_f[3] = {hsl[0] / 255. * 360, hsl[1] / 255., hsl[2] / 255.};
    float rgb_f[3];
    hsl_to_rgb_f(hsl_f, rgb_f);
    rgb[0] = round(rgb_f[0] * 255);
    rgb[1] = round(rgb_f[1] * 255);
    rgb[2] = round(rgb_f[2] * 255);
}

void rgb_to_hsl(const uint8_t rgb[3], uint8_t hsl[3])
{
    // XXX: use an optimized function that use int operations.
    float rgb_f[3] = {rgb[0] / 255., rgb[1] / 255., rgb[2] / 255.};
    float hsl_f[3];
    rgb_to_hsl_f(rgb_f, hsl_f);
    hsl[0] = round(hsl_f[0] * 255 / 360);
    hsl[1] = round(hsl_f[1] * 255);
    hsl[2] = round(hsl_f[2] * 255);
}
