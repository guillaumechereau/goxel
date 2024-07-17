/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-Present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "mo_reader.h"
#include "stb_ds.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char *key;
    char *value;
} entry_t;

struct mo_file {
    void *data;
    entry_t *entries;
    bool free_data;
};

typedef struct {
    int32_t magic;
    int32_t version;
    int32_t count;
    int32_t offset_original;
    int32_t offset_translation;
    int32_t hashtable_size;
    int32_t offset_hashtable;
} header_t;

mo_file_t *mo_open_from_data(void *data, int size, bool free_data)
{

    mo_file_t *mo;
    header_t *header;
    int i, or_len, or_ofs, tr_len, tr_ofs;

    mo = calloc(1, sizeof(*mo));
    mo->data = data;
    mo->free_data = free_data;
    if (!data) return mo;

    header = data;
    assert(header->magic == 0x950412DE);

    (void)or_len;
    (void)tr_len;
    for (i = 0; i < header->count; i++) {
        or_len = *(uint32_t *)(data + header->offset_original + i * 8 + 0);
        or_ofs = *(uint32_t *)(data + header->offset_original + i * 8 + 4);
        tr_len = *(uint32_t *)(data + header->offset_translation + i * 8 + 0);
        tr_ofs = *(uint32_t *)(data + header->offset_translation + i * 8 + 4);
        if (tr_len == 0) continue;
        assert(*((char *)(data + or_ofs + or_len)) == '\0');
        assert(*((char *)(data + tr_ofs + tr_len)) == '\0');
        shput(mo->entries, (char *)data + or_ofs, (char *)data + tr_ofs);
    }

    return mo;
}

void mo_close(mo_file_t *mo)
{
    if (!mo) return;
    shfree(mo->entries);
    if (mo->free_data) {
        free(mo->data);
    }
    free(mo);
}

const char *mo_get(mo_file_t *mo, const char *str)
{
    if (!mo->data) return NULL;
    return shget(mo->entries, str);
}
