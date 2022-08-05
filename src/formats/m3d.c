/*
 * Model 3D .m3d format support.
 *
 * Model 3D is an application and engine netural 3D model format capable of
 * storing voxel images, see https://bztsrc.gitlab.io/model3d
*/

/**
 * goxel specific private chunks
 *
 * NOTE: I could have used separate chunks for each value, like '.gox'
 * does, but that would result in a considerably larger file size. For
 * future improvements, the chunk also stores the goxel version, so files
 * saved by old versions can be parsed consistently (irrelevant for now).
 *
 * image meta data (single chunk)
 * offset       size        description
 *      0          4        magic 'gxel'
 *      4          4        chunk size (32 or n)
 *      8          3        goxel version (bytes 0,15,0 as of writing)
 *     11          1        light fixed flag
 *     12          4        float, light pitch
 *     16          4        float, light yaw
 *     20          4        float, light intensity
 *     24          4        float, light ambient
 *     28          4        float, light shadow
 *     32     n - 32        img->box struct, might be missing
 *
 * camera data (multiple chunk)
 * offset       size        description
 *      0          4        magic 'gxcm'
 *      4          4        chunk size (82)
 *      8          4        camera name, offset to string table or 0
 *     12          1        orthographic flag
 *     13          1        active flag
 *     14          4        float, distance
 *     18         64        4 * 4 floats, transformation matrix
 *
 * layer meta data (multiple chunk, always saved right after VOXD chunks)
 * offset       size        description
 *      0          4        magic 'gxlr'
 *      4          4        chunk size
 *      8          4        id
 *     12          4        base_id
 *     16         64        4 * 4 floats, transformation matrix
 *     80          2        image_path length (p)
 *     82          p        image_path
 * 82 + p          1        box size (b)
 * 83 + p          b        box data
 * 83 +p+b         1        shape size (s, includes color size)
 * 84 +p+b     s - 4        shape name
 * 84 +p+b+s       4        shape color (always RGBA, no cmap)
 */
#define M3D_SAVE_PREVIEW 0
#define M3D_DEBUGCHUNKS  0   /* set this to 1 to avoid compression */

#include "goxel.h"
#include "file_format.h"
#include <errno.h>

// XXX: should be something in goxel.h
// note from bzt: couldn't agree more. I've just copied from gox.c for now
static const shape_t *SHAPES[] = {
	&shape_sphere,
	&shape_cube,
	&shape_cylinder,
};

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

#define MTRLSIZ 32  /* size of one material chunk */
#define GXELSIZ 32  /* size of one image meta chunk */
#define GXCMSIZ 82  /* size of one camera chunk */

// material types
enum {
	m3dp_Kd = 0,                /* scalar display properties */
	m3dp_Ka,
	m3dp_Ks,
	m3dp_Ns,
	m3dp_Ke,
	m3dp_Tf,
	m3dp_Km,
	m3dp_d,
	m3dp_il,

	m3dp_Pr = 64,               /* scalar physical properties */
	m3dp_Pm,
	m3dp_Ps,
	m3dp_Ni,
	m3dp_Nt,

	m3dp_map_Kd = 128,          /* textured display map properties */
	m3dp_map_Ka,
	m3dp_map_Ks,
	m3dp_map_Ns,
	m3dp_map_Ke,
	m3dp_map_Tf,
	m3dp_map_Km, /* bump map */
	m3dp_map_D,
	m3dp_map_N,  /* normal map */

	m3dp_map_Pr = 192,          /* textured physical map properties */
	m3dp_map_Pm,
	m3dp_map_Ps,
	m3dp_map_Ni,
	m3dp_map_Nt
};

/*** m3d reader ***/

static int import_as_m3d(image_t *image, const char *path)
{
	FILE *file;
	layer_t *layer, *layer_tmp;
	camera_t *camera, *camera_tmp;
	material_t *material, *mat_tmp;
	mesh_iterator_t iter = {0};
	long int size;
	unsigned char *data, *buf, *s, *e, *chunk;
	char *strtbl, *n;
	int i, j, k, l, x, y, z, ci_s, si_s, sk_s, vd_s, vp_s, nt;
	int sx, sy, sz, px, py, pz, pos[3];
	int aabb[2][3];
	uint32_t *cmap = NULL, C;
	uint64_t *palette = NULL;
	uint8_t black[4] = { 0, 0, 0, 255 }, *c = (uint8_t*)&C;

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

	// Remove all layers, materials and camera.
	// XXX: should have a way to create a totally empty image instead.
	// note from bzt: copied from gox.c, why not goxel_reset()?
	DL_FOREACH_SAFE(goxel.image->layers, layer, layer_tmp) {
		mesh_delete(layer->mesh);
		free(layer);
	}
	DL_FOREACH_SAFE(goxel.image->materials, material, mat_tmp) {
		material_delete(material);
	}
	DL_FOREACH_SAFE(goxel.image->cameras, camera, camera_tmp) {
		camera_delete(camera);
	}

	goxel.image->layers = NULL;
	goxel.image->materials = NULL;
	goxel.image->active_material = NULL;
	goxel.image->cameras = NULL;
	goxel.image->active_camera = NULL;

	// The file might not have a gxel chunk with box info, and clearing
	// the box would shrink the canvas. Not sure we want this.
	//memset(&goxel.image->box, 0, sizeof(goxel.image->box));

	// iterate on chunks, simply skip those we don't care about
	layer = NULL;
	for(chunk = s; chunk < e && memcmp(chunk, "OMD3", 4);) {
		// decode chunk header and adjust to the next chunk
		s = chunk;
		l = *((uint32_t*)(chunk + 4));
		chunk += l;
		if(l < 8 || chunk >= e) break;
		l -= 8;
		// goxel specific image meta data
		if(!memcmp(s, "gxel", 4) && l >= GXELSIZ) {
			goxel.rend.light.fixed = s[11];
			memcpy(&goxel.rend.light.pitch, s + 12, 4);
			memcpy(&goxel.rend.light.yaw, s + 16, 4);
			memcpy(&goxel.rend.light.intensity, s + 20, 4);
			memcpy(&goxel.rend.settings.ambient, s + 24, 4);
			memcpy(&goxel.rend.settings.shadow, s + 28, 4);
			if(l >= GXELSIZ + sizeof(goxel.image->box))
				memcpy(&goxel.image->box, s + GXELSIZ, sizeof(goxel.image->box));
		} else
		// goxel specific camera data
		if(!memcmp(s, "gxcm", 4) && l >= GXCMSIZ) {
			camera = camera_new("unnamed");
			DL_APPEND(goxel.image->cameras, camera);
			// we always save 4 bytes string offsets, but m3d files
			// might have different sizes too, so be prepared
			s += 8; n = NULL;
			switch(si_s) {
				case 1: n = strtbl + s[0]; break;
				case 2: n = strtbl + *((uint16_t*)s); break;
				case 4: n = strtbl + *((uint32_t*)s); break;
			}
			s += si_s;
			if(n)
				strncpy(camera->name, n, sizeof(camera->name) - 1);
			camera->ortho = *s++;
			if(*s++) goxel.image->active_camera = camera;
			memcpy(&camera->dist, s, 4); s += 4;
			memcpy(&camera->mat, s, 64);
		} else
		// if it's a color map (not saved, but m3d files might have it)
		if(!memcmp(s, "CMAP", 4)) { cmap = (uint32_t*)(s + 8); } else
		// material chunk
		if(!memcmp(s, "MTRL", 4)) {
			material = image_add_material(goxel.image, NULL);
			s += 8; n = NULL;
			switch(si_s) {
				case 1: n = strtbl + s[0]; break;
				case 2: n = strtbl + *((uint16_t*)s); break;
				case 4: n = strtbl + *((uint32_t*)s); break;
			}
			s += si_s;
			if(n && *n)
				strncpy(material->name, n, sizeof(material->name) - 1);
			// parse material properties
			while(s < chunk) {
				switch(s[0]) {
					/* metallic, float */
					case m3dp_Pm:
						memcpy(&material->metallic, s + 1, 4);
						s += 5;
					break;
					/* roughness, float */
					case m3dp_Pr:
						memcpy(&material->roughness, s + 1, 4);
						s += 5;
					break;
					/* diffuse color */
					case m3dp_Kd:
						s++; C = 0;
						switch(ci_s) {
							case 1:  C = cmap ? cmap[s[0]] : 0; s++; break;
							case 2:  C = cmap ? cmap[*((uint16_t*)s)] : 0; s += 2; break;
							case 4:  C = *((uint32_t*)s); s += 4; break;
						}
						material->base_color[0] = c[0] / 255.0;
						material->base_color[1] = c[1] / 255.0;
						material->base_color[2] = c[2] / 255.0;
						material->base_color[3] = c[3] / 255.0;
					break;
					/* emission color */
					case m3dp_Ke:
						s++; C = 0;
						switch(ci_s) {
							case 1:  C = cmap ? cmap[s[0]] : 0; s++; break;
							case 2:  C = cmap ? cmap[*((uint16_t*)s)] : 0; s += 2; break;
							case 4:  C = *((uint32_t*)s); s += 4; break;
						}
						material->emission[0] = c[0] / 255.0;
						material->emission[1] = c[1] / 255.0;
						material->emission[2] = c[2] / 255.0;
					break;
					/* skip over properties we don't care about */
					case m3dp_Ka: case m3dp_Ks: case m3dp_Tf:
						s += 1 + ci_s;
					break;
					default:
						s += 1 + (s[0] >= 128 ? si_s : 4);
					break;
				}
			}
		} else
		// voxel types
		if(!memcmp(s, "VOXT", 4)) {
			s += 8;
			// this will get an upper bound of number of types
			nt = l / (ci_s + si_s + 3 + sk_s);
			palette = (uint64_t *)malloc(nt * sizeof(uint64_t));
			if(palette) {
				memset(palette, 0, nt * sizeof(uint64_t));
				// get voxel types
				for(i = 0; i < nt && s < chunk; i++) {
					switch(ci_s) {
						case 1:  palette[i] = cmap ? cmap[s[0]] : 0; s++; break;
						case 2:  palette[i] = cmap ? cmap[*((uint16_t*)s)] : 0; s += 2; break;
						case 4:  palette[i] = *((uint32_t*)s); s += 4; break;
					}
					// copy the material index to the palette entry's upper 32 bits
					memcpy((char*)&palette[i] + 4, s, si_s);
					// skip over additional attributes
					s += si_s + 2;
					j = *s;
					s += 1 + sk_s + j * (2 + si_s);
				}
				// if we actually have less types than the upper bound
				if(i != nt) {
					nt = i;
					palette = (uint64_t *)realloc(palette, nt * sizeof(uint64_t));
				}
			}
		} else
		// voxel data
		if(!memcmp(s, "VOXD", 4)) {
			layer = image_add_layer(goxel.image, NULL);
			iter = mesh_get_accessor(layer->mesh);
			memset(layer->name, 0, sizeof(layer->name));
			// workaround a goxel bug
			layer->material = NULL;
			// m3d files might have layer names
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
			layer->visible = s[0] ? 0 : 1;
			// decompress RLE and set colors in mesh
			x = y = z = 0; n = NULL;
			for(s += 2, i = 0; s < chunk && i < sx * sy * sz;) {
				l = ((*s++) & 0x7F) + 1;
				if(s[-1] & 0x80) {
					if(vp_s == 1) { k = *s++; } else { k = *((uint16_t*)s); s += 2; }
					for(j = 0; j < l; j++, i++) {
						if(k >= 0 && k < nt) {
							pos[0] = px + x; pos[1] = -(pz + z); pos[2] = py + y;
							mesh_set_at(layer->mesh, &iter, pos,
								palette ? (uint8_t*)&palette[k] : (uint8_t*)&black);
							if(!n && palette && (palette[k] >> 32) > 0)
								n = strtbl + (palette[k] >> 32);
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
							if(!n && palette && (palette[k] >> 32) > 0)
								n = strtbl + (palette[k] >> 32);
						}
						x++; if(x >= sx) { x = 0; z++; if(z >= sz) { z = 0; y++; } }
					}
				}
			}
			// m3d allows multiple materials on the same layer, but goxel doesn't.
			// so we simply set the first voxel's type which has a material, this
			// will work for all m3d files exported from goxel, and reasonable for
			// other m3d files with multiple materials
			if(n && !layer->material) {
				DL_FOREACH(goxel.image->materials, material) {
					if(!strcmp(material->name, n)) {
						layer->material = material;
						break;
					}
				}
			}
			// goxel specific layer data (must follow VOXD chunk)
			if(!memcmp(chunk, "gxlr", 4)) {
				s = chunk;
				l = *((uint32_t*)(chunk + 4));
				chunk += l;
				if(l < 8 || chunk >= e) break;
				if(l >= 82) {
					s += 8;
					// these are always saved
					memcpy(&layer->id, s, 4); s += 4;
					memcpy(&layer->base_id, s, 4); s += 4;
					memcpy(&layer->mat, s, 64); s += 64;
					// image path might be empty
					l = (s[1] << 8) | s[0]; s += 2;
					if(s + l < chunk && l)
						layer->image = texture_new_image((char*)s, TF_NEAREST);
					s += l;
					// box might be empty
					l = *s++;
					if(s + l < chunk && l == 64)
						memcpy(&layer->box, s, 64);
					s += l;
					// shape name and color might be missing
					l = *s++;
					if(s + l < chunk && l > 5) {
						for (i = 0; i < ARRAY_SIZE(SHAPES); i++) {
							if (strcmp(SHAPES[i]->id, (char*)s) == 0) {
								layer->shape = SHAPES[i];
								break;
							}
						}
						memcpy(&layer->color, s + l - 4, 4);
					}
					s += l;
				}
			}
		}
	}
	if(palette) free(palette);
	free(data);

	// gox.c line 650 does this, however this leaks memory
	//goxel.image->path = strdup(path);
	goxel.image->saved_key = image_get_key(goxel.image);

	// Add a default camera if there is none.
	if (!goxel.image->cameras) {
		image_add_camera(goxel.image, NULL);
		camera_fit_box(goxel.image->active_camera, goxel.image->box);
	}

	// Set default image box if we didn't have one.
	if (box_is_null(goxel.image->box)) {
		mesh_get_bbox(goxel_get_layers_mesh(goxel.image), aabb, true);
		if (aabb[0][0] > aabb[1][0]) {
			aabb[0][0] = -16;
			aabb[0][1] = -16;
			aabb[0][2] = 0;
			aabb[1][0] = 16;
			aabb[1][1] = 16;
			aabb[1][2] = 32;
		}
		bbox_from_aabb(goxel.image->box, aabb);
	}

	// Update plane, snap mask not to confuse people.
	plane_from_vectors(goxel.plane, goxel.image->box[3],
					   VEC(1, 0, 0), VEC(0, 1, 0));

	return 0;
}

/*** m3d writer ***/

/* string table stuff. */
typedef struct {
	char *data;     // concatenated, escape-safe string buffer
	int len, num;   // length and number of strings
	int *str;       // string offsets in data
} m3d_strtable_t;

/* remove unsafe characters from identifiers (tab, newline, directory separators etc.) */
char *m3d_safestr(const char *in, int morelines)
{
	char *out, *o, *i = (char*)in;
	int l;
	if(!in || !*in) {
		out = (char*)malloc(1);
		if(!out) return NULL;
		out[0] =0;
	} else {
		for(o = (char*)in, l = 0; *o && ((morelines & 1) || (*o != '\r' && *o != '\n')) && l < 256; o++, l++);
		out = o = (char*)malloc(l+1);
		if(!out) return NULL;
		while(*i == ' ' || *i == '\t' || *i == '\r' || (morelines && *i == '\n')) i++;
		for(; *i && (morelines || (*i != '\r' && *i != '\n')); i++) {
			if(*i == '\r') continue;
			if(*i == '\n') {
				if(morelines >= 3 && o > out && *(o-1) == '\n') break;
				if(i > in && *(i-1) == '\n') continue;
				if(morelines & 1) {
					if(morelines == 1) *o++ = '\r';
					*o++ = '\n';
				} else
					break;
			} else
			if(*i == ' ' || *i == '\t') {
				*o++ = morelines? ' ' : '_';
			} else
				*o++ = !morelines && (*i == '/' || *i == '\\') ? '_' : *i;
		}
		for(; o > out && (*(o-1) == ' ' || *(o-1) == '\t' || *(o-1) == '\r' || *(o-1) == '\n'); o--);
		*o = 0;
		out = (char*)realloc(out, (uintptr_t)o - (uintptr_t)out + 1);
	}
	return out;
}

/* add a string to string table */
void m3d_addstr(m3d_strtable_t *tbl, const char *str)
{
	int i, l;
	char *safe = m3d_safestr(str, 0);

	if(!safe) return;
	// first 4 strings are mandatory: title, license, author, comment
	if(tbl->num > 3) {
		for(i = 4; i < tbl->num && tbl->data && strcmp(tbl->data + tbl->str[i], safe); i++);
		if(tbl->data && i < tbl->num) { free(safe); return; }
	}
	l = strlen(safe) + 1;
	tbl->data = (char*)realloc(tbl->data, tbl->len + l);
	if(!tbl->data) { free(safe); return; }
	i = tbl->num++;
	tbl->str = (int*)realloc(tbl->str, tbl->num * sizeof(int));
	if(!tbl->str) { free(safe); return; }
	tbl->str[i] = tbl->len;
	memcpy(tbl->data + tbl->len, safe, l);
	tbl->len += l;
	free(safe);
}

/* return string table offset index for a string */
int m3d_getstr(m3d_strtable_t *tbl, const char *str)
{
	int i;
	char *safe;

	if(!tbl->data || !tbl->str) return 0;
	safe = m3d_safestr(str, 0);
	if(!safe) return 0;
	for(i = 0; i < tbl->num && strcmp(tbl->data + tbl->str[i], safe); i++);
	free(safe);
	return i < tbl->num ? i : 0;
}

/* free string table */
void m3d_freestr(m3d_strtable_t *tbl)
{
	if(tbl->data) free(tbl->data);
	if(tbl->str) free(tbl->str);
}

#define WRITE(type, v, file) \
	({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})

static int export_as_m3d(const image_t *img, const char *path)
{
	FILE *out;
	uint8_t *data = NULL, *comp = NULL, *chunk;
	uint16_t *blk;
	uint32_t *intp;
	uint64_t *palette = NULL; /* lower 32 bits: RGBA, upper 32 bit: material index */
	uint64_t n;
	camera_t *camera;
	layer_t *layer;
	material_t *material;
	mesh_iterator_t iter;
	m3d_strtable_t str;
	int size = 0, num_palette = 0, i, k, l, o, pos[3];
	int matnum = 0, camnum = 0, lyrnum = 0;
	uint8_t v[8], *ptr;
	int xmin, ymin, zmin, xmina, ymina, zmina;
	int xmax, ymax, zmax, xmaxa, ymaxa, zmaxa;
	int xsiz, ysiz, zsiz;

	// write out file
	LOG_I("Save to %s", path);
	img = img ?: goxel.image;
	out = fopen(path, "wb");
	if (!out) {
		LOG_E("Cannot save to %s: %s", path, strerror(errno));
		return -1;
	}
	fwrite("3DMO", 4, 1, out);

	// construct the string table
	memset(&str, 0, sizeof(m3d_strtable_t));
	// these are not stored by goxel, but we must save them
	m3d_addstr(&str, "");   // title, model name
	m3d_addstr(&str, "");   // license
	m3d_addstr(&str, "");   // author
	m3d_addstr(&str, "");   // comment
	DL_FOREACH(img->cameras, camera) {
		m3d_addstr(&str, camera->name);
		camnum++;
	}
	DL_FOREACH(img->materials, material) {
		m3d_addstr(&str, material->name);
		matnum++;
	}

	// iterate through layers to get the palette and overall dimension
	// (as well as layer names)
	xmina = ymina = zmina = INT_MAX;
	xmaxa = ymaxa = zmaxa = INT_MIN;
	size = 0;
	DL_FOREACH(img->layers, layer) {
		m3d_addstr(&str, layer->name);
		iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
		memset(&v, 0, 8);
		if(str.str && layer->material)
			memcpy(&v[4], &str.str[m3d_getstr(&str, layer->material->name)], 4);
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
				if(!memcmp(&palette[i], v, 8)) break;
			}
			if(i >= num_palette) {
				i = num_palette++;
				palette = realloc(palette, num_palette * 8);
				if(!palette) {
memerr:             m3d_freestr(&str);
					if(data) free(data);
					if(palette) free(palette);
					LOG_E("Memory allocation error %s", path);
					return -1;
				}
				memcpy(&palette[i], v, 8);
			}
		}
		// add optional strings' lengths
		if(layer->image && layer->image->path)
			size += strlen(layer->image->path) + 3;
		if(!box_is_null(layer->box))
			size += sizeof(layer->box) + 1;
		if(layer->shape)
			size += strlen(layer->shape->id) + 6;
		lyrnum++;
	}
	if(num_palette >= 65534) {
		m3d_freestr(&str);
		if(palette) free(palette);
		LOG_E("Cannot save, too many palette entries (max 65534) %s: %d",
			path, num_palette);
		return -1;
	}
	// do not center on Y axis
	xmina += (xmaxa - xmina) / 2;
	zmina += (zmaxa - zmina) / 2;

	size += 16 + str.len +          /* header size */
		8 + num_palette * 11 +      /* voxel type chunk size */
		GXELSIZ + sizeof(img->box) +/* goxel specific image meta */
		matnum * MTRLSIZ +          /* total size of material chunks */
		camnum * GXCMSIZ;           /* total size of camera chunks */
		/* voxel data chunk reallocated and size calculated dynamically */
	data = calloc(size, 1);
	if(!data) goto memerr;

	// construct model header
	intp = (uint32_t*)(data + 0);
	memcpy(data + 0, "HEAD", 4);    // chunk magic
	intp[1] = 16 + str.len;         // chunk size
	intp[2] = 0x3F800000;           // scale
	intp[3] = 0x018FCFA0;           // flags
	chunk = data + 16 + str.len;
	if(str.data)
		memcpy(data + 16, str.data, str.len);

	// goxel specific meta chunk
	memcpy(chunk, "gxel", 4);   // chunk magic
	i = GXELSIZ;
	sscanf(GOXEL_VERSION_STR, "%u.%u.%u", &k, &l, &o);
	chunk[8] = o;
	chunk[9] = l;
	chunk[10] = k;
	chunk[11] = goxel.rend.light.fixed;
	memcpy(chunk + 12, &goxel.rend.light.pitch, 4);
	memcpy(chunk + 16, &goxel.rend.light.yaw, 4);
	memcpy(chunk + 20, &goxel.rend.light.intensity, 4);
	memcpy(chunk + 24, &goxel.rend.settings.ambient, 4);
	memcpy(chunk + 28, &goxel.rend.settings.shadow, 4);
	if (!box_is_null(img->box)) {
		i += sizeof(img->box);
		memcpy(chunk + GXELSIZ, &img->box, sizeof(img->box));
	}
	memcpy(chunk + 4, &i, 4);
	chunk += i;

	// goxel specific camera chunks
	DL_FOREACH(img->cameras, camera) {
		memcpy(chunk, "gxcm", 4);   // chunk magic
		i = GXCMSIZ; memcpy(chunk + 4, &i, 4);
		if(str.str)
			memcpy(chunk + 8, &str.str[m3d_getstr(&str, camera->name)], 4);
		chunk[12] = camera->ortho ? 1 : 0;
		chunk[13] = camera == img->active_camera ? 1 : 0;
		memcpy(chunk + 14, &camera->dist, 4);
		memcpy(chunk + 18, &camera->mat, 64);
		chunk += GXCMSIZ;
	}

	// materials
	DL_FOREACH(img->materials, material) {
		memcpy(chunk, "MTRL", 4);
		i = MTRLSIZ; memcpy(chunk + 4, &i, 4);
		/* material name index */
		if(str.str)
			memcpy(chunk + 8, &str.str[m3d_getstr(&str, material->name)], 4);
		/* diffuse color */
		chunk[12] = m3dp_Kd;
		chunk[13] = (int)(material->base_color[0] * 255.0);
		chunk[14] = (int)(material->base_color[1] * 255.0);
		chunk[15] = (int)(material->base_color[2] * 255.0);
		chunk[16] = (int)(material->base_color[3] * 255.0);
		/* metallic, float */
		chunk[17] = m3dp_Pm;
		memcpy(chunk + 18, &material->metallic, 4);
		/* roughness, float */
		chunk[22] = m3dp_Pr;
		memcpy(chunk + 23, &material->roughness, 4);
		/* emission color */
		chunk[27] = m3dp_Ke;
		chunk[28] = (int)(material->emission[0] * 255.0);
		chunk[29] = (int)(material->emission[1] * 255.0);
		chunk[30] = (int)(material->emission[2] * 255.0);
		chunk[31] = 255; /* goxel does not store alpha for emission */
		chunk += MTRLSIZ;
	}

	// voxel types chunk
	memcpy(chunk, "VOXT", 4);
	i = 8 + 11 * num_palette; memcpy(chunk + 4, &i, 4);
	chunk += 8;
	for(i = 0; i < num_palette; i++, chunk += 11)
		memcpy(chunk, &palette[i], 8);

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
		memset(&v, 0, 8);
		if(str.str && layer->material)
			memcpy(&v[4], &str.str[m3d_getstr(&str, layer->material->name)], 4);
		if(xmin < xmax && ymin < ymax && zmin < zmax) {
			// get layer data
			xsiz = xmax - xmin;
			ysiz = ymax - ymin;
			zsiz = zmax - zmin;
			n = xsiz * ysiz * zsiz;
			blk = malloc(n * 2);
			if(!blk) goto memerr;
			memset(blk, 0xFF, n * 2);
			iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
			while (mesh_iter(&iter, pos)) {
				mesh_get_at(layer->mesh, &iter, pos, v);
				if(!v[3]) continue;
				for(i = 0; i < num_palette; i++) {
					if(!memcmp(&palette[i], v, 8)) break;
				}
				// convert to right-handed coordinate system with indices
				blk[(pos[2] - ymin) * zsiz * xsiz +
					(zmax - pos[1] - 1) * xsiz +
					(pos[0] - xmin)] = i;
			}
			// voxel data chunk
			// allocate memory for the worst case, when every voxel is a packet
			// 38: min voxd chunk length, 88: min gxlr chunk length. size already
			// includes the optional gxlr string lengths.
			chunk -= (uintptr_t)data;
			data = realloc(data, size + 38 + n * 3 + 88);
			if(!data) { free(blk); goto memerr; }
			chunk += (uintptr_t)data;
			intp = (uint32_t*)(chunk);
			memset(chunk, 0, 38);
			memcpy(chunk, "VOXD", 4);   // chunk magic
			if(str.str)
				memcpy(chunk + 8, &str.str[m3d_getstr(&str, layer->name)], 4);
			i = xmin - xmina; memcpy(chunk + 12, &i, 4);
			i = ymin - ymina; memcpy(chunk + 16, &i, 4);
			i = zmin - zmina; memcpy(chunk + 20, &i, 4);
			memcpy(chunk + 24, &xsiz, 4);
			memcpy(chunk + 28, &ysiz, 4);
			memcpy(chunk + 32, &zsiz, 4);
			chunk[36] = layer->visible ? 0 : 255;
			// rle compression
			ptr = chunk + 38;
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
			free(blk);
			intp[1] = ptr + o - chunk;  // chunk size
			size += intp[1];
			chunk = data + size;
			intp = (uint32_t*)(chunk);
			// add goxel specific layer meta chunk right after VOXD chunk
			memcpy(chunk, "gxlr", 4);   // chunk magic
			ptr = chunk + 8;
			memcpy(ptr, &layer->id, 4); ptr += 4;
			memcpy(ptr, &layer->base_id, 4); ptr += 4;
			memcpy(ptr, &layer->mat, 64); ptr += 64;
			if(layer->image && layer->image->path) {
				l = strlen(layer->image->path);
				if(l > 65534) l = 65534;
				l++;
				memcpy(ptr, &l, 2); ptr += 2;
				memcpy(ptr, layer->image->path, l - 1); ptr += l - 1;
				*ptr++ = 0;
			} else {
				*ptr++ = 0;
				*ptr++ = 0;
			}
			if(!box_is_null(layer->box)) {
				*ptr++ = sizeof(layer->box);
				memcpy(ptr, &layer->box, sizeof(layer->box));
				ptr += sizeof(layer->box);
			} else
				*ptr++ = 0;
			if(layer->shape) {
				l = strlen(layer->shape->id);
				if(l > 250) l = 250;
				l++;
				*ptr++ = l; memcpy(ptr, layer->shape->id, l); ptr += l;
				*ptr++ = 0;
				memcpy(ptr, &layer->color, 4);
			}
			intp[1] = ptr - chunk;  // chunk size
			size += intp[1];
			chunk = ptr;
		}
	}
	free(palette);
	m3d_freestr(&str);

	// end chunk
	data = realloc(data, size + 4);
	memcpy(data + size, "OMD3", 4);
	size += 4;

#if M3D_DEBUGCHUNKS
	(void)comp;
#else
	// compress payload
	comp = stbi_zlib_compress(data, size, &i, 9);
	if(comp && i) {
		free(data);
		data = comp;
		size = i;
	}
#endif

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

static void _save_model_wrapper(void) {
    const char *path = goxel.image->path;
    if (!path) path = sys_get_save_path("m3d\0*.m3d\0", "untitled.m3d");
    if (!path) return;
    if (path != goxel.image->path) {
        free(goxel.image->path);
        goxel.image->path = strdup(path);
    }

    export_as_m3d(goxel.image, goxel.image->path);
    goxel.image->saved_key = image_get_key(goxel.image);
    sys_on_saved(path);
}

static void _new_model_wrapper(void) {
    goxel_reset();
}

static void _save_model_as_wrapper(void)
{
    const char *path;
    path = sys_get_save_path("m3d\0*.m3d\0", "untitled.m3d");
    if (!path) return;
    if (path != goxel.image->path) {
        free(goxel.image->path);
        goxel.image->path = strdup(path);
    }
    export_as_m3d(goxel.image, goxel.image->path);
    goxel.image->saved_key = image_get_key(goxel.image);
    sys_on_saved(path);
}

static void _open_model_wrapper(void)
{
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "m3d\0*.m3d\0", NULL, NULL);
    if (!path) return;
    image_delete(goxel.image);
    goxel.image = image_new();
    import_as_m3d(goxel.image, path);

    if (goxel.image->path != NULL)
	    free(goxel.image->path);

    goxel.image->path = strdup(path);
}

ACTION_REGISTER(open,
    .help = "Open an image",
    .cfunc = _open_model_wrapper,
    .default_shortcut = "Ctrl O",
)

ACTION_REGISTER(save_as,
    .help = "Save the image as",
    .cfunc = _save_model_as_wrapper,
)

ACTION_REGISTER(save,
    .help = "Save the image",
    .cfunc = _save_model_wrapper,
    .default_shortcut = "Ctrl S"
)

ACTION_REGISTER(reset,
    .help = "New",
    .cfunc = _new_model_wrapper,
    .default_shortcut = "Ctrl N"
)
