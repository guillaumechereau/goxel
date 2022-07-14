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

