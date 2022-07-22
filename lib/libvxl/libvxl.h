#ifndef LIBVXL_H
#define LIBVXL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//! @file libvxl.h
//! Reads and writes vxl maps, using an likewise internal memory format

//! @brief Internal chunk size
//! @note Map is split into square chunks internally to speed up modifications
//! @note Lower values can speed up map access, but can lead to higher memory fragmentation
#define LIBVXL_CHUNK_SIZE		16
//! @brief How many blocks the buffer will grow once it is full
#define LIBVXL_CHUNK_GROWTH		2
#define LIBVXL_CHUNK_SHRINK		4

//! @brief The default color to use when a block is solid, but has no color
//!
//! This is the case for e.g. underground blocks which are not visible from the surface
#define DEFAULT_COLOR(x,y,z)	0x674028

//! @brief Used to map coordinates to a key
//!
//! This format is used:
//! @code{.c}
//! 0xYYYXXXZZ
//! @endcode
//! This leaves 12 bits for X and Y and 8 bits for Z
#define pos_key(x,y,z)			(((y)<<20) | ((x)<<8) | (z))
#define key_discardz(key)		((key)&0xFFFFFF00)
#define key_getx(key)			(((key)>>8)&0xFFF)
#define key_gety(key)			(((key)>>20)&0xFFF)
#define key_getz(key)			((key)&0xFF)

struct __attribute((packed)) libvxl_span {
	uint8_t length;
	uint8_t color_start;
	uint8_t color_end;
	uint8_t air_start;
};

struct libvxl_block {
	uint32_t position;
	uint32_t color;
};

struct libvxl_chunk {
	struct libvxl_block* blocks;
	size_t length, index;
};

struct libvxl_map {
	size_t width, height, depth;
	struct libvxl_chunk* chunks;
	size_t* geometry;
	size_t streamed;
};

struct libvxl_stream {
	struct libvxl_map* map;
	size_t* chunk_offsets;
	size_t chunk_size;
	void* buffer;
	size_t buffer_offset;
	size_t pos;
};

struct __attribute((packed)) libvxl_kv6 {
	char magic[4];
	int width, height, depth;
	float pivot[3];
	int len;
};

struct __attribute((packed)) libvxl_kv6_block {
	int color;
	short z;
	unsigned char visfaces;
	unsigned char normal;
};

struct libvxl_chunk_copy {
	size_t width, height, depth;
	size_t* geometry;
	struct libvxl_block* blocks_sorted;
	size_t blocks_sorted_count;
};

void libvxl_copy_chunk_destroy(struct libvxl_chunk_copy* copy);

uint32_t libvxl_copy_chunk_get_color(struct libvxl_chunk_copy* copy, size_t x,
									 size_t y, size_t z);

bool libvxl_copy_chunk_is_solid(struct libvxl_chunk_copy* copy, size_t x,
								size_t y, size_t z);

void libvxl_copy_chunk(struct libvxl_map* map, struct libvxl_chunk_copy* copy,
					   size_t x, size_t y);

//! @brief Load a map from memory or create an empty one
//!
//! Example:
//! @code{.c}
//! struct libvxl_map m;
//! libvxl_create(&m,512,512,64,ptr);
//! @endcode
//! @param map Pointer to a struct of type libvxl_map that stores information about the loaded map
//! @param w Width of map (x-coord)
//! @param h Height of map (y-coord)
//! @param d Depth of map (z-coord)
//! @param data Pointer to valid map data, left unmodified also not freed
//! @param len map data size in bytes
//! @note Pass **NULL** as map data to create a new empty map, just water level will be filled with DEFAULT_COLOR
//! @returns 1 on success
bool libvxl_create(struct libvxl_map* map, size_t w, size_t h, size_t d, const void* data, size_t len);

//! @brief Write a map to disk, uses the libvxl_stream API internally
//! @param map Map to be written
//! @param name Filename of output file
//! @returns total bytes written to disk
size_t libvxl_writefile(struct libvxl_map* map, char* name);

//! @brief Compress the map back to vxl format and save it in *out*, the total byte size will be written to *size*
//! @param map Map to compress
//! @param out pointer to memory where the vxl will be stored
//! @param size pointer to an int, total byte size
void libvxl_write(struct libvxl_map* map, void* out, size_t* size);

//! @brief Tells if a block is solid at location [x,y,z]
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
//! @returns solid=1, air=0
//! @note Blocks out of map bounds are always non-solid
bool libvxl_map_issolid(struct libvxl_map* map, int x, int y, int z);

//! @brief Tells if a block is visible on the surface, meaning it is exposed to air
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
//! @returns on surface=1
bool libvxl_map_onsurface(struct libvxl_map* map, int x, int y, int z);

//! @brief Read block color
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
//! @returns color of block at location [x,y,z] in format *0xAARRGGBB*, on error *0*
uint32_t libvxl_map_get(struct libvxl_map* map, int x, int y, int z);

//! @brief Read color of topmost block (as seen from above at Z=0)
//!
//! See libvxl_map_get() for to be expected color format
//!
//! Example:
//! @code{.c}
//! int[2] result;
//! libvxl_map_gettop(&m,256,256,&result);
//! int color = result[0];
//! int height = result[1];
//! @endcode
//! @param map Map to use
//! @param x x-coordinate of block column
//! @param y y-coordinate of block column
//! @param result pointer to *int[2]*, is filled with color at *index 0* and height at *index 1*
//! @note *result* is left unmodified if [x,y,z] is out of map bounds
//! @returns *nothing, see result param*
void libvxl_map_gettop(struct libvxl_map* map, int x, int y, uint32_t* result);

//! @brief Set block at location [x,y,z] to a new color
//!
//! See libvxl_map_get() for expected color format, alpha component can be discarded
//!
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
//! @param color replacement color
//! @note nothing is changed if [x,y,z] is out of map bounds
void libvxl_map_set(struct libvxl_map* map, int x, int y, int z, uint32_t color);

//! @brief Set location [x,y,z] to air, will destroy any block at this position
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
void libvxl_map_setair(struct libvxl_map* map, int x, int y, int z);

//! @brief Free a map from memory
//! @param map Map to free
void libvxl_free(struct libvxl_map* map);

//! @brief Tries to guess the size of a map
//! @note This won't always give accurate results for a map's height
//! @note It is assumed the map is square.
//! @param size Pointer to int where edge length of the square will be stored
//! @param depth Pointer to int where map height will be stored
//! @param data Pointer to valid map data, left unmodified also not freed
//! @param len compressed map size in bytes
bool libvxl_size(size_t* size, size_t* depth, const void* data, size_t len);

//! @brief Start streaming a map
//! @param stream Pointer to a struct of type libvxl_stream
//! @param map Map to stream
//! @param chunk_size size in bytes each call to libvxl_stream_read() will encode at most
void libvxl_stream(struct libvxl_stream* stream, struct libvxl_map* map, size_t chunk_size);

//! @brief Free a stream from memory
//! @param map stream to free
void libvxl_stream_free(struct libvxl_stream* stream);

//! @brief Read bytes from the stream, this is at most stream->chunk_size bytes
//! @note The actual byte count encoded is returned, this can be less but never more than chunk_size
//! @note *0* is returned when the stream reached its end
//! @param stream stream to use
//! @param out pointer to buffer where encoded bytes are stored
//! @returns total byte count that was encoded
size_t libvxl_stream_read(struct libvxl_stream* stream, void* out);

//! @brief Check if a position is inside a map's boundary
//! @param map Map to use
//! @param x x-coordinate of block
//! @param y y-coordinate of block
//! @param z z-coordinate of block
bool libvxl_map_isinside(struct libvxl_map* map, int x, int y, int z);

#endif
