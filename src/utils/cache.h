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
