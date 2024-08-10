/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "file_format.h"
#include "goxel.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-const-int-float-conversion"
#pragma GCC diagnostic ignored "-Wnan-infinity-disabled"
#define VOXELIZER_IMPLEMENTATION
#include "../ext_src/voxelizer/voxelizer.h"
#pragma GCC diagnostic pop

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../ext_src/tinyobjloader/tinyobj_loader_c.h"

typedef struct {
    union {
        struct {
            float    v[3];
            uint8_t  c[3];
        };
        float vn[3];
        struct {
            int  vs[4];
            int vns[4];
        };
    };
} line_t;

typedef struct {
    bool y_up;
} export_options_t;

static export_options_t g_export_options = {
    .y_up = true,
};

static UT_icd line_icd = {sizeof(line_t), NULL, NULL, NULL};

static int lines_find(UT_array *lines, const line_t *line, int search_nb)
{
    int i, len;
    line_t *l;
    len = utarray_len(lines);
    for (i = len - 1; (i >= 0) && (i > len - 1 - search_nb); i--) {
        l = (line_t*)utarray_eltptr(lines, i);
        if (memcmp(l, line, sizeof(*line)) == 0) return i + 1;
    }
    return 0;
}

/*
 * Function: lines_add
 * Add a line entry into a list and return its index.
 *
 * Parameters:
 *   lines      - The list.
 *   line       - The new line we want to add.
 *   search_nb  - How far into the list we search for a similar line.  If
 *                a similar line is found we just return its index insead
 *                of adding a new one.
 */
static int lines_add(UT_array *lines, const line_t *line, int search_nb)
{
    int idx;
    idx = lines_find(lines, line, search_nb);
    if (idx) return idx;
    utarray_push_back(lines, line);
    return utarray_len(lines);
}

static int export(const volume_t *volume, const char *path, bool ply)
{
    // XXX: Merge faces that can be merged into bigger ones.
    //      Allow to chose between quads or triangles.
    //      Also export mlt file for the colors.
    voxel_vertex_t* verts;
    float v[3];
    uint8_t c[3];
    int nb_elems, i, j, bpos[3];
    float mat[4][4];
    FILE *out;
    const int N = BLOCK_SIZE;
    int size = 0, subdivide;
    UT_array *lines_f, *lines_v, *lines_vn;
    line_t line, face, *line_ptr = NULL;
    volume_iterator_t iter;
    static const float ZUP2YUP[4][4] = {
        {1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1},
    };

    utarray_new(lines_f, &line_icd);
    utarray_new(lines_v, &line_icd);
    utarray_new(lines_vn, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){};
    iter = volume_get_iterator(volume,
            VOLUME_ITER_TILES | VOLUME_ITER_INCLUDES_NEIGHBORS);
    while (volume_iter(&iter, bpos)) {
        mat4_set_identity(mat);
        if (g_export_options.y_up) {
            mat4_mul(ZUP2YUP, mat, mat);
        }
        mat4_itranslate(mat, bpos[0], bpos[1], bpos[2]);
        nb_elems = volume_generate_vertices(volume, bpos,
                                    goxel.rend.settings.effects, verts,
                                    &size, &subdivide);
        for (i = 0; i < nb_elems; i++) {
            // Put the vertices.
            for (j = 0; j < size; j++) {
                v[0] = verts[i * size + j].pos[0] / (float)subdivide;
                v[1] = verts[i * size + j].pos[1] / (float)subdivide;
                v[2] = verts[i * size + j].pos[2] / (float)subdivide;
                mat4_mul_vec3(mat, v, v);
                memcpy(c, verts[i * size + j].color, 3);
                line = (line_t){
                    .v = {v[0], v[1], v[2]}, .c = {c[0], c[1], c[2]}};
                // XXX: not sure about the search nb value to use here.
                face.vs[j] = lines_add(lines_v, &line, 1024);
            }
            // Put the normals
            for (j = 0; j < size; j++) {
                v[0] = verts[i * size + j].normal[0];
                v[1] = verts[i * size + j].normal[1];
                v[2] = verts[i * size + j].normal[2];
                mat4_mul_dir3(mat, v, v);
                line = (line_t){.vn = {v[0], v[1], v[2]}};
                face.vns[j] = lines_add(lines_vn, &line, 512);
            }
            lines_add(lines_f, &face, 0);
        }
    }
    out = fopen(path, "w");
    if (ply) {
        fprintf(out, "ply\n");
        fprintf(out, "format ascii 1.0\n");
        fprintf(out, "comment Generated from Goxel " GOXEL_VERSION_STR "\n");
        fprintf(out, "element vertex %d\n", utarray_len(lines_v));
        fprintf(out, "property float x\n");
        fprintf(out, "property float y\n");
        fprintf(out, "property float z\n");
        fprintf(out, "property float red\n");
        fprintf(out, "property float green\n");
        fprintf(out, "property float blue\n");
        fprintf(out, "element face %d\n", utarray_len(lines_f));
        fprintf(out, "property list uchar int vertex_indices\n");
        fprintf(out, "end_header\n");
        while( (line_ptr = (line_t*)utarray_next(lines_v, line_ptr))) {
            fprintf(out, "%g %g %g %f %f %f\n",
                    line_ptr->v[0], line_ptr->v[1], line_ptr->v[2],
                    line_ptr->c[0] / 255.,
                    line_ptr->c[1] / 255.,
                    line_ptr->c[2] / 255.);
        }
        while( (line_ptr = (line_t*)utarray_next(lines_f, line_ptr))) {
            if (size == 4) {
                fprintf(out, "4 %d %d %d %d\n", line_ptr->vs[0] - 1,
                                                line_ptr->vs[1] - 1,
                                                line_ptr->vs[2] - 1,
                                                line_ptr->vs[3] - 1);
            } else {
                fprintf(out, "3 %d %d %d\n",    line_ptr->vs[0] - 1,
                                                line_ptr->vs[1] - 1,
                                                line_ptr->vs[2] - 1);
            }
        }
    } else {
        fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
        while( (line_ptr = (line_t*)utarray_next(lines_v, line_ptr))) {
            fprintf(out, "v %g %g %g %f %f %f\n",
                    line_ptr->v[0], line_ptr->v[1], line_ptr->v[2],
                    line_ptr->c[0] / 255.,
                    line_ptr->c[1] / 255.,
                    line_ptr->c[2] / 255.);
        }
        while( (line_ptr = (line_t*)utarray_next(lines_vn, line_ptr))) {
            fprintf(out, "vn %g %g %g\n",
                    line_ptr->vn[0], line_ptr->vn[1], line_ptr->vn[2]);
        }
        while( (line_ptr = (line_t*)utarray_next(lines_f, line_ptr))) {
            if (size == 4) {
                fprintf(out, "f %d//%d %d//%d %d//%d %d//%d\n",
                             line_ptr->vs[0], line_ptr->vns[0],
                             line_ptr->vs[1], line_ptr->vns[1],
                             line_ptr->vs[2], line_ptr->vns[2],
                             line_ptr->vs[3], line_ptr->vns[3]);
            } else {
                fprintf(out, "f %d//%d %d//%d %d//%d\n",
                             line_ptr->vs[0], line_ptr->vns[0],
                             line_ptr->vs[1], line_ptr->vns[1],
                             line_ptr->vs[2], line_ptr->vns[2]);
            }
        }
    }
    fclose(out);
    utarray_free(lines_f);
    utarray_free(lines_v);
    utarray_free(lines_vn);
    free(verts);
    return 0;
}

static int wavefront_export(const file_format_t *format,
                            const image_t *image, const char *path)
{
    const volume_t *volume = goxel_get_layers_volume(image);
    return export(volume, path, false);
}

int ply_export(const file_format_t *format, const image_t *image,
               const char *path)
{
    const volume_t *volume = goxel_get_layers_volume(image);
    return export(volume, path, true);
}

static void export_gui(file_format_t *format)
{
    gui_checkbox(_("Y Up"), &g_export_options.y_up, _("Use +Y up convention"));
}

static void get_file_data(void *ctx, const char *filename, const int is_mtl,
                          const char *obj_filename, char **data, size_t *len)
{
    int size;

    if (!filename) {
        (*data) = NULL;
        (*len) = 0;
        return;
    }
    *data = read_file(filename, &size);
    *len = size;
}

static float g_resolution = 1.0;

static void import_gui(file_format_t *format)
{
    gui_dummy(200, 0); // Just to fix the width.
    gui_input_float("Resolution", &g_resolution, 0.01, 0, 100000, "%.3f");
}

static int wavefront_import(const file_format_t *format, image_t *image,
                            const char *path)
{
    int err;
    tinyobj_attrib_t attrib;
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes;
    tinyobj_material_t *materials = NULL;
    size_t num_materials;
    int i, pos[3];
    uint8_t color[4];
    float res = g_resolution;
    unsigned int flags;
    vx_mesh_t *mesh;
    vx_point_cloud_t *cloud;
    volume_iterator_t iter = {0};
    layer_t *layer;

    // XXX TODO: free the file data at the end!
    flags = TINYOBJ_FLAG_TRIANGULATE;
    err = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials,
                            &num_materials, path, get_file_data, NULL, flags);
    if (err != TINYOBJ_SUCCESS) {
        LOG_E("Cannot load %s", path);
        return -1;
    }

    mesh = vx_mesh_alloc(attrib.num_vertices, attrib.num_faces);
    for (i = 0; i < attrib.num_vertices; i++) {
        mesh->vertices[i].x = attrib.vertices[i * 3 + 0];
        mesh->vertices[i].z = attrib.vertices[i * 3 + 1];
        mesh->vertices[i].y = -attrib.vertices[i * 3 + 2];
        mesh->colors[i].r = 255;
        mesh->colors[i].g = 255;
        mesh->colors[i].b = 255;
    }
    for (i = 0; i < attrib.num_faces; i++) {
        mesh->indices[i] = attrib.faces[i].v_idx;
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);

    cloud = vx_voxelize_pc(mesh, res, res, res, res * 0.1);

    layer = image_add_layer(image, NULL);
    for (i = 0; i < cloud->nvertices; i++) {
        pos[0] = floor(cloud->vertices[i].x / res);
        pos[1] = floor(cloud->vertices[i].y / res);
        pos[2] = floor(cloud->vertices[i].z / res);
        color[0] = cloud->colors[i].r;
        color[1] = cloud->colors[i].g;
        color[2] = cloud->colors[i].b;
        color[3] = 255;
        volume_set_at(layer->volume, &iter, pos, color);
    }

    vx_point_cloud_free(cloud);

    return 0;
}

FILE_FORMAT_REGISTER(obj,
    .name = "obj",
    .exts = {"*.obj"},
    .exts_desc = "obj",
    .export_gui = export_gui,
    .export_func = wavefront_export,
    .import_func = wavefront_import,
    .import_gui = import_gui,
)

FILE_FORMAT_REGISTER(ply,
    .name = "ply",
    .exts = {"*.ply"},
    .exts_desc = "ply",
    .export_gui = export_gui,
    .export_func = ply_export,
)
