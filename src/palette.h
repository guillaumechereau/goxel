// XXX: probably need to redo the code here.

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t  color[4];
    char     name[32];
} palette_entry_t;

typedef struct palette palette_t;
struct palette {
    palette_t *next, *prev; // For the global list of palettes.
    char    name[128];
    int     columns;
    int     size;
    int     allocated;
    palette_entry_t *entries;
};

// Load all the available palettes into a list.
void palette_load_all(palette_t **list);

/*
 * Function: palette_search
 * Search a given color in a palette
 *
 * Parameters:
 *   palette    - A palette.
 *   col        - The color we are looking for.
 *   exact      - If set to true, return -1 if no color is found, else
 *                return the closest color.
 *
 * Return:
 *   The index of the color in the palette.
 */
int palette_search(const palette_t *palette, const uint8_t col[4],
                   bool exact);

void palette_insert(palette_t *p, const uint8_t col[4], const char *name);
