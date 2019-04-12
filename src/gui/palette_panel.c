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

#ifndef GUI_PALETTE_COLUMNS_NB
#   define GUI_PALETTE_COLUMNS_NB 8
#endif

void gui_palette_panel(void)
{
    palette_t *p;
    int i, current, nb = 0, nb_col = GUI_PALETTE_COLUMNS_NB;
    const char **names;
    char id[128];

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

    for (i = 0; i < p->size; i++) {
        snprintf(id, sizeof(id), "%d", i);
        gui_push_id(id);
        gui_palette_entry(p->entries[i].color, goxel.painter.color);
        if ((i + 1) % nb_col && i != p->size - 1) gui_same_line();
        gui_pop_id();
    }
}

