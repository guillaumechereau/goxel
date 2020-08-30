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

#include "goxel.h"
#include "file_format.h"

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

static int export(const mesh_t *mesh, const char *path, bool ply)
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
    mesh_iterator_t iter;

    utarray_new(lines_f, &line_icd);
    utarray_new(lines_v, &line_icd);
    utarray_new(lines_vn, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){};
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, bpos)) {
        mat4_set_identity(mat);
        mat4_itranslate(mat, bpos[0], bpos[1], bpos[2]);
        nb_elems = mesh_generate_vertices(mesh, bpos,
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

static int wavefront_export(const image_t *image, const char *path)
{
    const mesh_t *mesh = goxel_get_layers_mesh(image);
    return export(mesh, path, false);
}

int ply_export(const image_t *image, const char *path)
{
    const mesh_t *mesh = goxel_get_layers_mesh(image);
    return export(mesh, path, true);
}

FILE_FORMAT_REGISTER(obj,
    .name = "obj",
    .ext = "obj\0*.obj\0",
    .export_func = wavefront_export,
)

FILE_FORMAT_REGISTER(ply,
    .name = "ply",
    .ext = "ply\0*.ply\0",
    .export_func = ply_export,
)
