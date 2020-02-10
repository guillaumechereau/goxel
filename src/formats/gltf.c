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
    json_t *materials;
    json_t *images;
    json_t *textures;

    palette_t palette;
} gltf_t;

typedef struct {
    float   pos[3];
    float   normal[3];

    // XXX: for vertex color we are wasting space here.
    union {
        uint8_t color[4];
        float   texcoord[2];
    };
} gltf_vertex_t;

typedef struct {
    bool vertex_color;
} export_options_t;

static export_options_t g_export_options = {};


// Return the next power of 2 larger or equal to x.
static int next_pow2(int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static void gltf_init(gltf_t *g, const export_options_t *options)
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
    g->materials = json_object_push(g->root, "materials", json_array_new(0));

    if (!options->vertex_color) {
        g->images = json_object_push(g->root, "images", json_array_new(0));
        g->textures = json_object_push(g->root, "textures", json_array_new(0));
    }
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

static void fill_buffer(const gltf_t *g, gltf_vertex_t *bverts,
                        const voxel_vertex_t *verts, int nb, int subdivide,
                        bool vertex_color)
{
    int i, c, s = 0;
    float uv[2];

    // The palette texture size.
    if (!vertex_color)
        s = max(next_pow2(ceil(log2(g->palette.size))), 16);

    for (i = 0; i < nb; i++) {
        bverts[i].pos[0] = (float)verts[i].pos[0] / subdivide;
        bverts[i].pos[1] = (float)verts[i].pos[1] / subdivide;
        bverts[i].pos[2] = (float)verts[i].pos[2] / subdivide;
        bverts[i].normal[0] = verts[i].normal[0];
        bverts[i].normal[1] = verts[i].normal[1];
        bverts[i].normal[2] = verts[i].normal[2];
        vec3_normalize(bverts[i].normal, bverts[i].normal);

        if (vertex_color) {
            bverts[i].color[0] = verts[i].color[0];
            bverts[i].color[1] = verts[i].color[1];
            bverts[i].color[2] = verts[i].color[2];
            bverts[i].color[3] = verts[i].color[3];
        } else {
            c = palette_search(&g->palette, verts[i].color, true);
            assert(c != -1);
            uv[0] = (c % s + 0.5) / s;
            uv[1] = (c / s + 0.5) / s;
            bverts[i].texcoord[0] = uv[0];
            bverts[i].texcoord[1] = uv[1];
        }
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

static int get_material_idx(const image_t *img, const material_t *mat)
{
    int i;
    const material_t *m;
    for (i = 0, m = img->materials; m; m = m->next, i++) {
        if (m == mat) return i;
    }
    return 0;
}

static void save_layer(gltf_t *g, json_t *root_node_children,
                       const image_t *img, const layer_t *layer,
                       const export_options_t *options)
{
    json_t *gmesh, *buffer, *primitives, *primitive, *attributes, *node,
           *buffer_view;
    mesh_iterator_t iter;
    int nb_elems, bpos[3], size = 0, subdivide;
    voxel_vertex_t *verts;
    const int N = BLOCK_SIZE;
    gltf_vertex_t *gverts;
    int buf_size;
    float pos_min[3], pos_max[3];
    mesh_t *mesh = layer->mesh;

    verts = calloc(N * N * N * 6 * 4, sizeof(*verts));
    gverts = calloc(N * N * N * 6 * 4, sizeof(*gverts));

    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, bpos)) {
        nb_elems = mesh_generate_vertices(mesh, bpos,
                                    goxel.rend.settings.effects, verts,
                                    &size, &subdivide);
        if (!nb_elems) continue;
        fill_buffer(g, gverts, verts, nb_elems * size, subdivide,
                    options->vertex_color);
        get_pos_min_max(gverts, nb_elems * size, pos_min, pos_max);
        buf_size = nb_elems * size * sizeof(*gverts);

        buffer = json_array_push(g->buffers, json_object_new(0));
        json_object_push_int(buffer, "byteLength", buf_size);
        json_object_push(buffer, "uri", json_data_new(gverts, buf_size, NULL));
        gmesh = json_array_push(g->meshes, json_object_new(0));
        primitives = json_object_push(gmesh, "primitives", json_array_new(0));
        primitive = json_array_push(primitives, json_object_new(0));
        attributes = json_object_push(primitive, "attributes",
                                      json_object_new(0));
        json_object_push_int(primitive, "material",
                             get_material_idx(img, layer->material));

        if (size == 4)
            make_quad_indices(g, primitive, nb_elems, size);

        buffer_view = json_array_push(g->buffer_views, json_object_new(0));
        json_object_push_int(buffer_view, "buffer", json_index(buffer));
        json_object_push_int(buffer_view, "byteLength", buf_size);
        json_object_push_int(buffer_view, "byteStride", sizeof(gltf_vertex_t));
        json_object_push_int(buffer_view, "target", 34962);

        make_attribute(g, buffer_view, attributes,
                       "POSITION", GLTF_FLOAT, "VEC3", false,
                       nb_elems * size, offsetof(gltf_vertex_t, pos),
                       pos_min, pos_max);
        make_attribute(g, buffer_view, attributes,
                       "NORMAL", GLTF_FLOAT, "VEC3", false,
                       nb_elems * size, offsetof(gltf_vertex_t, normal),
                       NULL, NULL);

        if (options->vertex_color) {
            make_attribute(g, buffer_view, attributes,
                           "COLOR_0", GLTF_UNSIGNED_BYTE, "VEC4", true,
                           nb_elems * size, offsetof(gltf_vertex_t, color),
                           NULL, NULL);
        } else {
            make_attribute(g, buffer_view, attributes,
                           "TEXCOORD_0", GLTF_FLOAT, "VEC2", false,
                           nb_elems * size, offsetof(gltf_vertex_t, texcoord),
                           NULL, NULL);
        }

        node = json_array_push(g->nodes, json_object_new(0));
        json_object_push(node, "translation", json_int_array_new(bpos, 3));
        json_object_push_int(node, "mesh", json_index(gmesh));
        json_array_push(root_node_children, json_integer_new(json_index(node)));
    }
    free(verts);
    free(gverts);
}

static void create_palette_texture(gltf_t *g, const image_t *img)
{
    // Create the global palette with all the colors.
    layer_t *layer;
    mesh_iterator_t iter;
    int i, s, pos[3], size;
    uint8_t c[4];
    uint8_t (*data)[3];
    uint8_t *png;
    json_t *image, *texture;

    DL_FOREACH(img->layers, layer) {
        iter = mesh_get_iterator(layer->mesh, 0);
        while (mesh_iter(&iter, pos)) {
            mesh_get_at(layer->mesh, &iter, pos, c);
            palette_insert(&g->palette, c, NULL);
        }
    }

    s = max(next_pow2(ceil(log2(g->palette.size))), 16);
    data = calloc(s * s, sizeof(*data));
    for (i = 0; i < g->palette.size; i++)
        memcpy(data[i], g->palette.entries[i].color, 3);
    png = img_write_to_mem((void*)data, s, s, 3, &size);
    free(data);
    image = json_array_push(g->images, json_object_new(0));
    json_object_push(image, "uri", json_data_new(png, size, "image/png"));
    free(png);

    texture = json_array_push(g->textures, json_object_new(0));
    json_object_push_int(texture, "source", 0);
}

static void gltf_export(const image_t *img, const char *path,
                        const export_options_t *options)
{
    char *json_buf;
    gltf_t g = {};
    FILE *file;
    json_serialize_opts opts = {.indent_size = 4};
    const layer_t *layer;
    json_t *root_node, *root_node_children, *scene, *scene_nodes, *material,
           *pbr, *tex;
    material_t *mat;

    gltf_init(&g, options);

    if (!options->vertex_color)
        create_palette_texture(&g, img);

    DL_FOREACH(img->materials, mat) {
        material = json_array_push(g.materials, json_object_new(0));
        json_object_push_string(material, "name", mat->name);
        json_object_push(material, "emissiveFactor",
                         json_float_array_new(mat->emission, 3));
        pbr = json_object_push(material, "pbrMetallicRoughness",
                               json_object_new(0));
        json_object_push(pbr, "baseColorFactor",
                         json_float_array_new(mat->base_color, 4));
        json_object_push_float(pbr, "metallicFactor", mat->metallic);
        json_object_push_float(pbr, "roughnessFactor", mat->roughness);

        if (!options->vertex_color) {
            tex = json_object_push(pbr, "baseColorTexture", json_object_new(0));
            json_object_push_int(tex, "index", 0);
        }
    }

    root_node = json_array_push(g.nodes, json_object_new(0));
    root_node_children = json_object_push(root_node, "children",
                                          json_array_new(0));
    json_object_push(root_node, "matrix", json_int_array_new((int[]) {
        1, 0,  0, 0,
        0, 0, -1, 0,
        0, 1,  0, 0,
        0, 0,  0, 1
    }, 16));
    scene = json_array_push(g.scenes, json_object_new(0));
    scene_nodes = json_object_push(scene, "nodes", json_array_new(0));
    json_array_push(scene_nodes, json_integer_new(json_index(root_node)));

    DL_FOREACH(img->layers, layer) {
        save_layer(&g, root_node_children, img, layer, options);
    }
    json_buf = malloc(json_measure_ex(g.root, opts));
    json_serialize_ex(json_buf, g.root, opts);
    file = fopen(path, "w");
    fwrite(json_buf, 1, strlen(json_buf), file);
    free(json_buf);
    fclose(file);
    json_builder_free(g.root);
    free(g.palette.entries);
}

static void export_as_gltf(const char *path)
{
    path = path ?: sys_get_save_path("gltf\0*.gltf\0", "untitled.gltf");
    if (!path) return;
    gltf_export(goxel.image, path, &g_export_options);
    sys_on_saved(path);
}

static void export_gui(void)
{
    gui_checkbox("Vertex color", &g_export_options.vertex_color,
                 "Save colors as a vertex attribute");
}

ACTION_REGISTER(export_as_gltf,
    .help = "Save the image as a gltf file",
    .cfunc = export_as_gltf,
    .csig = "vp",
    .file_format = {
        .name = "gltf",
        .ext = "*.gltf\0",
        .export_gui = export_gui,
    },
)
