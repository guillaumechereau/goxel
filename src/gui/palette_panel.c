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

#include "goxel.h"

void gui_palette_panel(void)
{
    int nb, i, current = -1;
    const palette_t *p;
    const char **names;
    gui_icon_info_t *grid;

    DL_COUNT(goxel.palettes, p, nb);
    names = (const char**)calloc(nb, sizeof(*names));

    i = 0;
    DL_FOREACH(goxel.palettes, p) {
        if (p == goxel.palette) current = i;
        names[i++] = p->name;
    }
    if (gui_combo("##palettes", &current, names, nb)) {
        goxel.palette = goxel.palettes;
        for (i = 0; i < current; i++) goxel.palette = goxel.palette->next;
    }
    free(names);

    p = goxel.palette;
    grid = calloc(p->size, sizeof(*grid));
    for (i = 0; i < p->size; i++) {
        grid[i] = (gui_icon_info_t) {
            .label = p->entries[i].name,
            .icon = 0,
            .color = {VEC4_SPLIT(p->entries[i].color)},
        };
        if (memcmp(goxel.painter.color, p->entries[i].color, 4) == 0)
            current = i;
    }
    if (gui_icons_grid(p->size, grid, &current)) {
        memcpy(goxel.painter.color, p->entries[current].color, 4);
    }
    free(grid);
}
