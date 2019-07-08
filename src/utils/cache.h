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

#ifndef CACHE_H
#define CACHE_H

// Generic data cache structure.

// Allow to cache blocks merge operations.
typedef struct cache cache_t;

/*
 * Function: cache_create
 * Create a new cache with a given max size (in byte).
 */
cache_t *cache_create(int size);

/*
 * Function: cache_add
 * Add an item into the cache.
 *
 * Parameters:
 *   cache      - A cache_t instance.
 *   key        - Unique key data for the cache item.
 *   keylen     - Size of the key data.
 *  data        - Pointer to the item data.  The cache takes ownership.
 *  cost        - Cost of the data used to compute the cache usage.
 *                It doesn't have to be the size.
 *  delfunc     - Function that the cache can use to free the data.
 */
void cache_add(cache_t *cache, const void *key, int keylen, void *data,
               int cost, int (*delfunc)(void *data));

/*
 * Function: cache_get
 * Retreive an item from the cache.
 *
 * Parameters:
 *   cache      - A cache_t instance.
 *   key        - Unique key data for the item.
 *   keylen     - Sizeo of the key data.
 *
 * Returns:
 *   The data owned by the cache, or NULL if no item with this key is in
 *   the cache.
 */
void *cache_get(cache_t *cache, const void *key, int keylen);

/*
 * Function: cache_clear
 * Delete all the cached items.
 */
void cache_clear(cache_t *cache);

/*
 * Function: cache_delete
 * Delete a cache.
 */
void cache_delete(cache_t *cache);


#endif // CACHE_H
