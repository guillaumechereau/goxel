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

#ifndef VOLUME_H
#define VOLUME_H

#include <stdbool.h>
#include <stdint.h>

#define TILE_SIZE 16

/* Type: volume_t
 * Opaque type that represents a voxel volume.
 */
typedef struct volume volume_t;

typedef struct tile tile_t;

/* Enum: VOLUME_ITER
 * Some flags that can be used to modify the behavior of the iteration
 * function.
 *
 * VOLUME_ITER_VOXELS - Iter on the voxels (default if zero).
 * VOLUME_ITER_TILES - Iter on the tiles: the iterator return successive
 *                    tiles positions.
 * VOLUME_ITER_INCLUDES_NEIGHBORS - Also yield one position for each
 *                                neighbor of the voxels.
 * VOLUME_ITER_SKIP_EMPTY - Don't yield empty voxels/tiles.
 */
enum {
    VOLUME_ITER_VOXELS                = 1 << 0,
    VOLUME_ITER_TILES                 = 1 << 1,
    VOLUME_ITER_INCLUDES_NEIGHBORS    = 1 << 2,
    VOLUME_ITER_SKIP_EMPTY            = 1 << 3,
};

/* Type volume
 * Fast iterator of all the volume voxels.
 *
 * This struct can be used when we want to make a lot of successive accesses
 * to the same volume.
 *
 * It's also the struct used as an iterator into the volume voxels.
 *
 * You don't need to care about the attributes, just create them with
 * <volume_get_accessor>, <volume_get_iterator>, <volume_get_box_iterator> or
 * <volume_get_union_iterator>.
 */
typedef struct {
    const volume_t *volume;
    const volume_t *volume2;
    // Current cached tile and its position.
    // the tile can be NULL if there is no tile at this position.
    tile_t *tile;
    int tile_pos[3];
    uint64_t tile_id;

    int pos[3];
    float box[4][4];
    int bbox[2][3];

    int flags;
} volume_iterator_t;
typedef volume_iterator_t volume_accessor_t;

/*
 * Function: volume_new
 * Create a new volume.
 */
volume_t *volume_new(void);

/*
 * Function: volume_delete
 * Delete a volume.
 */
void volume_delete(volume_t *volume);

/* Function: volume_clear
 *
 * Clear all the voxels from a volume.
 */
void volume_clear(volume_t *volume);

/*
 * Function: volume_get_tile_aabb
 * Compute the AABB box of a given tile of the volume.
 */
static inline void volume_get_tile_aabb(const int pos[3], int aabb[2][3])
{
    aabb[0][0] = pos[0];
    aabb[0][1] = pos[1];
    aabb[0][2] = pos[2];
    aabb[1][0] = pos[0] + TILE_SIZE;
    aabb[1][1] = pos[1] + TILE_SIZE;
    aabb[1][2] = pos[2] + TILE_SIZE;
}

volume_t *volume_dup(const volume_t *volume);

volume_t *volume_copy(const volume_t *volume);

void volume_set(volume_t *volume, const volume_t *other);

volume_accessor_t volume_get_accessor(const volume_t *volume);
void volume_get_at(const volume_t *volume, volume_iterator_t *it,
                 const int pos[3], uint8_t out[4]);
uint8_t volume_get_alpha_at(const volume_t *volume, volume_iterator_t *it,
                          const int pos[3]);

/*
 * Function: volume_set_at
 *
 * Set a single voxel value in a volume.
 *
 * Inputs:
 *   volume - The volume.
 *   it   - Optional volume iterator.  Successive access to the same volume
 *          using the iterator are optimized.
 *   pos  - Position of the voxel.
 *   v    - Value to set.
 */
void volume_set_at(volume_t *volume, volume_iterator_t *it,
                 const int pos[3], const uint8_t v[4]);

// XXX: we should remove this one I guess.
void volume_remove_empty_tiles(volume_t *volume, bool fast);

/*
 * Function: volume_clear_tile
 * Set to zero all the voxels in a given tile.
 */
void volume_clear_tile(volume_t *volume, volume_iterator_t *it,
                        const int pos[3]);

/*
 * Function: volume_is_empty
 *
 * Test whether a volume has no voxel.
 *
 * Returns:
 *   true if the volume is empty, false otherwise.
 */
bool volume_is_empty(const volume_t *volume);

/*
 * Function: volume_get_bbox
 *
 * Get the bounding box of a volume.
 *
 * Inputs:
 *   volume   - The volume
 *   exact  - If true, compute the exact bounding box.  If false, returns
 *            an approximation that might be slightly bigger than the
 *            actual box, but faster to compute.
 *
 * Outputs:
 *   bbox  - The bounding box as the bottom left and top right corner of
 *           the volume.  If the volume is empty, this will contain all zero.
 *
 * Returns:
 *   true if the volume is not empty.
 */
bool volume_get_bbox(const volume_t *volume, int bbox[2][3], bool exact);

/*
 * Function: volume_get_iterator
 * Return an iterator that yield all the voxels of the volume.
 *
 * Parameters:
 *   volume  - The volume we want to iterate.
 *   flags - Any of the <VOLUME_ITER> enum.
 */
volume_iterator_t volume_get_iterator(const volume_t *volume, int flags);

// Return an iterator that follow a given box shape.
volume_iterator_t volume_get_box_iterator(const volume_t *volume,
                                      const float box[4][4],
                                      int flags);

volume_iterator_t volume_get_union_iterator(
        const volume_t *m1, const volume_t *m2, int flags);

int volume_iter(volume_iterator_t *it, int pos[3]);

/*
 * Function: volume_get_key
 *
 * Return a value that is guarantied to be different for different volumes.
 *
 * This key can be used for quickly testing if two volumes are the same.
 *
 * Note that two volumes with the same key are guarantied to have the same
 * content, but two volumes with different key could still have the same
 * content: this is not an actual hash!
 *
 * Inputs:
 *   volume - The volume..
 *
 * Return:
 *   The key, if the volume input is NULL, returns zero.
 *
 */
uint64_t volume_get_key(const volume_t *volume);

void *volume_get_tile_data(const volume_t *volume, volume_accessor_t *accessor,
                           const int bpos[3], uint64_t *id);

// Maybe replace this with a generic volume_copy_part function?
void volume_copy_tile(const volume_t *src, const int src_pos[3],
                      volume_t *dst, const int dst_pos[3]);

void volume_read(const volume_t *volume,
                 const int pos[3], const int size[3],
                 uint8_t *data);

int volume_get_tiles_count(const volume_t *volume);

typedef struct {
    int       nb_volumes;
    int       nb_tiles;
    uint64_t  mem;
} volume_global_stats_t;

void volume_get_global_stats(volume_global_stats_t *stats);

/*
 * Function: volume_copy_to_string
 *
 * Returns a string a box within this volume. The returned string is on the heap and owned
 * by the caller, you must call free() on it later.
 *
 * Inputs:
 *   volume - The volume.
 *   aabb - The bounding box within the volume to write to the string.
 *
 * Return:
 *   The heap-allocated encoded string.
 */
char* volume_copy_to_string(const volume_t *volume, const int aabb[2][3]);

/*
 * Function: volume_parse_string_header
 *
 * Parses a voxel encoding string header, as returned by volume_copy_to_string(). 
 *
 * Inputs:
 *   encoded_str - The string with encoded voxels.
 *   size - If parsed successfully, the size will be written to this array.
 *
 * Return:
 *   The beginning of the hex-encoded voxels if the header is valid,
 *   NULL otherwise.
 */
const char* volume_parse_string_header(const char *encoded_str, int size[3]);

/*
 * Function: volume_merge_from_string
 *
 * Merges the string-encoded voxels into this volume within the specified aabb.
 *
 * Inputs:
 *   volume - The volume.
 *   aabb - The bounding box within the volume to merge the string into.
 *   encoded_str - The string with encoded voxels, from volume_copy_to_string().
 */
void volume_merge_from_string(volume_t *volume, const int aabb[2][3], const char *encoded_str);

#endif // VOLUME_H
