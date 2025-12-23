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
#include <string.h>

void gui_palette_panel(void)
{
    int nb, i, current = -1, last_used_current = -1;
    const palette_t *p;
    const char **names;
    gui_icon_info_t *grid;

    // Show last used colors section
    if (goxel.last_used_colors_count > 0) {
        gui_text("Last Used");
        
        gui_push_id("last_used_colors");
        gui_icon_info_t *last_used_grid = calloc(goxel.last_used_colors_count, sizeof(*last_used_grid));
        for (i = 0; i < goxel.last_used_colors_count; i++) {
            last_used_grid[i] = (gui_icon_info_t) {
                .label = goxel.last_used_colors[i].name,
                .icon = 0,
                .color = {VEC4_SPLIT(goxel.last_used_colors[i].color)},
            };
            if (memcmp(goxel.painter.color, goxel.last_used_colors[i].color, 4) == 0)
                last_used_current = i;
        }
        if (gui_icons_grid(goxel.last_used_colors_count, last_used_grid, &last_used_current)) {
            memcpy(goxel.painter.color, goxel.last_used_colors[last_used_current].color, 4);
            goxel_add_to_last_used_colors(goxel.painter.color, goxel.last_used_colors[last_used_current].name);
        }
        free(last_used_grid);
        gui_pop_id();
        
        gui_separator();
    }

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
        // Clear search when switching palettes
        goxel_clear_palette_search();
    }
    free(names);

    // Search box
    gui_push_id("palette_search");
    gui_text("Search");
    
    // Use a local static buffer (global buffer has issues)
    static char search_buffer[256] = "";
    
    char prev_query[256];
    strncpy(prev_query, search_buffer, sizeof(prev_query) - 1);
    prev_query[sizeof(prev_query) - 1] = '\0';
    
    bool changed = gui_input_text("##search", search_buffer, sizeof(search_buffer));
    if (changed) {
        // Copy to global buffer and filter
        strncpy(goxel.palette_search_query, search_buffer, sizeof(goxel.palette_search_query) - 1);
        goxel.palette_search_query[sizeof(goxel.palette_search_query) - 1] = '\0';
        goxel_filter_palette(search_buffer);
    }
    
    // Also filter if query is different from before (more reliable)
    if (strcmp(prev_query, search_buffer) != 0) {
        strncpy(goxel.palette_search_query, search_buffer, sizeof(goxel.palette_search_query) - 1);
        goxel.palette_search_query[sizeof(goxel.palette_search_query) - 1] = '\0';
        goxel_filter_palette(search_buffer);
    }
    gui_pop_id();

    gui_push_id("main_palette");
    
    // Determine which palette entries to show (filtered or full)
    palette_entry_t *entries_to_show;
    int entries_count;
    
    if (strlen(search_buffer) > 0) {
        // There's a search query - show filtered results (even if empty)
        entries_to_show = goxel.filtered_entries;
        entries_count = goxel.filtered_count;
    } else {
        // No search query - show full palette
        p = goxel.palette;
        entries_to_show = p->entries;
        entries_count = p->size;
    }
    
    // Create grid for display
    if (entries_count > 0) {
        grid = calloc(entries_count, sizeof(*grid));
        current = -1;
        for (i = 0; i < entries_count; i++) {
            grid[i] = (gui_icon_info_t) {
                .label = entries_to_show[i].name,
                .icon = 0,
                .color = {VEC4_SPLIT(entries_to_show[i].color)},
            };
            if (memcmp(goxel.painter.color, entries_to_show[i].color, 4) == 0)
                current = i;
        }
        
        if (gui_icons_grid(entries_count, grid, &current)) {
            memcpy(goxel.painter.color, entries_to_show[current].color, 4);
            goxel_add_to_last_used_colors(goxel.painter.color, entries_to_show[current].name);
        }
        free(grid);
    } else if (strlen(search_buffer) > 0) {
        // Show "no results" message when searching but no matches found
        gui_text("No matching colors found");
    }
    gui_pop_id();
}
