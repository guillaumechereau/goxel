#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "libvxl.h"

static struct libvxl_chunk* chunk_fposition(struct libvxl_map* map, size_t x,
											size_t y) {
	assert(map && x < map->width && y < map->height);

	size_t chunk_cnt = (map->width + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	size_t chunk_x = x / LIBVXL_CHUNK_SIZE;
	size_t chunk_y = y / LIBVXL_CHUNK_SIZE;
	return map->chunks + chunk_x + chunk_y * chunk_cnt;
}

static bool libvxl_geometry_get(struct libvxl_map* map, size_t x, size_t y,
								size_t z) {
	assert(map && x < map->width && y < map->height && z < map->depth);

	size_t offset = z + (x + y * map->width) * map->depth;
	return (map->geometry[offset / (sizeof(size_t) * 8)]
			& ((size_t)1 << (offset % (sizeof(size_t) * 8))))
		> 0;
}

static void libvxl_geometry_set(struct libvxl_map* map, size_t x, size_t y,
								size_t z, size_t state) {
	assert(map && x < map->width && y < map->height && z < map->depth);

	size_t offset = z + (x + y * map->width) * map->depth;

	size_t* val = map->geometry + offset / (sizeof(size_t) * 8);
	size_t bit = offset % (sizeof(size_t) * 8);

	*val = (*val & ~((size_t)1 << bit)) | (state << bit);
}

static int cmp(const void* a, const void* b) {
	struct libvxl_block* aa = (struct libvxl_block*)a;
	struct libvxl_block* bb = (struct libvxl_block*)b;

	return aa->position - bb->position;
}

static void libvxl_chunk_put(struct libvxl_chunk* chunk, uint32_t pos,
							 uint32_t color) {
	assert(chunk);

	if(chunk->index == chunk->length) { // needs to grow
		chunk->length *= LIBVXL_CHUNK_GROWTH;
		chunk->blocks = realloc(chunk->blocks,
								chunk->length * sizeof(struct libvxl_block));
	}

	memcpy(chunk->blocks + (chunk->index++),
		   &(struct libvxl_block) {
			   .position = pos,
			   .color = color,
		   },
		   sizeof(struct libvxl_block));
}

static struct libvxl_block*
libvxl_chunk_gequal_block(struct libvxl_chunk* chunk, uint32_t pos) {
	assert(chunk);

	size_t start = 0;
	size_t end = chunk->index;
	while(end > start) {
		size_t mid = (start + end) / 2;
		if(pos > chunk->blocks[mid].position) {
			start = mid + 1;
		} else if(pos < chunk->blocks[mid].position) {
			end = mid;
		} else {
			return chunk->blocks + mid;
		}
	}

	return chunk->blocks + start;
}

static void libvxl_chunk_insert(struct libvxl_chunk* chunk, uint32_t pos,
								uint32_t color) {
	assert(chunk);

	// TODO: use libvxl_chunk_gequal_block

	size_t start = 0;
	size_t end = chunk->index;
	while(end > start) {
		size_t mid = (start + end) / 2;
		if(pos > chunk->blocks[mid].position) {
			start = mid + 1;
		} else if(pos < chunk->blocks[mid].position) {
			end = mid;
		} else { // diff=0, replace color
			chunk->blocks[mid].color = color;
			return;
		}
	}

	if(chunk->index == chunk->length) { // needs to grow
		chunk->length *= LIBVXL_CHUNK_GROWTH;
		chunk->blocks = realloc(chunk->blocks,
								chunk->length * sizeof(struct libvxl_block));
	}

	memmove(chunk->blocks + start + 1, chunk->blocks + start,
			(chunk->index - start) * sizeof(struct libvxl_block));
	chunk->blocks[start].position = pos;
	chunk->blocks[start].color = color;
	chunk->index++;
}

static size_t libvxl_span_length(struct libvxl_span* s) {
	assert(s);

	return s->length > 0 ? s->length * 4 :
							 (s->color_end + 2 - s->color_start) * 4;
}

void libvxl_free(struct libvxl_map* map) {
	if(!map)
		return;
	size_t sx = (map->width + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	size_t sy = (map->height + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	for(size_t k = 0; k < sx * sy; k++)
		free(map->chunks[k].blocks);
	free(map->chunks);
	free(map->geometry);
}

bool libvxl_size(size_t* size, size_t* depth, const void* data, size_t len) {
	if(!data || !size || !depth || len == 0)
		return false;
	size_t offset = 0;
	size_t columns = 0;
	*depth = 0;
	while(offset + sizeof(struct libvxl_span) - 1 < len) {
		struct libvxl_span* desc = (struct libvxl_span*)(data + offset);
		if(desc->color_end + 1 > (int)*depth)
			*depth = desc->color_end + 1;
		if(!desc->length)
			columns++;
		offset += libvxl_span_length(desc);
	}
	*depth = 1 << (int)ceil(log2f(*depth));
	*size = sqrt(columns);
	return true;
}

bool libvxl_create(struct libvxl_map* map, size_t w, size_t h, size_t d,
				   const void* data, size_t len) {
	if(!map)
		return false;
	map->streamed = 0;
	map->width = w;
	map->height = h;
	map->depth = d;
	size_t sx = (w + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	size_t sy = (h + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	map->chunks = malloc(sx * sy * sizeof(struct libvxl_chunk));
	for(size_t y = 0; y < sy; y++) {
		for(size_t x = 0; x < sx; x++) {
			map->chunks[x + y * sx].length = LIBVXL_CHUNK_SIZE
				* LIBVXL_CHUNK_SIZE * 2; // allows for two fully filled layers
			map->chunks[x + y * sx].index = 0;
			map->chunks[x + y * sx].blocks = malloc(
				map->chunks[x + y * sx].length * sizeof(struct libvxl_block));
		}
	}

	size_t sg = (w * h * d + (sizeof(size_t) * 8 - 1)) / (sizeof(size_t) * 8)
		* sizeof(size_t);
	map->geometry = malloc(sg);
	if(data) {
		memset(map->geometry, 0xFF, sg);
	} else {
		memset(map->geometry, 0x00, sg);
		for(size_t y = 0; y < h; y++)
			for(size_t x = 0; x < w; x++)
				libvxl_map_set(map, x, y, d - 1, DEFAULT_COLOR(x, y, d - 1));
		return true;
	}

	size_t offset = 0;
	for(size_t y = 0; y < map->height; y++) {
		for(size_t x = 0; x < map->width; x++) {
			struct libvxl_chunk* chunk = chunk_fposition(map, x, y);

			while(1) {
				if(offset + sizeof(struct libvxl_span) - 1 >= len)
					return false;
				struct libvxl_span* desc = (struct libvxl_span*)(data + offset);
				if(offset + libvxl_span_length(desc) - 1 >= len)
					return false;
				uint32_t* color_data
					= (uint32_t*)(data + offset + sizeof(struct libvxl_span));

				for(size_t z = desc->air_start; z < desc->color_start; z++)
					libvxl_geometry_set(map, x, y, z, 0);

				for(size_t z = desc->color_start; z <= desc->color_end;
					z++) // top color run
					libvxl_chunk_put(chunk, pos_key(x, y, z),
									 color_data[z - desc->color_start]);

				size_t top_len = desc->color_end - desc->color_start + 1;
				size_t bottom_len = desc->length - 1 - top_len;

				if(desc->length > 0) {
					if(offset + libvxl_span_length(desc)
						   + sizeof(struct libvxl_span) - 1
					   >= len)
						return false;
					struct libvxl_span* desc_next
						= (struct libvxl_span*)(data + offset
												+ libvxl_span_length(desc));
					for(size_t z = desc_next->air_start - bottom_len;
						z < desc_next->air_start; z++) // bottom color run
						libvxl_chunk_put(
							chunk, pos_key(x, y, z),
							color_data[z - (desc_next->air_start - bottom_len)
									   + top_len]);
					offset += libvxl_span_length(desc);
				} else {
					offset += libvxl_span_length(desc);
					break;
				}
			}
		}
	}

	for(size_t z = 0; z < map->depth; z++) {
		for(size_t x = 0; x < map->width; x++) {
			bool A = libvxl_geometry_get(map, x, 0, z);
			bool B = libvxl_geometry_get(map, x, map->height - 1, z);

			struct libvxl_chunk* c1 = chunk_fposition(map, x, 0);
			struct libvxl_block* b1 = bsearch(
				&(struct libvxl_block) {
					.position = pos_key(x, 0, z),
				},
				c1->blocks, c1->index, sizeof(struct libvxl_block), cmp);

			struct libvxl_chunk* c2 = chunk_fposition(map, x, map->height - 1);
			struct libvxl_block* b2 = bsearch(
				&(struct libvxl_block) {
					.position = pos_key(x, map->height - 1, z),
				},
				c2->blocks, c2->index, sizeof(struct libvxl_block), cmp);

			if(A && !B && !b1)
				libvxl_chunk_insert(c1, pos_key(x, 0, z),
									DEFAULT_COLOR(x, 0, z));

			if(!A && B && !b2)
				libvxl_chunk_insert(c2, pos_key(x, map->height - 1, z),
									DEFAULT_COLOR(x, map->height - 1, z));
		}

		for(size_t y = 0; y < map->height; y++) {
			bool A = libvxl_geometry_get(map, 0, y, z);
			bool B = libvxl_geometry_get(map, map->width - 1, y, z);

			struct libvxl_chunk* c1 = chunk_fposition(map, 0, y);
			struct libvxl_block* b1 = bsearch(
				&(struct libvxl_block) {
					.position = pos_key(0, y, z),
				},
				c1->blocks, c1->index, sizeof(struct libvxl_block), cmp);

			struct libvxl_chunk* c2 = chunk_fposition(map, map->width - 1, y);
			struct libvxl_block* b2 = bsearch(
				&(struct libvxl_block) {
					.position = pos_key(map->width - 1, y, z),
				},
				c2->blocks, c2->index, sizeof(struct libvxl_block), cmp);

			if(A && !B && !b1)
				libvxl_chunk_insert(c1, pos_key(0, y, z),
									DEFAULT_COLOR(0, y, z));

			if(!A && B && !b2)
				libvxl_chunk_insert(c2, pos_key(map->width - 1, y, z),
									DEFAULT_COLOR(map->width - 1, y, z));
		}
	}

	return true;
}

static size_t find_successive_surface(struct libvxl_chunk* chunk,
									  size_t block_offset, int x, int y,
									  size_t start,
									  struct libvxl_block** current) {
	assert(chunk && current);

	*current = chunk->blocks + block_offset;
	struct libvxl_block* end_marker = chunk->blocks + chunk->index;

	if(*current < end_marker && (*current)->position == pos_key(x, y, start)) {
		while(1) {
			uint32_t next_z = key_getz((*current)->position) + 1;
			(*current)++;

			if(*current >= end_marker
			   || (*current)->position != pos_key(x, y, next_z))
				return next_z;
		}
	} else {
		return start;
	}
}

static void libvxl_column_encode(struct libvxl_map* map, size_t* chunk_offsets,
								 int x, int y, void* out, size_t* offset) {
	assert(map && chunk_offsets && out && offset);

	struct libvxl_chunk* chunk = chunk_fposition(map, x, y);

	bool first_run = true;
	size_t z
		= key_getz(chunk->blocks[chunk_offsets[chunk - map->chunks]].position);
	while(1) {
		size_t top_start = libvxl_geometry_get(map, x, y, z) ?
			z :
			  key_getz(
				chunk->blocks[chunk_offsets[chunk - map->chunks]].position);

		struct libvxl_block* last_surface_block;
		size_t top_end
			= find_successive_surface(chunk, chunk_offsets[chunk - map->chunks],
									  x, y, top_start, &last_surface_block);

		size_t bottom_start = map->depth;

		if(top_end == map->depth || !libvxl_geometry_get(map, x, y, top_end)) {
			bottom_start = top_end;
		} else if(last_surface_block < chunk->blocks + chunk->index
				  && key_discardz(last_surface_block->position)
					  == pos_key(x, y, 0)) {
			bottom_start = key_getz(last_surface_block->position);
		}

		struct libvxl_span* desc = (struct libvxl_span*)(out + *offset);
		desc->color_start = top_start;
		desc->color_end = top_end - 1;
		desc->air_start = first_run ? 0 : z;
		*offset += sizeof(struct libvxl_span);

		assert(desc->air_start <= desc->color_start);
		assert(desc->color_start <= desc->color_end + 1);

		first_run = false;

		for(size_t k = top_start; k < top_end; k++) {
			*(uint32_t*)(out + *offset)
				= (chunk->blocks[chunk_offsets[chunk - map->chunks]++].color
				   & 0xFFFFFF)
				| 0x7F000000;
			*offset += sizeof(uint32_t);
		}

		if(bottom_start == map->depth) {
			/* this is the last span of this column, do not emit bottom colors,
			 * set length to 0 */
			desc->length = 0;
			break;
		} else { // bottom_start < map->depth
			struct libvxl_block* last_surface_block;
			size_t bottom_end = find_successive_surface(
				chunk, chunk_offsets[chunk - map->chunks], x, y, bottom_start,
				&last_surface_block);

			// there are more spans to follow, emit bottom colors
			if(bottom_end < map->depth) {
				desc->length
					= 1 + top_end - top_start + bottom_end - bottom_start;

				for(size_t k = bottom_start; k < bottom_end; k++) {
					*(uint32_t*)(out + *offset)
						= (chunk->blocks[chunk_offsets[chunk - map->chunks]++]
							   .color
						   & 0xFFFFFF)
						| 0x7F000000;
					*offset += sizeof(uint32_t);
				}

				z = bottom_end;
			} else { // bottom_end == map->depth
				desc->length = 1 + top_end - top_start;
				z = bottom_start;
			}
		}
	}
}

void libvxl_stream(struct libvxl_stream* stream, struct libvxl_map* map,
				   size_t chunk_size) {
	if(!stream || !map || chunk_size == 0)
		return;
	stream->map = map;
	map->streamed++;
	stream->chunk_size = chunk_size;
	stream->pos = pos_key(0, 0, 0);
	stream->buffer_offset = 0;
	stream->buffer = malloc(stream->chunk_size * 2);

	size_t sx = (map->width + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	size_t sy = (map->height + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	stream->chunk_offsets = malloc(sx * sy * sizeof(size_t));
	memset(stream->chunk_offsets, 0, sx * sy * sizeof(size_t));
}

void libvxl_stream_free(struct libvxl_stream* stream) {
	if(!stream)
		return;
	stream->map->streamed--;
	free(stream->buffer);
	free(stream->chunk_offsets);
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

size_t libvxl_stream_read(struct libvxl_stream* stream, void* out) {
	if(!stream || !out || key_gety(stream->pos) >= stream->map->height)
		return 0;
	while(stream->buffer_offset < stream->chunk_size
		  && key_gety(stream->pos) < stream->map->height) {
		libvxl_column_encode(stream->map, stream->chunk_offsets,
							 key_getx(stream->pos), key_gety(stream->pos),
							 stream->buffer, &stream->buffer_offset);
		if(key_getx(stream->pos) + 1 < stream->map->width)
			stream->pos
				= pos_key(key_getx(stream->pos) + 1, key_gety(stream->pos), 0);
		else
			stream->pos = pos_key(0, key_gety(stream->pos) + 1, 0);
	}
	size_t length = stream->buffer_offset;
	memcpy(out, stream->buffer, min(length, stream->chunk_size));
	if(length < stream->chunk_size) {
		stream->buffer_offset = 0;
	} else {
		memmove(stream->buffer, stream->buffer + stream->chunk_size,
				length - stream->chunk_size);
		stream->buffer_offset -= stream->chunk_size;
	}
	return min(length, stream->chunk_size);
}

void libvxl_write(struct libvxl_map* map, void* out, size_t* size) {
	if(!map || !out)
		return;
	size_t sx = (map->width + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	size_t sy = (map->height + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;

	size_t chunk_offsets[sx * sy];
	memset(chunk_offsets, 0, sx * sy * sizeof(size_t));

	size_t offset = 0;
	for(uint32_t y = 0; y < map->height; y++)
		for(uint32_t x = 0; x < map->width; x++)
			libvxl_column_encode(map, chunk_offsets, x, y, out, &offset);

	if(size)
		*size = offset;
}

size_t libvxl_writefile(struct libvxl_map* map, char* name) {
	if(!map || !name)
		return 0;
	uint8_t buf[1024];
	struct libvxl_stream s;
	libvxl_stream(&s, map, 1024);
	FILE* f = fopen(name, "wb");
	size_t read, total = 0;
	while((read = libvxl_stream_read(&s, buf))) {
		fwrite(buf, 1, read, f);
		total += read;
	}
	fclose(f);
	libvxl_stream_free(&s);
	return total;
}

/*void libvxl_kv6_write(struct libvxl_map* map, char* name) {
	FILE* f = fopen(name, "wb");

	struct libvxl_kv6 header;
	strcpy(header.magic, "Kvxl");
	header.width = map->width;
	header.height = map->height;
	header.depth = map->depth;
	header.pivot[0] = map->width / 2.0F;
	header.pivot[1] = map->height / 2.0F;
	header.pivot[2] = map->depth / 2.0F;

	header.len = 0;

	int sx = (map->width + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	int sy = (map->height + LIBVXL_CHUNK_SIZE - 1) / LIBVXL_CHUNK_SIZE;
	for(int y = 0; y < sy; y++)
		for(int x = 0; x < sx; x++)
			header.len += map->chunks[x + y * sx].index;

	fwrite(&header, sizeof(header), 1, f);

	int xoffset[map->width];
	short xyoffset[map->width * map->height];
	memset(xoffset, 0, sizeof(xoffset));
	memset(xyoffset, 0, sizeof(xyoffset));

	size_t chunk_offsets[sx * sy];
	memset(chunk_offsets, 0, sizeof(chunk_offsets));

	for(size_t y = 0; y < map->height; y++) {
		for(size_t x = 0; x < map->width; x++) {
			struct libvxl_chunk* chunk = chunk_fposition(map, x, y);

			while(1) {
				struct libvxl_block* next
					= &chunk->blocks[chunk_offsets[chunk - map->chunks]];
				if(key_getx(next->position) != x)
					break;

				chunk_offsets[chunk - map->chunks]++;

				struct libvxl_kv6_block blk;
				blk.color = next->color & 0xFFFFFF;
				blk.z = key_getz(next->position);
				blk.normal = 0;

				blk.visfaces = 0;
				if(libvxl_map_issolid(map, x + 1, y, blk.z))
					blk.visfaces |= 0x01;
				if(libvxl_map_issolid(map, x - 1, y, blk.z))
					blk.visfaces |= 0x02;
				if(libvxl_map_issolid(map, x, y + 1, blk.z))
					blk.visfaces |= 0x04;
				if(libvxl_map_issolid(map, x, y - 1, blk.z))
					blk.visfaces |= 0x08;
				if(libvxl_map_issolid(map, x, y, blk.z + 1))
					blk.visfaces |= 0x10;
				if(libvxl_map_issolid(map, x, y, blk.z - 1))
					blk.visfaces |= 0x20;

				fwrite(&blk, sizeof(blk), 1, f);

				xoffset[x]++;
				xyoffset[x + y * map->width]++;
			}
		}
	}

	fwrite(xoffset, sizeof(xoffset), 1, f);
	fwrite(xyoffset, sizeof(xyoffset), 1, f);

	fclose(f);
}*/

bool libvxl_map_isinside(struct libvxl_map* map, int x, int y, int z) {
	return map && x >= 0 && y >= 0 && z >= 0 && x < (int)map->width
		&& y < (int)map->height && z < (int)map->depth;
}

uint32_t libvxl_map_get(struct libvxl_map* map, int x, int y, int z) {
	if(!map || x < 0 || y < 0 || z < 0 || x >= (int)map->width
	   || y >= (int)map->height || z >= (int)map->depth)
		return 0;
	if(!libvxl_geometry_get(map, x, y, z))
		return 0;
	struct libvxl_chunk* chunk = chunk_fposition(map, x, y);
	struct libvxl_block* loc = bsearch(
		&(struct libvxl_block) {
			.position = pos_key(x, y, z),
		},
		chunk->blocks, chunk->index, sizeof(struct libvxl_block), cmp);
	return loc ? loc->color : DEFAULT_COLOR(x, y, z);
}

bool libvxl_map_issolid(struct libvxl_map* map, int x, int y, int z) {
	if(z < 0)
		return false;
	if(!map || z >= (int)map->depth)
		return true;
	return libvxl_geometry_get(map, (size_t)x % (size_t)map->width,
							   (size_t)y % (size_t)map->height, z);
}

bool libvxl_map_onsurface(struct libvxl_map* map, int x, int y, int z) {
	if(!map)
		return false;
	return !libvxl_map_issolid(map, x, y + 1, z)
		|| !libvxl_map_issolid(map, x, y - 1, z)
		|| !libvxl_map_issolid(map, x + 1, y, z)
		|| !libvxl_map_issolid(map, x - 1, y, z)
		|| !libvxl_map_issolid(map, x, y, z + 1)
		|| !libvxl_map_issolid(map, x, y, z - 1);
}

void libvxl_map_gettop(struct libvxl_map* map, int x, int y, uint32_t* result) {
	if(!map || x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height)
		return;

	struct libvxl_chunk* c = chunk_fposition(map, x, y);
	struct libvxl_block* block = libvxl_chunk_gequal_block(c, pos_key(x, y, 0));

	assert(block < c->blocks + c->index);
	assert(key_discardz(block->position) == pos_key(x, y, 0));
	assert(key_getz(block->position) < map->depth);

	result[0] = block->color;
	result[1] = key_getz(block->position);
}

static void libvxl_map_set_internal(struct libvxl_map* map, int x, int y, int z,
									uint32_t color) {
	assert(map);

	if(z < 0 || z >= (int)map->depth)
		return;

	x = (size_t)x % (size_t)map->width;
	y = (size_t)y % (size_t)map->height;

	if(libvxl_geometry_get(map, x, y, z) && !libvxl_map_onsurface(map, x, y, z))
		return;

	libvxl_chunk_insert(chunk_fposition(map, x, y), pos_key(x, y, z), color);
}

static void libvxl_map_setair_internal(struct libvxl_map* map, int x, int y,
									   int z) {
	assert(map);

	if(z < 0 || z >= (int)map->depth)
		return;

	x = (size_t)x % (size_t)map->width;
	y = (size_t)y % (size_t)map->height;

	if(!libvxl_geometry_get(map, x, y, z))
		return;

	struct libvxl_chunk* chunk = chunk_fposition(map, x, y);

	struct libvxl_block* loc = bsearch(
		&(struct libvxl_block) {
			.position = pos_key(x, y, z),
		},
		chunk->blocks, chunk->index, sizeof(struct libvxl_block), cmp);

	if(loc) {
		memmove(loc, loc + 1,
				(chunk->blocks + (--chunk->index) - loc)
					* sizeof(struct libvxl_block));

		if(chunk->index * LIBVXL_CHUNK_SHRINK <= chunk->length) {
			chunk->length /= LIBVXL_CHUNK_GROWTH;
			chunk->blocks = realloc(
				chunk->blocks, chunk->length * sizeof(struct libvxl_block));
		}
	}
}

void libvxl_map_set(struct libvxl_map* map, int x, int y, int z,
					uint32_t color) {
	if(!map || x < 0 || y < 0 || z < 0 || x >= (int)map->width
	   || y >= (int)map->height || z >= (int)map->depth)
		return;

	libvxl_geometry_set(map, x, y, z, 1);
	libvxl_map_set_internal(map, x, y, z, color);

	if(!libvxl_map_onsurface(map, x, y + 1, z))
		libvxl_map_setair_internal(map, x, y + 1, z);
	if(!libvxl_map_onsurface(map, x, y - 1, z))
		libvxl_map_setair_internal(map, x, y - 1, z);

	if(!libvxl_map_onsurface(map, x + 1, y, z))
		libvxl_map_setair_internal(map, x + 1, y, z);
	if(!libvxl_map_onsurface(map, x - 1, y, z))
		libvxl_map_setair_internal(map, x - 1, y, z);

	if(!libvxl_map_onsurface(map, x, y, z + 1))
		libvxl_map_setair_internal(map, x, y, z + 1);
	if(!libvxl_map_onsurface(map, x, y, z - 1))
		libvxl_map_setair_internal(map, x, y, z - 1);
}

void libvxl_map_setair(struct libvxl_map* map, int x, int y, int z) {
	if(!map || x < 0 || y < 0 || z < 0 || x >= (int)map->width
	   || y >= (int)map->height || z >= (int)map->depth - 1)
		return;

	bool surface_prev[6] = {
		libvxl_map_issolid(map, x, y + 1, z) ?
			libvxl_map_onsurface(map, x, y + 1, z) :
			  true,
		libvxl_map_issolid(map, x, y - 1, z) ?
			libvxl_map_onsurface(map, x, y - 1, z) :
			  true,
		libvxl_map_issolid(map, x + 1, y, z) ?
			libvxl_map_onsurface(map, x + 1, y, z) :
			  true,
		libvxl_map_issolid(map, x - 1, y, z) ?
			libvxl_map_onsurface(map, x - 1, y, z) :
			  true,
		libvxl_map_issolid(map, x, y, z + 1) ?
			libvxl_map_onsurface(map, x, y, z + 1) :
			  true,
		libvxl_map_issolid(map, x, y, z - 1) ?
			libvxl_map_onsurface(map, x, y, z - 1) :
			  true,
	};

	libvxl_map_setair_internal(map, x, y, z);
	libvxl_geometry_set(map, x, y, z, 0);

	if(!surface_prev[0] && libvxl_map_onsurface(map, x, y + 1, z))
		libvxl_map_set_internal(map, x, y + 1, z, DEFAULT_COLOR(x, y + 1, z));
	if(!surface_prev[1] && libvxl_map_onsurface(map, x, y - 1, z))
		libvxl_map_set_internal(map, x, y - 1, z, DEFAULT_COLOR(x, y - 1, z));

	if(!surface_prev[2] && libvxl_map_onsurface(map, x + 1, y, z))
		libvxl_map_set_internal(map, x + 1, y, z, DEFAULT_COLOR(x + 1, y, z));
	if(!surface_prev[3] && libvxl_map_onsurface(map, x - 1, y, z))
		libvxl_map_set_internal(map, x - 1, y, z, DEFAULT_COLOR(x - 1, y, z));

	if(!surface_prev[4] && libvxl_map_onsurface(map, x, y, z + 1))
		libvxl_map_set_internal(map, x, y, z + 1, DEFAULT_COLOR(x, y, z + 1));
	if(!surface_prev[5] && libvxl_map_onsurface(map, x, y, z - 1))
		libvxl_map_set_internal(map, x, y, z - 1, DEFAULT_COLOR(x, y, z - 1));
}

void libvxl_copy_chunk_destroy(struct libvxl_chunk_copy* copy) {
	if(!copy)
		return;

	free(copy->geometry);
	free(copy->blocks_sorted);
}

uint32_t libvxl_copy_chunk_get_color(struct libvxl_chunk_copy* copy, size_t x,
									 size_t y, size_t z) {
	if(!copy)
		return 0;

	struct libvxl_block* loc = bsearch(
		&(struct libvxl_block) {
			.position = pos_key(x, y, z),
		},
		copy->blocks_sorted, copy->blocks_sorted_count,
		sizeof(struct libvxl_block), cmp);
	return loc ? (loc->color & 0xFFFFFF) : 0;
}

bool libvxl_copy_chunk_is_solid(struct libvxl_chunk_copy* copy, size_t x,
								size_t y, size_t z) {
	if(!copy || x >= copy->width || y >= copy->height || z >= copy->depth)
		return true;

	size_t offset = z + (x + y * copy->width) * copy->depth;
	return copy->geometry[offset / (sizeof(size_t) * 8)]
		& ((size_t)1 << (offset % (sizeof(size_t) * 8)));
}

void libvxl_copy_chunk(struct libvxl_map* map, struct libvxl_chunk_copy* copy,
					   size_t x, size_t y) {
	if(!map || !copy || x >= map->width || y >= map->height)
		return;

	size_t sg
		= (map->width * map->height * map->depth + (sizeof(size_t) * 8 - 1))
		/ (sizeof(size_t) * 8) * sizeof(size_t);
	copy->geometry = malloc(sg);
	memcpy(copy->geometry, map->geometry, sg);

	struct libvxl_chunk* c = chunk_fposition(map, x, y);
	copy->blocks_sorted = malloc(c->index * sizeof(struct libvxl_block));
	copy->blocks_sorted_count = c->index;
	memcpy(copy->blocks_sorted, c->blocks,
		   c->index * sizeof(struct libvxl_block));

	copy->width = map->width;
	copy->height = map->height;
	copy->depth = map->depth;
}
