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


#include "goxel.h"
#include "utils/json.h"

enum {
    GLTF_BYTE = 5120,
    GLTF_UNSIGNED_BYTE = 5121,
    GLTF_SHORT = 5122,
    GLTF_UNSIGNED_SHORT = 5123,
    GLTF_UNSIGNED_INT = 5125,
    GLTF_FLOAT = 5126,
};

typedef json_value json_t;

typedef struct {
    json_t *root;
    json_t *asset;
    json_t *buffers;
    json_t *buffer_views;
    json_t *meshes;
    json_t *accessors;
    json_t *nodes;
    json_t *scenes;
} gltf_t;

typedef struct {
    float   pos[3];
    float   normal[3];
    uint8_t color[4];
} gltf_vertex_t;

static void gltf_init(gltf_t *g)
{
    g->root = json_object_new(0);
    g->asset = json_object_push(g->root, "asset", json_object_new(0));
    json_object_push(g->asset, "version", json_string_new("2.0"));
    json_object_push(g->asset, "generator", json_string_new("goxel"));

    g->buffers = json_object_push(g->root, "buffers", json_array_new(0));
    g->buffer_views = json_object_push(g->root, "bufferViews",
                                       json_array_new(0));
    g->meshes = json_object_push(g->root, "meshes", json_array_new(0));
    g->accessors = json_object_push(g->root, "accessors", json_array_new(0));
    g->nodes = json_object_push(g->root, "nodes", json_array_new(0));
    g->scenes = json_object_push(g->root, "scenes", json_array_new(0));
}

// Create a buffer view and attribute.
static void make_attribute(gltf_t *g, json_t *buffer_view, json_t *attributes,
                           const char *name,
                           int component_type, const char *type,
                           bool normalized, int nb, int ofs,
                           const float v_min[3], const float v_max[3])
{
    json_t *accessor;

    accessor = json_array_push(g->accessors, json_object_new(0));
    json_object_push_int(accessor, "bufferView", json_index(buffer_view));
    json_object_push_int(accessor, "componentType", component_type);
    json_object_push_int(accessor, "byteOffset", ofs);
    json_object_push_string(accessor, "type", type);
    json_object_push_int(accessor, "count", nb);
    if (normalized) json_object_push_bool(accessor, "normalized", true);

    if (v_min)
        json_object_push(accessor, "min", json_float_array_new(v_min, 3));
    if (v_max)
        json_object_push(accessor, "max", json_float_array_new(v_max, 3));

    json_object_push_int(attributes, name, json_index(accessor));
}

static void make_quad_indices(gltf_t *g, json_t *primitive, int nb, int size)
{
    json_t *buffer, *buffer_view, *accessor;
    uint16_t *data;
    int i;

    data = calloc(nb * 6, sizeof(*data));
    for (i = 0; i < nb * 6; i++)
        data[i] = (i / 6) * 4 + ((int[]){0, 1, 2, 2, 3, 0})[i % 6];
    buffer = json_array_push(g->buffers, json_object_new(0));
    json_object_push_int(buffer, "byteLength", nb * 6 * sizeof(*data));
    json_object_push(buffer, "uri",
            json_data_new(data, nb * 6 * sizeof(*data), NULL));
    free(data);
    buffer_view = json_array_push(g->buffer_views, json_object_new(0));
    json_object_push_int(buffer_view, "buffer", json_index(buffer));
    json_object_push_int(buffer_view, "byteLength", nb * 6 * sizeof(*data));
    json_object_push_int(buffer_view, "target", 34963);

    accessor = json_array_push(g->accessors, json_object_new(0));
    json_object_push_int(accessor, "bufferView", json_index(buffer_view));
    json_object_push_int(accessor, "componentType", GLTF_UNSIGNED_SHORT);
    json_object_push_int(accessor, "count", nb * 6);
    json_object_push_string(accessor, "type", "SCALAR");

    json_object_push_int(primitive, "indices", json_index(accessor));
}

static void fill_buffer(gltf_vertex_t *bverts, const voxel_vertex_t *verts,
                        int nb, int subdivide)
{
    int i;
    for (i = 0; i < nb; i++) {
        bverts[i].pos[0] = (float)verts[i].pos[0] / subdivide;
        bverts[i].pos[1] = (float)verts[i].pos[1] / subdivide;
        bverts[i].pos[2] = (float)verts[i].pos[2] / subdivide;
        bverts[i].normal[0] = verts[i].normal[0];
        bverts[i].normal[1] = verts[i].normal[1];
        bverts[i].normal[2] = verts[i].normal[2];
        vec3_normalize(bverts[i].normal, bverts[i].normal);
        bverts[i].color[0] = verts[i].color[0];
        bverts[i].color[1] = verts[i].color[1];
        bverts[i].color[2] = verts[i].color[2];
        bverts[i].color[3] = verts[i].color[3];
    }
}

static void get_pos_min_max(gltf_vertex_t *bverts, int nb,
                            float pos_min[3], float pos_max[3])
{
    int i;
    pos_min[0] = +FLT_MAX;
    pos_min[1] = +FLT_MAX;
    pos_min[2] = +FLT_MAX;
    pos_max[0] = -FLT_MAX;
    pos_max[1] = -FLT_MAX;
    pos_max[2] = -FLT_MAX;
    for (i = 0; i < nb; i++) {
        pos_min[0] = min(bverts[i].pos[0], pos_min[0]);
        pos_min[1] = min(bverts[i].pos[1], pos_min[1]);
        pos_min[2] = min(bverts[i].pos[2], pos_min[2]);
        pos_max[0] = max(bverts[i].pos[0], pos_max[0]);
        pos_max[1] = max(bverts[i].pos[1], pos_max[1]);
        pos_max[2] = max(bverts[i].pos[2], pos_max[2]);
    }
}

static void gltf_export(const mesh_t *mesh, const char *path)
{
    json_t *gmesh, *buffer, *primitives, *primitive, *attributes, *node,
           *scene, *scene_nodes, *root_node, *root_node_children, *buffer_view;
    char *json_buf;
    FILE *file;
    mesh_iterator_t iter;
    int nb_elems, bpos[3], size = 0, subdivide;
    voxel_vertex_t *verts;
    const int N = BLOCK_SIZE;
    gltf_t g;
    json_serialize_opts opts = {.indent_size = 4};
    gltf_vertex_t *gverts;
    int buf_size;
    float pos_min[3], pos_max[3];

    gltf_init(&g);

    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    gverts = calloc(N * N * N * 6 * 4, sizeof(*gverts));
    root_node = json_array_push(g.nodes, json_object_new(0));
    json_object_push(root_node, "matrix", json_int_array_new((int[]) {
        1, 0,  0, 0,
        0, 0, -1, 0,
        0, 1,  0, 0,
        0, 0,  0, 1
    }, 16));
    root_node_children = json_object_push(root_node, "children",
                                          json_array_new(0));
    scene = json_array_push(g.scenes, json_object_new(0));
    scene_nodes = json_object_push(scene, "nodes", json_array_new(0));
    json_array_push(scene_nodes, json_integer_new(json_index(root_node)));

    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, bpos)) {
        nb_elems = mesh_generate_vertices(mesh, bpos,
                                    goxel.rend.settings.effects, verts,
                                    &size, &subdivide);
        if (!nb_elems) continue;
        fill_buffer(gverts, verts, nb_elems * size, subdivide);
        get_pos_min_max(gverts, nb_elems * size, pos_min, pos_max);
        buf_size = nb_elems * size * sizeof(*gverts);

        buffer = json_array_push(g.buffers, json_object_new(0));
        json_object_push_int(buffer, "byteLength", buf_size);
        json_object_push(buffer, "uri", json_data_new(gverts, buf_size, NULL));
        gmesh = json_array_push(g.meshes, json_object_new(0));
        primitives = json_object_push(gmesh, "primitives", json_array_new(0));
        primitive = json_array_push(primitives, json_object_new(0));
        attributes = json_object_push(primitive, "attributes",
                                      json_object_new(0));

        if (size == 4)
            make_quad_indices(&g, primitive, nb_elems, size);

        buffer_view = json_array_push(g.buffer_views, json_object_new(0));
        json_object_push_int(buffer_view, "buffer", json_index(buffer));
        json_object_push_int(buffer_view, "byteLength", buf_size);
        json_object_push_int(buffer_view, "byteStride", sizeof(gltf_vertex_t));
        json_object_push_int(buffer_view, "target", 34962);

        make_attribute(&g, buffer_view, attributes,
                       "POSITION", GLTF_FLOAT, "VEC3", false,
                       nb_elems * size, offsetof(gltf_vertex_t, pos),
                       pos_min, pos_max);
        make_attribute(&g, buffer_view, attributes,
                       "COLOR_0", GLTF_UNSIGNED_BYTE, "VEC4", true,
                       nb_elems * size, offsetof(gltf_vertex_t, color),
                       NULL, NULL);
        make_attribute(&g, buffer_view, attributes,
                       "NORMAL", GLTF_FLOAT, "VEC3", false,
                       nb_elems * size, offsetof(gltf_vertex_t, normal),
                       NULL, NULL);

        node = json_array_push(g.nodes, json_object_new(0));
        json_object_push(node, "translation", json_int_array_new(bpos, 3));
        json_object_push_int(node, "mesh", json_index(gmesh));
        json_array_push(root_node_children, json_integer_new(json_index(node)));
    }
    free(verts);
    free(gverts);

    json_buf = malloc(json_measure_ex(g.root, opts));
    json_serialize_ex(json_buf, g.root, opts);
    file = fopen(path, "w");
    fwrite(json_buf, 1, strlen(json_buf), file);
    free(json_buf);
    fclose(file);
    json_value_free(g.root);
}

static void export_as_gltf(const char *path)
{
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "gltf\0*.gltf\0", NULL, "untitled.gltf");
    if (!path) return;
    gltf_export(goxel_get_layers_mesh(), path);
}

ACTION_REGISTER(export_as_gltf,
    .help = "Save the image as a gltf file",
    .cfunc = export_as_gltf,
    .csig = "vp",
    .file_format = {
        .name = "gltf",
        .ext = "*.gltf\0",
    },
)
