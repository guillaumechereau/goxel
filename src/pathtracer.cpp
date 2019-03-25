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

#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "../ext_src/yocto/yocto_bvh.h"
#include "../ext_src/yocto/yocto_scene.h"
#include "../ext_src/yocto/yocto_trace.h"

// For debug.
#include "../ext_src/yocto/yocto_sceneio.h"

#ifndef __clang__
#pragma GCC diagnostic pop
#endif

extern "C" {
#include "goxel.h"
}

using namespace yocto;

struct pathtracer_internal {

    // Different hash keys to quickly check for state changes.
    uint64_t mesh_key;
    uint64_t camera_key;
    uint64_t world_key;
    uint64_t light_key;

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
    float exposure;
};

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

static bool sync_mesh(pathtracer_t *pt, int w, int h, bool force)
{
    uint64_t key;
    mesh_iterator_t iter;
    mesh_t *mesh = goxel.render_mesh;
    int block_pos[3];
    yocto_shape shape;
    yocto_instance instance;
    pathtracer_internal_t *p = pt->p;

    key = mesh_get_key(goxel.render_mesh);
    key = crc64(key, goxel.back_color, sizeof(goxel.back_color));
    key = crc64(key, (const uint8_t*)&w, sizeof(w));
    key = crc64(key, (const uint8_t*)&h, sizeof(h));
    key = crc64(key, (const uint8_t*)&goxel.rend.settings.effects,
                     sizeof(goxel.rend.settings.effects));
    key = crc64(key, (const uint8_t*)&force, sizeof(force));
    if (!force && key == p->mesh_key) return false;
    p->mesh_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    p->scene = {};
    p->lights = {};

    // The mesh has changed, regenerates the scene.
    iter = mesh_get_iterator(mesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        shape = create_shape_for_block(mesh, block_pos);
        if (shape.positions.empty()) continue;
        p->scene.shapes.push_back(shape);
        instance.name = shape.name;
        instance.shape = p->scene.shapes.size() - 1;
        instance.frame = make_translation_frame<float>({
                block_pos[0], block_pos[1], block_pos[2]});
        p->scene.instances.push_back(instance);
    }

    return true;
}

static bool sync_camera(pathtracer_t *pt, int w, int h,
                        const camera_t *camera, bool force)
{
    pathtracer_internal_t *p = pt->p;
    yocto_camera *cam;
    uint64_t key;

    add_missing_cameras(p->scene);
    cam = &p->scene.cameras[0];

    key = crc64(0, (uint8_t*)camera->view_mat, sizeof(camera->view_mat));
    key = crc64(key, (uint8_t*)camera->proj_mat, sizeof(camera->proj_mat));
    key = crc64(key, (uint8_t*)&w, sizeof(w));
    key = crc64(key, (uint8_t*)&h, sizeof(h));
    if (!force && key == p->camera_key) return false;
    p->camera_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

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

static bool sync_world(pathtracer_t *pt, bool force)
{
    uint64_t key;
    pathtracer_internal_t *p = pt->p;
    yocto_scene &scene = p->scene;
    auto texture = yocto_texture{};
    auto environment = yocto_environment{};
    float turbidity = 3;
    bool has_sun = false;

    key = crc64(0, &pt->world, sizeof(pt->world));
    if (!force && key == p->world_key) return false;
    p->world_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    scene.environments = {};
    scene.textures = {};
    texture.name = "<world>";
    texture.filename = "textures/uniform.hdr";

    switch (pt->world) {
    case PT_WORLD_NONE:
        return true;
    case PT_WORLD_SKY:
        texture.hdr_image.resize({1024, 512});
        make_sunsky_image(texture.hdr_image, pif / 4, turbidity, has_sun);
        break;
    case PT_WORLD_UNIFORM:
        texture.hdr_image = {{64, 64}, {0.5, 0.5, 0.5, 1.0}};
        break;
    }
    scene.textures.push_back(texture);
    environment.name = "<world>";
    environment.emission = {1, 1, 1};
    environment.emission_texture = (int)scene.textures.size() - 1;
    environment.frame = make_rotation_frame(vec3f{1.f, 0.f, 0.f}, pif / 2);
    scene.environments.push_back(environment);

    return true;
}

static bool sync_light(pathtracer_t *pt, bool force)
{

    uint64_t key;
    pathtracer_internal_t *p = pt->p;
    yocto_shape shape;
    yocto_instance instance;
    yocto_material material;
    float d = 100;
    float ke = 20;

    key = crc64(0, &pt->world, sizeof(pt->world));
    if (!force && key == p->light_key) return false;
    p->light_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    material.name = "light";
    material.emission = {ke * d * d, ke * d * d, ke * d * d};
    p->scene.materials.push_back(material);

    shape.name = "light";
    shape.positions.push_back({0, 0, 0});
    shape.positions.push_back({1, 0, 0});
    shape.positions.push_back({1, 1, 0});

    shape.triangles.push_back({0, 1, 2});
    shape.material = p->scene.materials.size() - 1;

    p->scene.shapes.push_back(shape);
    instance.shape = p->scene.shapes.size() - 1;
    instance.name = shape.name;
    instance.frame = make_translation_frame<float>({50, 20, 100});
    p->scene.instances.push_back(instance);

    return true;
}

static bool sync(pathtracer_t *pt, int w, int h, bool force)
{
    pathtracer_internal_t *p = pt->p;

    if (sync_mesh(pt, w, h, force)) force = true;
    if (sync_world(pt, force)) force = true;
    if (sync_light(pt, force)) force = true;
    if (sync_camera(pt, w, h, &goxel.camera, force)) force = true;

    if (force) {
        // tesselate_shapes(p->scene); ?
        add_missing_materials(p->scene);
        add_missing_names(p->scene);
        update_transforms(p->scene);
        // Update BVH.
        p->bvh = {};
        build_scene_bvh(p->scene, p->bvh);
        init_trace_lights(p->lights, p->scene);
    }

    if (force) {
        if (p->lights.instances.empty() &&
                p->lights.environments.empty()) {
            p->trace_options.sampler_type = trace_sampler_type::eyelight;
        }
        p->trace_options.image_size = {w, h};

        p->image = image4f({w, h});
        p->display = image4f({w, h});

        trace_image_async_stop(p->trace_futures, p->trace_queue,
                               p->trace_options);

        init_trace_state(p->state, {w, h});

        p->trace_options.cancel_flag = &p->trace_stop;
        // p->trace_options.run_serially = true;
        p->trace_sample = 0;
        p->trace_stop = false;
        trace_image_async_start(p->image, p->state, p->scene,
                                p->bvh, p->lights,
                                p->trace_futures, p->trace_sample,
                                p->trace_queue, p->trace_options);
    }
    return force;
}

static void make_preview(pathtracer_t *pt)
{
    int i, j, pi, pj;
    pathtracer_internal_t *p = pt->p;
    trace_image_options preview_options = p->trace_options;
    int preview_ratio = 8;
    image4f preview;

    preview_options.image_size /= preview_ratio;
    preview_options.num_samples = 1;
    preview = trace_image(p->scene, p->bvh, p->lights,
                          preview_options);
    tonemap_image(preview, preview, p->exposure, false, true);

    for (i = 0; i < pt->w; i++) {
        for (j = 0; j < pt->h; j++) {
            pi = clamp(i / preview_ratio, 0, preview.size().y - 1);
            pj = clamp(j / preview_ratio, 0, preview.size().x - 1);
            memcpy(&pt->buf[(i * pt->w + j) * 4],
                   preview.data() + (pi * preview.size().x + pj),
                   4 * sizeof(float));
        }
    }
}

/*
 * Function: pathtracer_iter
 * Iter the rendering process of the current mesh.
 *
 * Parameters:
 *   pt     - A pathtracer instance.
 */
void pathtracer_iter(pathtracer_t *pt)
{
    pathtracer_internal_t *p;
    bool changed;
    int i, j, size = 0;
    image_region region = image_region{};

    if (!pt->p) pt->p = new pathtracer_internal_t();
    p = pt->p;
    p->trace_options.image_size = {pt->w, pt->h};
    changed = sync(pt, pt->w, pt->h, pt->force_restart);
    pt->force_restart = false;
    assert(p->display.size()[0] == pt->w);
    assert(p->display.size()[1] == pt->h);

    if (changed) {
        make_preview(pt);
        pt->progress = 0;
        return;
    }

    while (p->trace_queue.try_pop(region)) {
        tonemap_image_region(p->display, region, p->image,
                p->exposure, false, true);
        for (i = region.min[1]; i < region.max[1]; i++)
        for (j = region.min[0]; j < region.max[0]; j++) {
            memcpy(&pt->buf[(i * pt->w + j) * 4],
                   p->display.data() + (i * pt->w + j),
                   4 * sizeof(float));
        }
        size += region.size().x * region.size().y;
        if (size >= p->image.size().x * p->image.size().y) break;
    }
    pt->progress = (float)p->trace_sample /
                   p->trace_options.num_samples;
}


/*
 * Stop the pathtracer thread if it is running.
 */
void pathtracer_stop(pathtracer_t *pt)
{
    pathtracer_internal_t *p = pt->p;
    if (!p) return;
    trace_image_async_stop(
        p->trace_futures, p->trace_queue, p->trace_options);
    delete p;
    pt->p = nullptr;
}
