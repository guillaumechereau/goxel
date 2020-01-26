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

#include "goxel.h"

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
                   bool exact)
{
    int i;
    assert(exact); // For the moment.
    for (i = 0; i < palette->size; i++) {
        if (memcmp(col, palette->entries[i].color, 4) == 0)
            return i;
    }
    return -1;
}

void palette_insert(palette_t *p, const uint8_t col[4], const char *name)
{
    palette_entry_t *e;
    if (palette_search(p, col, true) != -1) return;
    if (p->allocated <= p->size) {
        p->allocated += 64;
        p->entries = realloc(p->entries, p->allocated * sizeof(*p->entries));
    }
    e = &p->entries[p->size];
    memset(e, 0, sizeof(*e));
    memcpy(e->color, col, 4);
    if (name)
        snprintf(e->name, sizeof(e->name), "%s", name);
    p->size++;
}


// Parse a gimp palette.
// XXX: we don't check for buffer overflow!
static int parse_gpl(const char *data, char *name, int *columns,
                     palette_entry_t *entries)
{
    const char *start, *end;
    int linen, r, g, b, nb = 0;
    char entry_name[128];

    for (linen = 1, start = data; *start; start = end + 1, linen++) {
        end = strchr(start, '\n');
        if (!end) end = start + strlen(start);

        if (name && sscanf(start, "Name: %[^\n]", name) == 1) {
            name = NULL;
            continue;
        }
        if (columns && sscanf(start, "Columns: %d", columns) == 1) {
            columns = NULL;
            continue;
        }

        if (sscanf(start, "%d %d %d %[^\n]", &r, &g, &b, entry_name) >= 3) {
            if (entries) {
                strcpy(entries[nb].name, entry_name);
                entries[nb].color[0] = r;
                entries[nb].color[1] = g;
                entries[nb].color[2] = b;
                entries[nb].color[3] = 255;
            }
            nb++;
        }
        if (!*end) break;
    }
    return nb;
}

/*
 * Function: parse_dat
 * Parse a Build engine (duke2d, blood...) palette
 */
static int parse_dat(const uint8_t *data, int len, palette_entry_t *entries)
{
    int i;
    if (len < 768) return -1;
    for (i = 0; i < 256; i++) {
        entries[i].color[0] = data[i * 3 + 0] * 4;
        entries[i].color[1] = data[i * 3 + 1] * 4;
        entries[i].color[2] = data[i * 3 + 2] * 4;
        entries[i].color[3] = 255;
    }
    return 256;
}

/*
 * Function: parse_png
 * Parse a png image into a palette.
 */
static int parse_png(const void *data, int len, palette_t *palette)
{
    int i, w, h, bpp = 3;
    uint8_t *img, color[4];
    img = img_read_from_mem((void*)data, len, &w, &h, &bpp);
    if (!img) return -1;

    for (i = 0; i < w * h; i++) {
        memcpy(color, (uint8_t[]){0, 0, 0, 255}, 4);
        memcpy(color, img + i * bpp, bpp);
        palette_insert(palette, color, NULL);
    }
    free(img);
    return palette->size;
}


static int on_palette(int i, const char *path, void *user)
{
    palette_t **list = user;
    const char *data;
    palette_t *pal;
    pal = calloc(1, sizeof(*pal));
    data = assets_get(path, NULL);
    pal->size = parse_gpl(data, pal->name, &pal->columns, NULL);
    pal->entries = calloc(pal->size, sizeof(*pal->entries));
    parse_gpl(data, NULL, NULL, pal->entries);
    DL_APPEND(*list, pal);
    return 0;
}

static int on_palette2(const char *dir, const char *name, void *user)
{
    palette_t **list = user;
    char *data, *path;
    int size, err = 0;
    palette_t *pal;

    if (    !str_endswith(name, ".gpl") &&
            !str_endswith(name, ".dat") &&
            !str_endswith(name, ".png"))
        return 0;

    asprintf(&path, "%s/%s", dir, name);
    pal = calloc(1, sizeof(*pal));
    data = read_file(path, &size);
    if (str_endswith(name, ".gpl")) {
        pal->size = parse_gpl(data, pal->name, &pal->columns, NULL);
        pal->entries = calloc(pal->size, sizeof(*pal->entries));
        err = parse_gpl(data, NULL, NULL, pal->entries);
    }
    else if (str_endswith(name, ".dat")) {
        snprintf(pal->name, sizeof(pal->name), "%s", name);
        pal->size = 256;
        pal->entries = calloc(pal->size, sizeof(*pal->entries));
        err = parse_dat((void*)data, size, pal->entries);
    }
    else if (str_endswith(name, ".png")) {
        snprintf(pal->name, sizeof(pal->name), "%s", name);
        err = parse_png(data, size, pal);
    }

    if (err < 0) {
        LOG_E("Cannot parse palette %s", path);
        free(pal);
        goto end;
    }

    DL_APPEND(*list, pal);
end:
    free(path);
    free(data);
    return 0;
}


void palette_load_all(palette_t **list)
{
    char *dir;
    assets_list("data/palettes/", list, on_palette);
    if (sys_get_user_dir()) {
        asprintf(&dir, "%s/palettes", sys_get_user_dir());
        sys_list_dir(dir, on_palette2, list);
        free(dir);
    }
}
