/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

static theme_t g_theme = {
    .name = "default",
    .sizes = {
        .item_height = 18,
        .item_padding_h = 4,
        .item_rounding = 4,
        .item_spacing_h = 4,
        .item_spacing_v = 4,
        .item_inner_spacing_h = 6,
    },
    .colors = {
        .background = VEC4(96, 114, 114, 255),
        .outline = VEC4(77, 77, 77, 255),
        .inner = VEC4(161, 161, 161, 255),
        .text = VEC4(17, 17, 17, 255),
        .inner_selected = VEC4(102, 102, 204, 255),
        .text_selected = VEC4(17, 17, 17, 255),
        .tabs_background = VEC4(48, 66, 66, 255),
        .tabs = VEC4(69, 86, 86, 255),
    },
};

theme_t *theme_get(void)
{
    return &g_theme;
}
