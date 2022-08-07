/*
 * Schematic (Minecraft / Minetest) format support by bzt
 *
 * They have a little bit different layout, but the same data is stored.
 * There are lots of other similarities and some shared code too. MTS,
 * old NBT and new NBT all need a palette with block names as color
 * names in order to work, because neither of them store voxel colors.
 */

#include "goxel.h"
#include "file_format.h"
#include <errno.h>

// Prevent warnings in stb code.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#ifdef WIN32
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#endif

// I guess it would be better to remove these defines here, and STB_*_STATIC
// in img.c as well. Currently using static duplicates some (small) zlib code

// get stbi_zlib_decompress()
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_GIF
#include "stb_image.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


/*** schematic reader ***/

/* match a name with multiple options */
int namematch(char *a, char *b)
{
	int la, lb, i;
	if(!a || !*a || !b || !*b) return 0;
	if(!strcmp(a, b)) return 1;
	la = strlen(a); lb = strlen(b);
	for(i = 0; i < lb - la + 1; i++) {
		if((!i || b[i-1] == '/') && !memcmp(a, b + i, la) &&
			(!b[i + la] || b[i + la] == '/' || b[i + la] == '\r' || b[i + la] == '\n'))
				return 1;
	}
	return 0;
}

/* parse an MSB integer, the only dynamic type we are interested in. */
#define SCHEM_GETINT(v,t) do{switch(t){case 1:v=d[0];d++;break;case 2:v=(d[0]<<8)|d[1];d+=2;break; \
	case 3:v=(d[0]<<24)|(d[1]<<16)|(d[2]<<8)|d[3];d+=4;break;}}while(0)

static int import_as_schem(image_t *image, const char *path)
{
	FILE *file;
	layer_t *layer;
	mesh_iterator_t iter = {0};
	long int size;
	unsigned char *data, *buff, *un;
	unsigned char *d, *s, *blk, *blkp = NULL;
	int i = -1, j, k, l, n, g = 0, x, y, z, mx = 0, my = 0, mz = 0, pos[3];
	int *palette = NULL;
	uint8_t black[4] = { 0, 0, 0, 255 };

	// check if all colors in the active palette have names,
	// because names are only loaded from .gpl files nowhere else
	if(goxel.palette && goxel.palette->entries) {
		for(i = 0; i < goxel.palette->size &&
			goxel.palette->entries[i].name[0]; i++);
	}
	if(!goxel.palette || i < goxel.palette->size) {
		gui_alert("Palette Error",
			"Please choose a valid palette\n"
			"with color names being block names");
		return -1;
	}

	// read in file data
	file = fopen(path, "rb");
	if(!file) {
ferr:   LOG_E("Cannot load from %s: %s", path, strerror(errno));
		return -1;
	}
	fseek(file, 0, SEEK_END);
	size = (long int)ftell(file);
	if(size < 8) { fclose(file); goto ferr; }
	fseek(file, 0, SEEK_SET);
	data = (unsigned char*)malloc(size);
	if(!data) {
		fclose(file);
memerr: if(data) free(data);
		LOG_E("Memory allocation error %s: %ld bytes", path, size);
		return -1;
	}
	if(fread(data, 1, size, file) != size) {
		fclose(file);
		free(data);
		LOG_E("Cannot load from %s: %s", path, strerror(errno));
		return -1;
	}
	fclose(file);

	layer = image_add_layer(goxel.image, NULL);
	iter = mesh_get_accessor(layer->mesh);
	memset(layer->name, 0, sizeof(layer->name));

	// if it's gzip compressed, uncompress it first
	if(data[0] == 0x1f && data[1] == 0x8b) {
		buff = data + 3;
		x = *buff++; buff += 6;
		if(x & 4) { y = *buff++; y += (*buff++ << 8); buff += y; }
		if(x & 8) { while(*buff++ != 0); }
		if(x & 16) { while(*buff++ != 0); }
		if(x & 2) buff += 2;
		size -= (int)(buff - data);
		buff = (uint8_t*)stbi_zlib_decode_malloc_guesssize_headerflag((const char*)buff, size, 4096, &x, 0);
		if(buff) { free(data); data = buff; buff = NULL; size = x; }
	}

	// if it's a Minetest schematic
	if(!memcmp(data, "MTSM", 4)) {
		LOG_I("Minetest MTS format %s", path);
		// uses MSB
		mx = (data[6] << 8) | data[7];
		my = (data[8] << 8) | data[9];
		mz = (data[10] << 8) | data[11];
		buff = data + 12;
		// version 4 and above has a probability map
		if(buff[0] > 3 || data[5] == 4)
			buff += my;
		// map the palette
		n = (buff[0] << 8) | buff[1]; buff += 2;
		palette = (int*)malloc(n * sizeof(int));
		if(!palette) goto memerr;
		for(i = 0; i < n; i++) {
			j = buff[1];
			buff += 2;
			if(j == 3 && (!memcmp(buff, "Air", 3) || !memcmp(buff, "air", 3))) 
				palette[i] = -1;
			else {
				palette[i] = 0;
				x = buff[j]; buff[j] = 0;
				for(k = 0; k < goxel.palette->size; k++) {
					if(namematch((char*)buff, goxel.palette->entries[k].name)) {
						palette[i] = k; break;
					}
				}
				buff[j] = x;
			}
			buff += j;
		}
		// uncompress indices
		size -= (int)(buff - data);
		un = (uint8_t*)stbi_zlib_decode_malloc_guesssize_headerflag((const char*)buff, size, 4096, &i, 1);
		if(un) { buff = un; size = i; }
		// set mesh colors
		for(z = 0; z < mz; z++)
			for(y = 0; y < my; y++)
				for(x = 0; x < mx; x++, buff += 2) {
					k = (buff[0] << 8) | buff[1];
					if(k < n && palette[k] >= 0) {
						pos[0] = x - mx/2; pos[1] = z - mz/2; pos[2] = y;
						mesh_set_at(layer->mesh, &iter, pos,
							palette[k] < goxel.palette->size ?
								(uint8_t*)&goxel.palette->entries[palette[k]].color :
								(uint8_t*)&black);
					}
				}
		if(un) free(un);
	} else
	// if it's a Minecraft NBT
	if(!memcmp(data + 3, "Schematic", 9)) {
		// get the dimensions and necessary chunk offsets, also MSB
		for(d = data, n = 0, blk = NULL; d < data + size - 12; d++) {
			if(!memcmp(d, "\000\006Height", 8)) { d += 8; SCHEM_GETINT(my, d[-9]); d--; } else
			if(!memcmp(d, "\000\006Length", 8)) { d += 8; SCHEM_GETINT(mz, d[-9]); d--; } else
			if(!memcmp(d, "\000\005Width", 7)) { d += 7; SCHEM_GETINT(mx, d[-8]); d--; } else
			if(!memcmp(d, "\000\011WEOffsetY", 11)) { d += 11; SCHEM_GETINT(g, d[-12]); d--; } else
			if(!memcmp(d, "\000\012PaletteMax", 12)) { d += 12; SCHEM_GETINT(n, d[-13]); d--; } else
			if(!memcmp(d, "\007\000\006Blocks", 9)) { d += 9; SCHEM_GETINT(l, 3); blk = d; d += l; d--; } else
			if(!memcmp(d, "\012\000\007Palette", 10)) { d += 10; blkp = d; } else
			if(!memcmp(d, "\007\000\011BlockData", 12)) { d += 12; SCHEM_GETINT(l, 3); blk = d; d += l; d--; }
		}
		if(!blk || !my || !mz || !mx || (blkp && !n)) {
			free(data);
			LOG_E("Unable to find mandatory NBT chunks in %s", path);
			return -1;
		}
		// map the palette
		if(!blkp) {
			// Old NBT format, index maps to palette 1=1
			LOG_I("Old NBT format %s", path);
			n = goxel.palette->size;
			palette = (int*)malloc(n * sizeof(int));
			if(!palette) goto memerr;
			palette[0] = -1;
			for(i = 1; i < n; i++) palette[i] = i;
		} else {
			// New NBT, using a named palette
			LOG_I("Old NBT format %s", path);
			palette = (int*)realloc(palette, n * sizeof(int));
			if(!palette) goto memerr;
			for(d = blkp; *d > 0 && *d < 4;) {
				j = *d++; d++; l = *d++; s = d; d += l;
				SCHEM_GETINT(i, j);
				if(i >= 0 && i < n) {
					x = s[l]; s[l] = 0;
					for(l = 0; s[l]; l++)
						if(s[l] == '[') { s[l] = 0; break; }
					if(l == 13 && !strcmp((char*)s, "minecraft:air"))
						palette[i] = -1;
					else {
						palette[i] = goxel.palette->size;
						for(k = 0; k < goxel.palette->size; k++)
							if(namematch((char*)s, goxel.palette->entries[k].name)) {
								palette[i] = k; break;
							}
					}
					s[l] = x;
				}
			}
		}
		// set mesh colors
		for(d = blk, i = y = 0; y < my; y++) {
			for(z = 0; z < mz; z++) {
				for(x = 0; x < mx; x++, i++) {
					if(!blkp) {
						// old NBT format uses one byte per index
						k = *d++;
					} else {
						// new NBT uses varying length encoding
						for(k = 0, j = 0; j < 21; d++, j += 7) {
							k |= (*d & 0x7F) << j;
							if(!(*d & 0x80)) { d++; break; }
						}
					}
					if(k < n && palette[k] >= 0) {
						pos[0] = x - mx/2; pos[1] = mz/2 - z; pos[2] = y + g;
						mesh_set_at(layer->mesh, &iter, pos,
							palette[k] < goxel.palette->size ?
								(uint8_t*)&goxel.palette->entries[palette[k]].color :
								(uint8_t*)&black);
					}
				}
			}
		}
        // Set The Canvas (Editor Box) Size To Fit With Current Model.
        action_exec2(ACTION_img_auto_resize);
	} else {
		LOG_E("Unknown schematic format %s", path);
	}

	if(palette) free(palette);
	free(data);
	return 0;
}

FILE_FORMAT_REGISTER(schem,
	.name = "Minecraft / Minetest",
	.ext = "mts\0*.mts\0nbt\0*.nbt\0schem\0*.schem\0schematic\0*.schematic\0",
	.import_func = import_as_schem
)
