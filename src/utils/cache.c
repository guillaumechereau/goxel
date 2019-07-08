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

#include "cache.h"
#include "uthash.h"

#include <assert.h>
#include <stdint.h>

typedef struct item item_t;
struct item {
    UT_hash_handle  hh;
    char            key[256];
    void            *data;
    int             cost;
    uint64_t        last_used;
    int             (*delfunc)(void *data);
};

struct cache {
    item_t *items;
    uint64_t clock;
    int size;
    int max_size;
};

cache_t *cache_create(int size)
{
    cache_t *cache = calloc(1, sizeof(*cache));
    cache->max_size = size;
    return cache;
}

static void cleanup(cache_t *cache)
{
    item_t *item;
    while (cache->size >= cache->max_size) {
        assert(cache->items);
        item = cache->items;
        HASH_DEL(cache->items, item);
        assert(item != cache->items);
        item->delfunc(item->data);
        cache->size -= item->cost;
        free(item);
    }
}

void cache_add(cache_t *cache, const void *key, int len, void *data,
               int cost, int (*delfunc)(void *data))
{
    item_t *item = calloc(1, sizeof(*item));
    assert(len <= sizeof(item->key));
    memcpy(item->key, key, len);
    item->data = data;
    item->cost = cost;
    item->last_used = cache->clock++;
    item->delfunc = delfunc;
    HASH_ADD(hh, cache->items, key, len, item);
    cache->size += cost;
    if (cache->size >= cache->max_size) cleanup(cache);
}

void *cache_get(cache_t *cache, const void *key, int keylen)
{
    item_t *item;
    HASH_FIND(hh, cache->items, key, keylen, item);
    if (!item) return NULL;
    item->last_used = cache->clock++;
    // Reinsert item on top of the hash list so that it stays sorted.
    HASH_DEL(cache->items, item);
    HASH_ADD(hh, cache->items, key, keylen, item);
    return item->data;
}

void cache_clear(cache_t *cache)
{
    item_t *item;
    while ((item = cache->items)) {
        HASH_DEL(cache->items, item);
        item->delfunc(item->data);
        cache->size -= item->cost;
        free(item);
    }
    assert(cache->size == 0);
}

/*
 * Function: cache_delete
 * Delete a cache.
 */
void cache_delete(cache_t *cache)
{
    cache_clear(cache);
    free(cache);
}
