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

// After we reach this size, we start to remove old items.
#define MAX_SIZE 512

typedef struct item item_t;
struct item {
    UT_hash_handle  hh;
    char            key[32];
    block_data_t    *data;
    uint64_t        last_used;
};

static item_t *g_items = NULL;
static uint64_t g_clock = 0;
static int g_size = 0;

static int sort_cmp(void *a, void *b)
{
    return sign(((item_t*)a)->last_used - ((item_t*)b)->last_used);
}

static void cleanup(void)
{
    item_t *item;
    HASH_SORT(g_items, sort_cmp);
    while (g_size >= MAX_SIZE) {
        item = g_items;
        HASH_DEL(g_items, item);
        item->data->ref--;
        if (item->data->ref == 0) {
            free(item->data);
            goxel->block_count--;
        }
        free(item);
        g_size--;
    }
}

void cache_add(const void *key, int len, block_data_t *data)
{
    item_t *item = calloc(1, sizeof(*item));
    assert(len <= sizeof(item->key));
    memcpy(item->key, key, len);
    item->data = data;
    item->last_used = g_clock++;
    data->ref++;
    HASH_ADD(hh, g_items, key, len, item);
    g_size++;
    if (g_size >= MAX_SIZE) cleanup();
}

block_data_t *cache_get(const void *key, int len)
{
    item_t *item;
    HASH_FIND(hh, g_items, key, len, item);
    if (!item) return NULL;
    item->last_used = g_clock++;
    return item->data;
}
