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

typedef struct {
    char    type[2];
    union {
        struct {
            vec3_t   v;
            uvec3b_t c;
        };
        vec3_t vn;
        struct {
            int  vs[4];
            int vns[4];
        };
    };
} line_t;

static UT_icd line_icd = {sizeof(line_t), NULL, NULL, NULL};

static int lines_find(UT_array *lines, const line_t *line)
{
    int i, ret = 0;
    line_t *l;
    for (i = 0; i < utarray_len(lines); i++) {
        l = (line_t*)utarray_eltptr(lines, i);
        if (strncmp(l->type, line->type, 2) != 0) continue;
        ret++;
        if (memcmp(l, line, sizeof(*line)) == 0)
            return ret;
    }
    return 0;
}

static int lines_add(UT_array *lines, const line_t *line)
{
    int i;
    i = lines_find(lines, line);
    if (i) return i;
    utarray_push_back(lines, line);
    return lines_find(lines, line);
}

static int lines_count(UT_array *lines, const char *type)
{
    line_t *line = NULL;
    int ret = 0;
    while( (line = (line_t*)utarray_next(lines, line))) {
        if (strncmp(line->type, type, 2) == 0)
            ret++;
    }
    return ret;
}

void wavefront_export(const mesh_t *mesh, const char *path)
{
    // XXX: Merge faces that can be merged into bigger ones.
    //      Allow to chose between quads or triangles.
    //      Also export mlt file for the colors.
    block_t *block;
    voxel_vertex_t* verts;
    vec3_t v;
    uvec3b_t c;
    int nb_quads, i, j, bpos[3];
    mat4_t mat;
    FILE *out;
    const int N = BLOCK_SIZE;
    UT_array *lines;
    line_t line, face, *line_ptr;

    utarray_new(lines, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){"f "};
    MESH_ITER_BLOCKS(mesh, bpos, NULL, NULL, block) {
        mat = mat4_identity;
        mat4_itranslate(&mat, bpos[0], bpos[1], bpos[2]);
        mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

        nb_quads = mesh_generate_vertices(mesh, block, bpos, 0, 0, verts);
        for (i = 0; i < nb_quads; i++) {
            // Put the vertices.
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].pos.x,
                         verts[i * 4 + j].pos.y,
                         verts[i * 4 + j].pos.z);
                v = mat4_mul_vec3(mat, v);
                c = verts[i * 4 + j].color.rgb;
                line = (line_t){"v ", .v = v, .c = c};
                face.vs[j] = lines_add(lines, &line);
            }
            // Put the normals
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].normal.x,
                         verts[i * 4 + j].normal.y,
                         verts[i * 4 + j].normal.z);
                line = (line_t){"vn", .vn = v};
                face.vns[j] = lines_add(lines, &line);
            }
            lines_add(lines, &face);
        }
    }
    out = fopen(path, "w");
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    line_ptr = NULL;
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "v ", 2) == 0)
            fprintf(out, "v %g %g %g %f %f %f\n",
                    VEC3_SPLIT(line_ptr->v),
                    line_ptr->c.r / 255.,
                    line_ptr->c.g / 255.,
                    line_ptr->c.b / 255.);
    }
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "vn", 2) == 0)
            fprintf(out, "vn %g %g %g\n", VEC3_SPLIT(line_ptr->vn));
    }
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "f ", 2) == 0)
            fprintf(out, "f %d//%d %d//%d %d//%d %d//%d\n",
                         line_ptr->vs[0], line_ptr->vns[0],
                         line_ptr->vs[1], line_ptr->vns[1],
                         line_ptr->vs[2], line_ptr->vns[2],
                         line_ptr->vs[3], line_ptr->vns[3]);
    }
    fclose(out);
    utarray_free(lines);
    free(verts);
}

void ply_export(const mesh_t *mesh, const char *path)
{
    block_t *block;
    voxel_vertex_t* verts;
    vec3_t v;
    uvec3b_t c;
    int nb_quads, i, j, bpos[3];
    mat4_t mat;
    FILE *out;
    const int N = BLOCK_SIZE;
    UT_array *lines;
    line_t line, face, *line_ptr;

    utarray_new(lines, &line_icd);
    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    face = (line_t){"f "};
    MESH_ITER_BLOCKS(mesh, bpos, NULL, NULL, block) {
        mat = mat4_identity;
        mat4_itranslate(&mat, bpos[0], bpos[1], bpos[2]);
        mat4_itranslate(&mat, -N / 2 + 0.5, -N / 2 + 0.5, -N / 2 + 0.5);

        nb_quads = mesh_generate_vertices(mesh, block, bpos, 0, 0, verts);
        for (i = 0; i < nb_quads; i++) {
            // Put the vertices.
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].pos.x,
                         verts[i * 4 + j].pos.y,
                         verts[i * 4 + j].pos.z);
                v = mat4_mul_vec3(mat, v);
                c = verts[i * 4 + j].color.rgb;
                line = (line_t){"v ", .v = v, .c = c};
                face.vs[j] = lines_add(lines, &line);
            }
            // Put the normals
            for (j = 0; j < 4; j++) {
                v = vec3(verts[i * 4 + j].normal.x,
                         verts[i * 4 + j].normal.y,
                         verts[i * 4 + j].normal.z);
                line = (line_t){"vn", .vn = v};
                face.vns[j] = lines_add(lines, &line);
            }
            lines_add(lines, &face);
        }
    }
    out = fopen(path, "w");
    fprintf(out, "ply\n");
    fprintf(out, "format ascii 1.0\n");
    fprintf(out, "comment Generated from Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "element vertex %d\n", lines_count(lines, "v "));
    fprintf(out, "property float x\n");
    fprintf(out, "property float y\n");
    fprintf(out, "property float z\n");
    fprintf(out, "property uchar red\n");
    fprintf(out, "property uchar green\n");
    fprintf(out, "property uchar blue\n");
    fprintf(out, "element face %d\n", lines_count(lines, "f "));
    fprintf(out, "property list uchar int vertex_index\n");
    fprintf(out, "end_header\n");
    line_ptr = NULL;
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "v ", 2) == 0)
            fprintf(out, "%g %g %g %d %d %d\n",
                    VEC3_SPLIT(line_ptr->v),
                    VEC3_SPLIT(line_ptr->c));
    }
    while( (line_ptr = (line_t*)utarray_next(lines, line_ptr))) {
        if (strncmp(line_ptr->type, "f ", 2) == 0)
            fprintf(out, "4 %d %d %d %d\n", line_ptr->vs[0] - 1,
                                            line_ptr->vs[1] - 1,
                                            line_ptr->vs[2] - 1,
                                            line_ptr->vs[3] - 1);
    }
    fclose(out);
    utarray_free(lines);
    free(verts);
}

static void export_as_obj(const char *path)
{
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "obj\0*.obj\0", NULL, "untitled.obj");
    if (!path) return;
    wavefront_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_obj,
    .help = "Export the image as a wavefront obj file",
    .cfunc = export_as_obj,
    .csig = "vp",
    .file_format = {
        .name = "obj",
        .ext = "*.obj\0",
    },
)

static void export_as_ply(const char *path)
{
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "ply\0*.ply\0", NULL, "untitled.ply");
    if (!path) return;
    ply_export(goxel->layers_mesh, path);
}

ACTION_REGISTER(export_as_ply,
    .help = "Save the image as a ply file",
    .cfunc = export_as_ply,
    .csig = "vp",
    .file_format = {
        .name = "ply",
        .ext = "*.ply\0",
    },
)
