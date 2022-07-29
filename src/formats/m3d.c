/*
 * Model 3D .m3d format support.
 *
 * Model 3D is an application and engine netural 3D model format capable of
 * storing voxel images, see https://bztsrc.gitlab.io/model3d
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

// get stbi_zlib_compress()
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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
	// .import_func = import_as_m3d,
	.export_func = export_as_m3d
)
