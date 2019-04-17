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

#include <cstddef>
#include <cstdio>
#include <iterator>

#define YOCTO_EMBREE 0

#include "../ext_src/yocto/yocto_bvh.h"
#include "../ext_src/yocto/yocto_scene.h"
#include "../ext_src/yocto/yocto_trace.h"

#ifndef __clang__
#pragma GCC diagnostic pop
#endif

extern "C" {
#include "goxel.h"
}

using namespace yocto;

enum {
    CHANGE_MESH         = 1 << 0,
    CHANGE_WORLD        = 1 << 1,
    CHANGE_LIGHT        = 1 << 2,
    CHANGE_CAMERA       = 1 << 3,
    CHANGE_FORCE        = 1 << 4,
    CHANGE_FLOOR        = 1 << 5,
    CHANGE_OPTIONS      = 1 << 6,
};

struct pathtracer_internal {

    // Different hash keys to quickly check for state changes.
    uint64_t mesh_key;
    uint64_t camera_key;
    uint64_t world_key;
    uint64_t light_key;
    uint64_t floor_key;
    uint64_t options_key;

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


/*
 * Get a item from a list by name or create a new one if it doesn't exists
 */
template <class T>
static T* getdefault(vector<T> &list, const string &name)
{
    for (T &value : list) {
        if (value.name == name) return &value;
    }
    list.push_back(T{});
    list.back().name = name;
    return &list.back();
}

/*
 * Return the index of an item in a list.
 */
template <class T>
static int getindex(const vector<T> &list, const T* elem)
{
    return elem - &list.front();
}


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
        for (i = 0; i < nb; i++)
            shape.quads[i] = {i * 4 + 0, i * 4 + 1, i * 4 + 2, i * 4 + 3};
    } else {
        shape.triangles.resize(nb);
        for (i = 0; i < nb; i++)
            shape.triangles[i] = {i * 3 + 0, i * 3 + 1, i * 3 + 2};
    }

end:
    free(vertices);
    return shape;
}

static int sync_mesh(pathtracer_t *pt, int w, int h, bool force)
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
    key = crc64(key, &pt->floor.type, sizeof(pt->floor.type));
    key = crc64(key, (const uint8_t*)&force, sizeof(force));
    if (!force && key == p->mesh_key) return 0;
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
        instance.frame = make_translation_frame(vec3f(
                                block_pos[0], block_pos[1], block_pos[2]));
        p->scene.instances.push_back(instance);
    }

    return CHANGE_MESH;
}

static int sync_floor(pathtracer_t *pt, bool force)
{
    pathtracer_internal_t *p = pt->p;
    uint64_t key = 0;
    yocto_shape *shape;
    yocto_instance *instance;
    int i;
    yocto_material *material;
    vec4f color;
    float pos[3] = {0, 0, 0};

    key = crc64(key, &pt->floor, sizeof(pt->floor));
    if (!force && key == p->floor_key) return 0;
    p->floor_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    if (pt->floor.type == PT_FLOOR_NONE) return CHANGE_FLOOR;

    color[0] = pt->floor.color[0] / 255.f;
    color[1] = pt->floor.color[1] / 255.f;
    color[2] = pt->floor.color[2] / 255.f;
    color[3] = 1.0;

    if (!box_is_null(goxel.image->box)) {
        pos[0] = goxel.image->box[3][0];
        pos[1] = goxel.image->box[3][1];
        pos[2] = goxel.image->box[3][2] - goxel.image->box[2][2];
    }

    material = getdefault(p->scene.materials, "<floor>");
    material->diffuse = vec3f(pt->floor.diffuse);
    material->specular = vec3f(pt->floor.specular);

    shape = getdefault(p->scene.shapes, "<floor>");
    shape->positions = {};
    shape->colors = {};
    shape->normals = {};
    shape->quads = {};
    for (i = 0; i < 4; i++) {
        shape->positions.push_back({i % 2 - 0.5f, i / 2 - 0.5f, 0.f});
        shape->normals.push_back({0, 0, 1});
        shape->colors.push_back(color);
    }
    shape->quads.push_back({0, 1, 3, 2});
    shape->material = getindex(p->scene.materials, material);
    instance = getdefault(p->scene.instances, "<floor>");
    instance->shape = getindex(p->scene.shapes, shape);
    instance->frame = make_translation_frame<float>({pos[0], pos[1], pos[2]}) *
                      make_scaling_frame(
                        vec3f(pt->floor.size[0], pt->floor.size[1], 1.f));
    return CHANGE_FLOOR;
}

static int sync_camera(pathtracer_t *pt, int w, int h,
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
    if (!force && key == p->camera_key) return 0;
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

    return CHANGE_CAMERA;
}

static int sync_world(pathtracer_t *pt, bool force)
{
    uint64_t key = 0;
    pathtracer_internal_t *p = pt->p;
    yocto_texture *texture;
    yocto_environment *environment;
    float turbidity = 3;
    bool has_sun = false;
    vec4f color;

    key = crc64(key, &pt->world.type, sizeof(pt->world.type));
    key = crc64(key, &pt->world.energy, sizeof(pt->world.energy));
    key = crc64(key, &pt->world.color, sizeof(pt->world.color));
    if (!force && key == p->world_key) return 0;
    p->world_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    texture = getdefault(p->scene.textures, "<world>");
    texture->filename = "textures/uniform.hdr";

    color[0] = pt->world.color[0] / 255.f;
    color[1] = pt->world.color[1] / 255.f;
    color[2] = pt->world.color[2] / 255.f;
    color[3] = 1.0;

    switch (pt->world.type) {
    case PT_WORLD_NONE:
        return true;
    case PT_WORLD_SKY:
        texture->hdr_image.resize({512, 256});
        make_sunsky_image(texture->hdr_image, pif / 4, turbidity, has_sun,
                          1.0f, 0, {color.x, color.y, color.z});
        break;
    case PT_WORLD_UNIFORM:
        texture->hdr_image = {{64, 64}, color};
        break;
    }
    environment = getdefault(p->scene.environments, "<world>");
    environment->emission = vec3f{1, 1, 1} * pt->world.energy;
    environment->emission_texture = getindex(p->scene.textures, texture);
    environment->frame = make_rotation_frame(vec3f{1.f, 0.f, 0.f}, pif / 2);

    return CHANGE_WORLD;
}

static int sync_light(pathtracer_t *pt, bool force)
{

    uint64_t key = 0;
    pathtracer_internal_t *p = pt->p;
    yocto_shape *shape;
    yocto_instance *instance;
    yocto_material *material;
    // Large enough to be considered at infinity, but not enough to produce
    // rendering effects.
    const float d = 10000;
    float ke;
    float light_dir[3];

    render_get_light_dir(&goxel.rend, light_dir);
    key = crc64(key, &pt->world, sizeof(pt->world));
    key = crc64(key, &pt->light, sizeof(pt->light));
    key = crc64(key, light_dir, sizeof(light_dir));

    if (!force && key == p->light_key) return 0;
    p->light_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);

    ke = pt->light.energy * 20;
    material = getdefault(p->scene.materials, "<light>");
    material->emission = {ke * d * d, ke * d * d, ke * d * d};

    shape = getdefault(p->scene.shapes, "<light>");
    shape->positions = {};
    shape->triangles = {};
    shape->positions.push_back({0, 0, 0});
    shape->positions.push_back({1, 0, 0});
    shape->positions.push_back({1, 1, 0});
    shape->triangles.push_back({0, 1, 2});
    shape->material = getindex(p->scene.materials, material);

    instance = getdefault(p->scene.instances, "<light>");
    instance->shape = getindex(p->scene.shapes, shape);
    instance->frame = make_translation_frame<float>(
            {light_dir[0] * d, light_dir[1] * d, light_dir[2] * d});

    return CHANGE_LIGHT;
}

static int sync_options(pathtracer_t *pt, bool force)
{
    uint64_t key = 0;
    pathtracer_internal_t *p = pt->p;
    key = crc64(key, &pt->num_samples, sizeof(pt->num_samples));
    if (!force && key == p->options_key) return 0;
    p->options_key = key;
    trace_image_async_stop(p->trace_futures, p->trace_queue, p->trace_options);
    p->trace_options.num_samples = pt->num_samples;
    return CHANGE_OPTIONS;
}

static int sync(pathtracer_t *pt, int w, int h, bool force)
{
    pathtracer_internal_t *p = pt->p;
    int changes = 0;

    if (force) changes |= CHANGE_FORCE;

    changes |= sync_mesh(pt, w, h, changes);
    changes |= sync_floor(pt, changes);
    changes |= sync_world(pt, changes);
    changes |= sync_light(pt, changes);
    changes |= sync_camera(pt, w, h, &goxel.camera, changes);
    changes |= sync_options(pt, changes);

    if (changes) {
        add_missing_materials(p->scene);
        add_missing_names(p->scene);
        update_transforms(p->scene);
    }

    // Update BVH if needed.
    if (changes & (CHANGE_MESH | CHANGE_LIGHT | CHANGE_FLOOR)) {
        p->bvh = {};
        build_scene_bvh(p->scene, p->bvh);
    }
    if (changes & CHANGE_LIGHT) {
        init_trace_lights(p->lights, p->scene);
    }

    if (changes) {
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
    return changes;
}

static void make_preview(pathtracer_t *pt)
{
    int i, j, pi, pj;
    pathtracer_internal_t *p = pt->p;
    trace_image_options preview_options = p->trace_options;
    int preview_ratio = 8;
    image4f preview;
    vec4b v;

    preview_options.image_size /= preview_ratio;
    preview_options.num_samples = 1;
    preview = trace_image(p->scene, p->bvh, p->lights,
                          preview_options);
    tonemap_image(preview, preview, p->exposure, false, true);

    for (i = 0; i < pt->h; i++) {
        for (j = 0; j < pt->w; j++) {
            pi = clamp(i / preview_ratio, 0, preview.size().y - 1);
            pj = clamp(j / preview_ratio, 0, preview.size().x - 1);
            v = float_to_byte(preview[{pj, pi}]);
            memcpy(&pt->buf[(i * pt->w + j) * 4], &v, 4);
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
    int changes, i, j, size = 0;
    image_region region = image_region{};
    vec4b v;

    if (!pt->p) pt->p = new pathtracer_internal_t();
    p = pt->p;
    p->trace_options.image_size = {pt->w, pt->h};
    changes = sync(pt, pt->w, pt->h, pt->force_restart);
    pt->force_restart = false;
    assert(p->display.size()[0] == pt->w);
    assert(p->display.size()[1] == pt->h);
    if (changes) pt->status = PT_RUNNING;

    if (changes & CHANGE_CAMERA) {
        make_preview(pt);
        pt->progress = 0;
        return;
    }

    while (p->trace_queue.try_pop(region)) {
        tonemap_image_region(p->display, region, p->image,
                p->exposure, false, true);
        for (i = region.min[1]; i < region.max[1]; i++)
        for (j = region.min[0]; j < region.max[0]; j++) {
            v = float_to_byte(p->display[{j, i}]);
            memcpy(&pt->buf[(i * pt->w + j) * 4], &v, 4);
        }
        size += region.size().x * region.size().y;
        if (size >= p->image.size().x * p->image.size().y) break;
    }
    pt->progress = (float)p->trace_sample /
                   p->trace_options.num_samples;

    if (pt->status != PT_FINISHED &&
            p->trace_sample == p->trace_options.num_samples) {
        pt->status = PT_FINISHED;
        if (*pt->output) {
            img_write(pt->buf, pt->w, pt->h, 4, pt->output);
        }
    }
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
