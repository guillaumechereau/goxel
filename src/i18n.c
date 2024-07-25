/* Goxel 3D voxels editor
 *
 * copyright (c) 2015-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "i18n.h"

#include "goxel.h"
#include "utils/mo_reader.h"

static const tr_lang_t LANGUAGES[] = {
    { "en", "English" },
    { "fr", "Français" },
    {},
};

static int current_lang_idx = 0;
static mo_file_t *mo_file = NULL;

const tr_lang_t *tr_get_supported_languages(void)
{
    return LANGUAGES;
}

void tr_set_language(const char *id)
{
    int i, size;
    const void *data;
    char uri[128];

    if (strcmp(LANGUAGES[current_lang_idx].id, id) == 0) return;
    for (i = 0; i < ARRAY_SIZE(LANGUAGES); i++) {
        if (strcmp(LANGUAGES[i].id, id) == 0) {
            LOG_D("Set lang '%s'", LANGUAGES[i].id);
            mo_close(mo_file);
            mo_file = NULL;
            current_lang_idx = i;
            if (i == 0) { // English: no need for mo file.
                break;
            }
            snprintf(uri, sizeof(uri), "asset://data/locale/%s.mo",
                     LANGUAGES[i].id);
            data = assets_get(uri, &size);
            assert(data);
            mo_file = mo_open_from_data((void *)data, size, false);
            break;
        }
    }
}

const tr_lang_t *tr_get_language(void)
{
    return &LANGUAGES[current_lang_idx];
}

const char *tr(const char *str)
{
    if (mo_file == NULL) {
        return str;
    }
    return mo_get(mo_file, str) ?: str;
}
