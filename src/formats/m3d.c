/*
 * Model 3D .m3d format support.
 *
 * Model 3D is an application and engine netural 3D model format capable of
 * storing voxel images & much more, see https://bztsrc.gitlab.io/model3d
 * Importer & Exporter Implemented By https://github.com/bztsrc
*/

#define M3D_SAVE_PREVIEW 0

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

// get stbi_zlib_compress()
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

static int import_as_m3d(image_t *image, const char *path)
{
	FILE *file;
	layer_t *layer;
	mesh_iterator_t iter = {0};
	long int size;
	unsigned char *data, *buf, *s, *e, *chunk;
	char *strtbl, *n;
	int i, j, k, l, x, y, z, ci_s, si_s, sk_s, vd_s, vp_s, nt = -1;
	int sx, sy, sz, px, py, pz, pos[3];
	uint32_t *cmap = NULL, *palette = NULL;
	uint8_t black[4] = { 0, 0, 0, 255 };

	// read in file data
	file = fopen(path, "rb");
	if(!file) {
		LOG_E("Cannot load from %s: %s", path, strerror(errno));
		return -1;
	}
	fseek(file, 0, SEEK_END);
	size = (long int)ftell(file);
	fseek(file, 0, SEEK_SET);
	data = (unsigned char*)malloc(size);
	if(!data) {
		fclose(file);
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
	// check magic and uncompress
	if(memcmp(data, "3DMO", 4)) {
		free(data);
		LOG_E("Bad file format %s", path);
		return -1;
	}
	// skip over header
	s = data + 8;
	e = data + size;
	size -= 8;
	// skip over optional preview chunk
	if(!memcmp(s, "PRVW", 4)) {
		size -= *((uint32_t*)(s + 4));
		s += *((uint32_t*)(s + 4));
	}
	// check if it's a header chunk, if not, then file is stream compressed
	if(memcmp(s, "HEAD", 4)) {
		buf = (unsigned char *)stbi_zlib_decode_malloc_guesssize_headerflag(
			(const char*)s, size, 4096, &l, 1);
		free(data);
		if(!buf || !l || memcmp(buf, "HEAD", 4)) {
			if(buf) free(buf);
			LOG_E("Uncompression error %s", path);
			return -1;
		}
		data = s = buf;
		e = data + l;
	}
	// decode item sizes from model header
	strtbl = (char*)s + 16;
	si_s = 1 << ((s[12] >> 4) & 3);
	ci_s = 1 << ((s[12] >> 6) & 3);
	sk_s = 1 << ((s[13] >> 6) & 3);
	vd_s = 1 << ((s[14] >> 6) & 3);
	vp_s = 1 << ((s[15] >> 0) & 3);
	if(ci_s == 8) ci_s = 0;
	if(sk_s == 8) sk_s = 0;
	if(si_s == 8) si_s = 0;
	if(vd_s == 8 || vp_s > 2) {
		free(data);
		LOG_E("Bad model header %s", path);
		return -1;
	}
	// iterate on chunks, simply skip those we don't care about
	for(chunk = s; chunk < e && memcmp(chunk, "OMD3", 4);) {
		// decode chunk header and adjust to the next chunk
		s = chunk;
		l = *((uint32_t*)(chunk + 4));
		chunk += l;
		if(l < 8 || chunk >= e) break;
		l -= 8;
		// if it's a color map (not saved, but m3d files might have it)
		if(!memcmp(s, "CMAP", 4)) { cmap = (uint32_t*)(s + 8); } else
		// voxel types
		if(!memcmp(s, "VOXT", 4)) {
			s += 8;
			// this will get an upper bound of number of types
			nt = l / (ci_s + si_s + 3 + sk_s);
			palette = (uint32_t *)malloc(nt * sizeof(uint32_t));
			if(palette) {
				memset(palette, 0, nt * sizeof(uint32_t));
				// get voxel types
				for(i = 0; i < nt && s < chunk; i++) {
					switch(ci_s) {
						case 1:  palette[i] = cmap ? cmap[s[0]] : 0; s++; break;
						case 2:  palette[i] = cmap ? cmap[*((uint16_t*)s)] : 0; s += 2; break;
						case 4:  palette[i] = *((uint32_t*)s); s += 4; break;
					}
					// skip over additional attributes
					s += si_s + 2;
					j = *s;
					s += 1 + sk_s + j * (2 + si_s);
				}
				// if we actually have less types than the upper bound
				if(i != nt) {
					nt = i;
					palette = (uint32_t *)realloc(palette, nt * sizeof(uint32_t));
				}
			}
		} else
		// voxel data
		if(!memcmp(s, "VOXD", 4)) {
			layer = image_add_layer(goxel.image, NULL);
			iter = mesh_get_accessor(layer->mesh);
			memset(layer->name, 0, sizeof(layer->name));
			// we don't save layer names, but m3d files might have it
			s += 8; n = NULL;
			switch(si_s) {
				case 1: n = strtbl + s[0]; break;
				case 2: n = strtbl + *((uint16_t*)s); break;
				case 4: n = strtbl + *((uint32_t*)s); break;
			}
			s += si_s;
			if(n)
				strncpy(layer->name, n, sizeof(layer->name) - 1);
			// get layer dimensions
			px = py = pz = sx = sy = sz = 0;
			switch(vd_s) {
				case 1:
					px = (int8_t)s[0]; py = (int8_t)s[1]; pz = (int8_t)s[2];
					sx = (int8_t)s[3]; sy = (int8_t)s[4]; sz = (int8_t)s[5];
					s += 6;
				break;
				case 2:
					px = *((int16_t*)(s+0)); py = *((int16_t*)(s+2)); pz = *((int16_t*)(s+4));
					sx = *((int16_t*)(s+6)); sy = *((int16_t*)(s+8)); sz = *((int16_t*)(s+10));
					s += 12;
				break;
				case 4:
					px = *((int32_t*)(s+0)); py = *((int32_t*)(s+4)); pz = *((int32_t*)(s+8));
					sx = *((int32_t*)(s+12)); sy = *((int32_t*)(s+16)); sz = *((int32_t*)(s+20));
					s += 24;
				break;
			}
			// decompress RLE and set colors in mesh
			x = y = z = 0;
			for(s += 2, i = 0; s < chunk && i < sx * sy * sz;) {
				l = ((*s++) & 0x7F) + 1;
				if(s[-1] & 0x80) {
					if(vp_s == 1) { k = *s++; } else { k = *((uint16_t*)s); s += 2; }
					for(j = 0; j < l; j++, i++) {
						if(k >= 0 && k < nt) {
							pos[0] = px + x; pos[1] = -(pz + z); pos[2] = py + y;
							mesh_set_at(layer->mesh, &iter, pos,
								palette ? (uint8_t*)&palette[k] : (uint8_t*)&black);
						}
						x++; if(x >= sx) { x = 0; z++; if(z >= sz) { z = 0; y++; } }
					}
				} else {
					for(j = 0; j < l; j++, i++) {
						if(vp_s == 1) { k = *s++; } else { k = *((uint16_t*)s); s += 2; }
						if(k >= 0 && k < nt) {
							pos[0] = px + x; pos[1] = -(pz + z); pos[2] = py + y;
							mesh_set_at(layer->mesh, &iter, pos,
								palette ? (uint8_t*)&palette[k] : (uint8_t*)&black);
						}
						x++; if(x >= sx) { x = 0; z++; if(z >= sz) { z = 0; y++; } }
					}
				}
			}
		}
	}
	if(palette) free(palette);
	free(data);
	return 0;
}

#define WRITE(type, v, file) \
	({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

static int export_as_m3d(const image_t *img, const char *path)
{
	FILE *out;
	uint8_t *data = NULL, *comp = NULL;
	uint16_t *blk;
	uint32_t *intp, *palette = NULL;
	layer_t *layer;
	mesh_iterator_t iter;
	int size = 0, num_palette = 0, i, k, l, n, o, pos[3];
	uint8_t v[4], *ptr, *hed;
	int xmin, ymin, zmin, xmina, ymina, zmina;
	int xmax, ymax, zmax, xmaxa, ymaxa, zmaxa;
	int xsiz, ysiz, zsiz;

	// write out file
	img = img ?: goxel.image;
	out = fopen(path, "wb");
	if (!out) {
		LOG_E("Cannot save to %s: %s", path, strerror(errno));
		return -1;
	}
	fwrite("3DMO", 4, 1, out);

	// iterate through layers to get the palette and overall dimension
	xmina = ymina = zmina = INT_MAX;
	xmaxa = ymaxa = zmaxa = INT_MIN;
	DL_FOREACH(img->layers, layer) {
		iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
		while (mesh_iter(&iter, pos)) {
			mesh_get_at(layer->mesh, &iter, pos, v);
			if(!v[3]) continue;
			xmina = min(xmina, pos[0]);
			ymina = min(ymina, pos[2]);
			zmina = min(zmina, pos[1]);
			xmaxa = max(xmaxa, pos[0] + 1);
			ymaxa = max(ymaxa, pos[2] + 1);
			zmaxa = max(zmaxa, pos[1] + 1);
			for(i = 0; i < num_palette; i++) {
				if(!memcmp(&palette[i], v, 4)) break;
			}
			if(i >= num_palette) {
				i = num_palette++;
				palette = realloc(palette, num_palette * 4);
				memcpy(&palette[i], v, 4);
			}
		}
	}
	// do not center on Y axis
	xmina += (xmaxa - xmina) / 2;
	zmina += (zmaxa - zmina) / 2;

	size = 20 + 8 * (num_palette + 1);
	data = calloc(size, 1);
	// construct model header
	intp = (uint32_t*)(data + 0);
	memcpy(data + 0, "HEAD", 4);    // chunk magic
	intp[1] = 20;                   // chunk size
	intp[2] = 0x3F800000;           // scale
	intp[3] = 0x014FCF80;           // flags
	// voxel types chunk
	intp = (uint32_t*)(data + 20);
	memcpy(data + 20, "VOXT", 4);   // chunk magic
	intp[1] = 8 * (num_palette + 1);// chunk size
	for(i = 0; i < num_palette; i++)
		intp[(i + 1) * 2] = palette[i];

	// iterate through layers to get voxels
	DL_FOREACH(img->layers, layer) {
		// get layer local dimensions
		xmin = ymin = zmin = INT_MAX;
		xmax = ymax = zmax = INT_MIN;
		iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
		while (mesh_iter(&iter, pos)) {
			mesh_get_at(layer->mesh, &iter, pos, v);
			if(!v[3]) continue;
			xmin = min(xmin, pos[0]);
			ymin = min(ymin, pos[2]);
			zmin = min(zmin, pos[1]);
			xmax = max(xmax, pos[0] + 1);
			ymax = max(ymax, pos[2] + 1);
			zmax = max(zmax, pos[1] + 1);
		}
		if(xmin < xmax && ymin < ymax && zmin < zmax) {
			// get layer data
			xsiz = xmax - xmin;
			ysiz = ymax - ymin;
			zsiz = zmax - zmin;
			n = xsiz * ysiz * zsiz;
			blk = malloc(n * 2);
			memset(blk, 0xFF, n * 2);
			iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
			while (mesh_iter(&iter, pos)) {
				mesh_get_at(layer->mesh, &iter, pos, v);
				if(!v[3]) continue;
				for(i = 0; i < num_palette; i++) {
					if(!memcmp(&palette[i], v, 4)) break;
				}
				// convert to right-handed coordinate system with indices
				blk[(pos[2] - ymin) * zsiz * xsiz +
					(zmax - pos[1] - 1) * xsiz +
					(pos[0] - xmin)] = i;
			}
			// voxel data chunk
			data = realloc(data, size + 23 + n * 3);
			hed = data + size;
			intp = (uint32_t*)(hed);
			memset(hed, 0, 23);
			memcpy(hed, "VOXD", 4);   // chunk magic
			i = xmin - xmina; memcpy(hed +  9, &i, 2);
			i = ymin - ymina; memcpy(hed + 11, &i, 2);
			i = zmin - zmina; memcpy(hed + 13, &i, 2);
			memcpy(hed + 15, &xsiz, 2);
			memcpy(hed + 17, &ysiz, 2);
			memcpy(hed + 19, &zsiz, 2);
			// rle compression
			ptr = hed + 23;
			k = o = 0; ptr[o++] = 0;
			for(i = 0; i < n; i++) {
				for(l = 1; l < 128 && i + l < n && blk[i] == blk[i + l]; l++);
				if(l > 1) {
					l--;
					if(ptr[k]) { ptr[k]--; ptr[o++] = 0x80 | l; }
					else ptr[k] = 0x80 | l;
					memcpy(ptr + o, &blk[i], 2);
					o += 2;
					k = o; ptr[o++] = 0;
					i += l;
					continue;
				}
				ptr[k]++;
				memcpy(ptr + o, &blk[i], 2);
				o += 2;
				if(ptr[k] > 127) { ptr[k]--; k = o; ptr[o++] = 0; }
			}
			if(!(ptr[k] & 0x80)) { if(ptr[k]) ptr[k]--; else o--; }
			intp[1] = ptr + o - hed;  // chunk size
			size += intp[1];
			free(blk);
		}
	}
	free(palette);
	// end chunk
	data = realloc(data, size + 4);
	memcpy(data + size, "OMD3", 4);
	size += 4;

	// compress payload
	comp = stbi_zlib_compress(data, size, &i, 9);
	if(comp && i) {
		free(data);
		data = comp;
		size = i;
	}

	// generate preview chunk
#if M3D_SAVE_PREVIEW
	if(num_palette) {
		ptr = calloc(128 * 128, 4);
		goxel_render_to_buf(ptr, 128, 128, 4);
		comp = img_write_to_mem(ptr, 128, 128, 4, &i);
		WRITE(uint32_t, 8 + size + 8 + i, out);
		fwrite("PRVW", 4, 1, out);
		WRITE(uint32_t, 8 + i, out);
		fwrite(comp, i, 1, out);
		free(ptr);
		free(comp);
	} else
#endif
	{
		WRITE(uint32_t, 8 + size, out);
	}
	// write out compressed chunks
	if(data) {
		fwrite(data, size, 1, out);
		free(data);
	}
	fclose(out);
	return 0;
}

FILE_FORMAT_REGISTER(m3d,
	.name = "Model3D",
	.ext = "m3d\0*.m3d\0",
	.import_func = import_as_m3d,
	.export_func = export_as_m3d
)
