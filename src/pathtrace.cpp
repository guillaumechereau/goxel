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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "../ext_src/yocto/yocto_bvh.h"
#include "../ext_src/yocto/yocto_scene.h"
#include "../ext_src/yocto/yocto_trace.h"

// For debug.
#include "../ext_src/yocto/yocto_sceneio.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

extern "C" {
#include "goxel.h"
}

using namespace yocto;

static struct {
    uint64_t key;
    uint64_t camera_key;
    yocto_scene scene;
    bvh_scene bvh;
    image4f image;
    image4f display;
    trace_state state;
    trace_lights lights;
    trace_image_options trace_options;
    atomic<int> trace_sample;
    vector<future<void>> trace_futures;
    concurrent_queue<image_region> trace_queue;
    atomic<bool> trace_stop;
} g_state = {};

static yocto_shape create_shape_for_block(
        const mesh_t *mesh, const int block_pos[3])
{
    voxel_vertex_t* vertices;
    int i, nb, size, subdivide;
    yocto_shape shape = {};

    vertices = (voxel_vertex_t*)calloc(
                BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE * 6 * 4,
                sizeof(*vertices));
    nb = mesh_generate_vertices(mesh, block_pos,
                                goxel.rend.settings.effects, vertices,
                                &size, &subdivide);
    if (!nb) goto end;

    // Set vertices data.
    shape.positions.resize(nb * size);
    shape.colors.resize(nb * size);
    shape.normals.resize(nb * size);
    for (i = 0; i < nb * size; i++) {
        shape.positions[i] = {vertices[i].pos[0] / (float)subdivide,
                              vertices[i].pos[1] / (float)subdivide,
                              vertices[i].pos[2] / (float)subdivide};
        shape.colors[i] = {vertices[i].color[0] / 255.f,
                           vertices[i].color[1] / 255.f,
                           vertices[i].color[2] / 255.f,
                           1.0f};
        shape.normals[i] = {vertices[i].normal[0] / 128.f,
                            vertices[i].normal[1] / 128.f,
                            vertices[i].normal[2] / 128.f};
    }

    // Set primitives (quads or triangles) data.
    if (size == 4) {
        shape.quads.resize(nb);
        for (i = 0; i < nb; i++) {
            shape.quads[i] = {i * 4 + 0, i * 4 + 1, i * 4 + 2, i * 4 + 3};
        }
    } else {
        // TODO
        assert(false);
    }

end:
    free(vertices);
    return shape;
}

static bool sync_mesh(int w, int h, bool force)
{
    uint64_t key;
    mesh_iterator_t iter;
    mesh_t *mesh = goxel.render_mesh;
    int block_pos[3];
    yocto_shape shape;
    yocto_instance instance;

    key = mesh_get_key(goxel.render_mesh);
    key = crc64(key, goxel.back_color, sizeof(goxel.back_color));
    key = crc64(key, (const uint8_t*)&w, sizeof(w));
    key = crc64(key, (const uint8_t*)&h, sizeof(h));
    key = crc64(key, (const uint8_t*)&goxel.rend.settings.effects,
                     sizeof(goxel.rend.settings.effects));
    key = crc64(key, (const uint8_t*)&force, sizeof(force));
    if (key == g_state.key) return false;

    trace_image_async_stop(g_state.trace_futures, g_state.trace_queue,
                           g_state.trace_options);

    g_state.key = key;
    g_state.scene = {};
    g_state.lights = {};

    // The mesh has changed, regenerates the scene.
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        shape = create_shape_for_block(mesh, block_pos);
        if (shape.positions.empty()) continue;
        g_state.scene.shapes.push_back(shape);
        instance.name = shape.name;
        instance.shape = g_state.scene.shapes.size() - 1;
        instance.frame = make_translation_frame<float>({
                block_pos[0], block_pos[1], block_pos[2]});
        g_state.scene.instances.push_back(instance);
    }

    return true;
}

static bool sync_camera(int w, int h, const camera_t *camera, bool force)
{
    yocto_camera *cam;
    uint64_t key;

    add_missing_cameras(g_state.scene);
    cam = &g_state.scene.cameras[0];

    key = crc64(0, (uint8_t*)camera->view_mat, sizeof(camera->view_mat));
    key = crc64(key, (uint8_t*)camera->proj_mat, sizeof(camera->proj_mat));
    key = crc64(key, (uint8_t*)&w, sizeof(w));
    key = crc64(key, (uint8_t*)&h, sizeof(h));
    if (!force && key == g_state.camera_key) return false;
    g_state.camera_key = key;

    cam->frame =
        make_translation_frame<float>({-goxel.camera.ofs[0],
                                       -goxel.camera.ofs[1],
                                       -goxel.camera.ofs[2]}) *
        make_rotation_frame(quat_inverse(vec<float, 4>{
                goxel.camera.rot[1], goxel.camera.rot[2],
                goxel.camera.rot[3], goxel.camera.rot[0]})) *
        make_translation_frame<float>({0, 0, goxel.camera.dist});
    set_camera_perspective(*cam, goxel.camera.fovy / 180 * M_PI, (float)w / h,
                           goxel.camera.dist);

    return true;
}

static void sync_light(void)
{
    yocto_shape shape;
    yocto_instance instance;
    yocto_material material;
    float d = 100;
    float ke = 20;

    material.name = "light";
    material.emission = {ke * d * d, ke * d * d, ke * d * d};
    g_state.scene.materials.push_back(material);

    shape.name = "light";
    shape.positions.push_back({0, 0, 0});
    shape.positions.push_back({1, 0, 0});
    shape.positions.push_back({1, 1, 0});

    shape.triangles.push_back({0, 1, 2});
    shape.material = g_state.scene.materials.size() - 1;

    g_state.scene.shapes.push_back(shape);
    instance.shape = g_state.scene.shapes.size() - 1;
    instance.name = shape.name;
    instance.frame = make_translation_frame<float>({50, 20, 100});
    g_state.scene.instances.push_back(instance);


    auto &scene = g_state.scene;
    auto texture     = yocto_texture{};
    texture.name     = "<sky>";
    texture.filename = "textures/sky.hdr";
    texture.hdr_image.resize({1024, 512});
    float turbidity = 3;
    bool has_sun = false;
    make_sunsky_image(texture.hdr_image, pif / 4, turbidity, has_sun);
    scene.textures.push_back(texture);
    auto environment             = yocto_environment{};
    environment.name             = "<sky>";
    environment.emission         = {1, 1, 1};
    environment.emission_texture = (int)scene.textures.size() - 1;
    environment.frame = make_rotation_frame(vec3f{1.f, 0.f, 0.f}, pif / 2);
    scene.environments.push_back(environment);
}

static void sync(int w, int h, bool force)
{
    bool mesh_changed, cam_changed;
    mesh_changed = sync_mesh(w, h, force);
    cam_changed = sync_camera(w, h, &goxel.camera, force || mesh_changed);

    if (mesh_changed) {
        sync_light();
        // tesselate_shapes(g_state.scene); ?

        add_missing_materials(g_state.scene);
        add_missing_names(g_state.scene);
        update_transforms(g_state.scene);

        // Update BVH.
        g_state.bvh = {};
        build_scene_bvh(g_state.scene, g_state.bvh);

        init_trace_lights(g_state.lights, g_state.scene);
    }

    if (mesh_changed || cam_changed) {
        if (g_state.lights.instances.empty() &&
                g_state.lights.environments.empty()) {
            g_state.trace_options.sampler_type = trace_sampler_type::eyelight;
        }
        g_state.trace_options.image_size = {w, h};

        g_state.image = image4f({w, h});
        g_state.display = image4f({w, h});

        trace_image_async_stop(g_state.trace_futures, g_state.trace_queue,
                               g_state.trace_options);

        init_trace_state(g_state.state, {w, h});

        g_state.trace_options.cancel_flag = &g_state.trace_stop;
        // g_state.trace_options.run_serially = true;
        g_state.trace_sample = 0;
        g_state.trace_stop = false;
        trace_image_async_start(g_state.image, g_state.state, g_state.scene,
                                g_state.bvh, g_state.lights,
                                g_state.trace_futures, g_state.trace_sample,
                                g_state.trace_queue, g_state.trace_options);
    }
}

/*
 * Function: pathtrace_iter
 * Iter the rendering process of the current mesh.
 *
 * Parameters:
 *   buf            - A RGBA image buffer.
 *   w              - Width of the image buffer.
 *   h              - Height of the image buffer.
 *   progress       - Rendering progress.
 *   force_restart  - Restart the rendering even if the image of view did
 *                    not change.
 */
void pathtrace_iter(float *buf, int w, int h, float *progress,
                    bool force_restart)
{
    g_state.trace_options.image_size = {w, h};
    sync(w, h, force_restart);
    assert(g_state.display.size()[0] == w);
    assert(g_state.display.size()[1] == h);

    auto region = image_region{};
    int size = 0;
    int i, j;
    float exposure = 1.0;

    while (g_state.trace_queue.try_pop(region)) {
        tonemap_image_region(g_state.display, region, g_state.image,
                exposure, false, true);
        for (i = region.min[1]; i < region.max[1]; i++)
        for (j = region.min[0]; j < region.max[0]; j++) {
            memcpy(&buf[(i * w + j) * 4],
                   g_state.display.data() + (i * w + j),
                   4 * sizeof(float));
        }
        size += region.size().x * region.size().y;
        if (size >= g_state.image.size().x * g_state.image.size().y) break;
    }
    *progress = (float)g_state.trace_sample / g_state.trace_options.num_samples;
}
