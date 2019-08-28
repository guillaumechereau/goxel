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

#ifndef YOCTO
#   define YOCTO 1
#endif

#if YOCTO

#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#define YOCTO_EMBREE 0
#define STB_IMAGE_STATIC

#include "../ext_src/yocto/yocto_bvh.h"
#include "../ext_src/yocto/yocto_scene.h"
#include "../ext_src/yocto/yocto_trace.h"

#ifndef __clang__
#pragma GCC diagnostic pop
#endif

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <future>
#include <deque>

extern "C" {
#include "goxel.h"
}

#include "xxhash.h"

using namespace yocto;
using namespace std;
typedef yocto::image<vec4f> image4f;

enum {
    CHANGE_MESH         = 1 << 0,
    CHANGE_WORLD        = 1 << 1,
    CHANGE_LIGHT        = 1 << 2,
    CHANGE_CAMERA       = 1 << 3,
    CHANGE_FORCE        = 1 << 4,
    CHANGE_FLOOR        = 1 << 5,
    CHANGE_OPTIONS      = 1 << 6,
    CHANGE_MATERIAL     = 1 << 7,
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
    trace_params trace_prms;
    bvh_params bvh_prms;
    tonemap_params tonemap_prms;
    atomic<int> trace_sample;
    vector<future<void>> trace_futures;
    deque<image_region> trace_queue;
    std::mutex trace_queuem;
    atomic<bool> trace_stop;
    float exposure;
};


template <typename T>
inline bool try_pop(std::deque<T> &queue, std::mutex &mut, T& value) {
    lock_guard<mutex> lock(mut);
    if (queue.empty()) return false;
    value = queue.front();
    queue.pop_front();
    return true;
}


/*
 * Get a item from a list by name or create a new one if it doesn't exists
 */
template <class T>
static T* getdefault(vector<T> &list, const string &uri,
                     bool *created=nullptr)
{
    if (created) *created = false;
    for (T &value : list) {
        if (value.uri == uri) return &value;
    }
    if (created) *created = true;
    list.push_back(T{});
    list.back().uri = uri;
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

static int get_material_id(pathtracer_t *pt, const material_t *mat,
                           int *changed)
{
    char name[128];
    pathtracer_internal_t *p = pt->p;
    yocto_material *m;
    bool created;
    const material_t default_mat = MATERIAL_DEFAULT;

    if (!mat) mat = &default_mat;
    snprintf(name, sizeof(name), "<mat_%x>", material_get_hash(mat));
    m = getdefault(p->scene.materials, name, &created);
    if (created) {
        *changed |= CHANGE_MATERIAL;
        m->metallic = mat->metallic;
        m->diffuse = {mat->base_color[0],
                      mat->base_color[1],
                      mat->base_color[2]};
        m->opacity = mat->base_color[3];
        m->specular = {0.04, 0.04, 0.04};
        m->roughness = mat->roughness;
        m->emission = {mat->emission[0], mat->emission[1], mat->emission[2]};
    }
    return getindex(p->scene.materials, m);
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
                                goxel.rend.settings.effects,
                                vertices, &size, &subdivide);
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

// Stop the asynchronous renderer.
void stop_render(vector<future<void>>& futures,
    deque<image_region>& queue,
    std::mutex& queuem,
    atomic<bool>* cancel)
{
    if (cancel) *cancel = true;
    for (auto& f : futures) f.get();
    futures.clear();
    {
        std::lock_guard<mutex> guard{queuem};
        queue.clear();
    }
}


static int sync_mesh(pathtracer_t *pt, int w, int h, bool force)
{
    uint32_t key = 0, k;
    mesh_iterator_t iter;
    const mesh_t *mesh;
    int block_pos[3], i, changed = 0;
    yocto_shape shape;
    yocto_instance instance;
    pathtracer_internal_t *p = pt->p;
    const layer_t *layers, *layer;

    layers = goxel_get_render_layers(false);
    DL_FOREACH(layers, layer) {
        if (!layer->visible || !layer->mesh) continue;
        k = mesh_get_key(layer->mesh);
        key = XXH32(&k, sizeof(k), key);
        i = get_material_id(pt, layer->material, &changed);
        key = XXH32(&i, sizeof(i), key);
    }
    key = XXH32(goxel.back_color, sizeof(goxel.back_color), key);
    key = XXH32(&w, sizeof(w), key);
    key = XXH32(&h, sizeof(h), key);
    key = XXH32(&goxel.rend.settings.effects,
                sizeof(goxel.rend.settings.effects), key);
    key = XXH32(&pt->floor.type, sizeof(pt->floor.type), key);
    key = XXH32(&force, sizeof(force), key);
    if (!force && key == p->mesh_key) return changed;

    p->mesh_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);
    p->scene = {};
    p->lights = {};
    changed |= CHANGE_MESH;

    DL_FOREACH(layers, layer) {
        if (!layer->visible || !layer->mesh) continue;
        mesh = layer->mesh;
        iter = mesh_get_iterator(mesh,
                        MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
        while (mesh_iter(&iter, block_pos)) {
            shape = create_shape_for_block(mesh, block_pos);
            // shape.material = get_material_id(pt, layer->material, &changed);
            if (shape.positions.empty()) continue;
            p->scene.shapes.push_back(shape);
            instance.material = get_material_id(pt, layer->material, &changed);
            instance.uri = shape.uri;
            instance.shape = p->scene.shapes.size() - 1;
            instance.frame = translation_frame(vec3f(
                                    block_pos[0], block_pos[1], block_pos[2]));
            p->scene.instances.push_back(instance);
        }
    }

    return changed;
}

static int sync_floor(pathtracer_t *pt, bool force)
{
    pathtracer_internal_t *p = pt->p;
    uint64_t key = 0;
    yocto_shape *shape;
    yocto_instance *instance;
    int i;
    vec4f color;
    float pos[3] = {0, 0, 0};
    int changed = 0;

    key = XXH32(&pt->floor, sizeof(pt->floor), key);
    if (!force && key == p->floor_key) return 0;
    changed |= CHANGE_FLOOR;
    p->floor_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);

    if (pt->floor.type == PT_FLOOR_NONE) return changed;

    color[0] = pt->floor.color[0] / 255.f;
    color[1] = pt->floor.color[1] / 255.f;
    color[2] = pt->floor.color[2] / 255.f;
    color[3] = 1.0;

    if (!box_is_null(goxel.image->box)) {
        pos[0] = goxel.image->box[3][0];
        pos[1] = goxel.image->box[3][1];
        pos[2] = goxel.image->box[3][2] - goxel.image->box[2][2];
    }

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
    // shape->material = get_material_id(pt, pt->floor.material, &changed);
    instance = getdefault(p->scene.instances, "<floor>");
    instance->material = get_material_id(pt, pt->floor.material, &changed);
    instance->shape = getindex(p->scene.shapes, shape);
    instance->frame = translation_frame({pos[0], pos[1], pos[2]}) *
                      scaling_frame(
                        vec3f(pt->floor.size[0], pt->floor.size[1], 1.f));
    return changed;
}

static int sync_camera(pathtracer_t *pt, int w, int h,
                       const float viewport[4],
                       const camera_t *camera, bool force)
{
    pathtracer_internal_t *p = pt->p;
    yocto_camera *cam;
    uint64_t key;
    float m[4][4], aspect, viewport_aspect, fovy;

    add_cameras(p->scene);
    cam = &p->scene.cameras[0];

    key = XXH32(camera->view_mat, sizeof(camera->view_mat), 0);
    key = XXH32(camera->proj_mat, sizeof(camera->proj_mat), key);
    key = XXH32(&w, sizeof(w), key);
    key = XXH32(&h, sizeof(h), key);
    if (!force && key == p->camera_key) return 0;
    p->camera_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);

    mat4_copy(camera->mat, m);
    cam->frame = frame3f(mat4f({m[0][0], m[0][1], m[0][2], m[0][3]},
                               {m[1][0], m[1][1], m[1][2], m[1][3]},
                               {m[2][0], m[2][1], m[2][2], m[2][3]},
                               {m[3][0], m[3][1], m[3][2], m[3][3]}));
    aspect = (float)w / h;
    viewport_aspect = viewport[2] / viewport[3];
    fovy = camera->fovy / 180 * M_PI;
    if (viewport_aspect < aspect) {
        fovy *= viewport_aspect / aspect;
    }
    set_yperspective(*cam, fovy, aspect, camera->dist);

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

    key = XXH32(&pt->world.type, sizeof(pt->world.type), key);
    key = XXH32(&pt->world.energy, sizeof(pt->world.energy), key);
    key = XXH32(&pt->world.color, sizeof(pt->world.color), key);
    if (!force && key == p->world_key) return 0;
    p->world_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);

    texture = getdefault(p->scene.textures, "<world>");
    texture->uri = "textures/uniform.hdr";

    color[0] = pt->world.color[0] / 255.f;
    color[1] = pt->world.color[1] / 255.f;
    color[2] = pt->world.color[2] / 255.f;
    color[3] = 1.0;

    switch (pt->world.type) {
    case PT_WORLD_NONE:
        return true;
    case PT_WORLD_SKY:
        texture->hdr = make_sunsky({512, 256}, pif / 4, turbidity, has_sun,
                          1.0f, 0, {color.x, color.y, color.z});
        break;
    case PT_WORLD_UNIFORM:
        texture->hdr = {{64, 64}, color};
        break;
    }
    environment = getdefault(p->scene.environments, "<world>");
    environment->emission = vec3f{1, 1, 1} * pt->world.energy;
    environment->emission_tex = getindex(p->scene.textures, texture);
    environment->frame = rotation_frame(vec3f{1.f, 0.f, 0.f}, pif / 2);

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
    key = XXH32(&pt->world, sizeof(pt->world), key);
    key = XXH32(&goxel.rend.light.intensity,
                sizeof(goxel.rend.light.intensity), key);
    key = XXH32(light_dir, sizeof(light_dir), key);

    if (!force && key == p->light_key) return 0;
    p->light_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);

    ke = goxel.rend.light.intensity;
    material = getdefault(p->scene.materials, "<light>");
    material->emission = {ke * d * d, ke * d * d, ke * d * d};

    shape = getdefault(p->scene.shapes, "<light>");
    shape->positions = {};
    shape->triangles = {};
    shape->positions.push_back({0, 0, 0});
    shape->positions.push_back({1, 0, 0});
    shape->positions.push_back({1, 1, 0});
    shape->triangles.push_back({0, 1, 2});

    instance = getdefault(p->scene.instances, "<light>");
    instance->material = getindex(p->scene.materials, material);
    instance->shape = getindex(p->scene.shapes, shape);
    instance->frame = translation_frame(
            {light_dir[0] * d, light_dir[1] * d, light_dir[2] * d});

    return CHANGE_LIGHT;
}

static int sync_options(pathtracer_t *pt, bool force)
{
    uint64_t key = 0;
    pathtracer_internal_t *p = pt->p;
    key = XXH32(&pt->num_samples, sizeof(pt->num_samples), key);
    if (!force && key == p->options_key) return 0;
    p->options_key = key;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);
    p->trace_prms.samples = pt->num_samples;
    p->trace_prms.resolution = max(pt->w, pt->h);
    return CHANGE_OPTIONS;
}

static void start_render(
        image4f& image, trace_state& state,
        const yocto_scene& scene, const bvh_scene& bvh,
        const trace_lights& lights,
        vector<future<void>>& futures, atomic<int>& current_sample,
        deque<image_region>& queue,
        mutex& queuem,
        const trace_params& trace_prms,
        atomic<bool>* cancel)
{
    auto& camera     = scene.cameras.at(trace_prms.camera);
    auto  image_size = camera_resolution(camera, trace_prms.resolution);
    state            = make_trace_state(image_size, trace_prms.seed);
    auto regions = make_regions(image.size(), trace_prms.region, true);
    if (cancel) *cancel = false;

    futures.clear();
    futures.emplace_back(async([trace_prms, regions, &current_sample, &image,
                                &scene, &lights, &bvh, &state, &queue,
                                &queuem, cancel]() {
        for (auto sample = 0; sample < trace_prms.samples;
             sample += trace_prms.batch) {
            if (cancel && *cancel) return;
            current_sample   = sample;
            auto num_samples = min(trace_prms.batch,
                                   trace_prms.samples - current_sample);
            auto futures  = vector<future<void>>{};
            int nthreads = std::thread::hardware_concurrency();
            std::atomic<size_t> next_idx(0);
            for (int thread_id = 0; thread_id < nthreads; thread_id++) {
                futures.emplace_back(async(std::launch::async,
                            [num_samples, &trace_prms, &image, &scene, &lights,
                             &bvh, &state, &queue, &queuem, &next_idx, cancel,
                             &regions]() {
                    while (true) {
                        if (cancel && *cancel) break;
                        auto idx = next_idx.fetch_add(1);
                        if (idx >= regions.size()) break;
                        auto region = regions[idx];
                        trace_region(image, state, scene, bvh, lights,
                                     region, num_samples, trace_prms);
                        {
                            std::lock_guard<mutex> guard{queuem};
                            queue.push_back(region);
                        }
                    }
                }));
            }
        }
        current_sample = trace_prms.samples;
    }));
}

static int sync(pathtracer_t *pt, int w, int h, const float viewport[4],
                bool force)
{
    pathtracer_internal_t *p = pt->p;
    int changes = 0;

    if (force) changes |= CHANGE_FORCE;

    changes |= sync_mesh(pt, w, h, changes);
    changes |= sync_floor(pt, changes);
    changes |= sync_world(pt, changes);
    changes |= sync_light(pt, changes);
    changes |= sync_camera(pt, w, h, viewport,
                           goxel.image->active_camera, changes);
    changes |= sync_options(pt, changes);

    if (changes) {
        add_materials(p->scene);
        update_transforms(p->scene);
    }

    // Update BVH if needed.
    if (changes & (CHANGE_MESH | CHANGE_LIGHT | CHANGE_FLOOR)) {
        p->bvh = make_bvh(p->scene, p->bvh_prms);
    }
    if (changes & (CHANGE_LIGHT | CHANGE_MATERIAL)) {
        p->lights = make_trace_lights(p->scene);
    }

    if (changes) {
        if (p->lights.instances.empty() &&
                p->lights.environments.empty()) {
            p->trace_prms.sampler = trace_params::sampler_type::eyelight;
        }
        p->trace_prms.resolution = max(w, h);
        p->image = image4f({w, h});
        p->display = image4f({w, h});

        stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                    &p->trace_stop);

        p->state = make_trace_state({w, h});
        p->trace_sample = 0;
        p->trace_stop = false;
        start_render(p->image, p->state, p->scene,
                     p->bvh, p->lights,
                     p->trace_futures, p->trace_sample,
                     p->trace_queue, p->trace_queuem, p->trace_prms,
                     &p->trace_stop);
    }
    return changes;
}

static void make_preview(pathtracer_t *pt)
{
    int i, j, pi, pj;
    pathtracer_internal_t *p = pt->p;
    trace_params preview_prms = p->trace_prms;
    int preview_ratio = 8;
    image4f preview;
    vec4b v;

    preview_prms.resolution /= preview_ratio;
    preview_prms.samples = 1;

    preview = trace_image(p->scene, p->bvh, p->lights, preview_prms);
    preview = tonemap(preview, p->tonemap_prms);

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
 *   viewport - The full view viewport.
 */
void pathtracer_iter(pathtracer_t *pt, const float viewport[4])
{
    pathtracer_internal_t *p;
    int changes, i, j, size = 0;
    image_region region = image_region{};
    vec4b v;

    if (!pt->p) pt->p = new pathtracer_internal_t();
    p = pt->p;
    p->trace_prms.resolution = max(pt->w, pt->h);
    changes = sync(pt, pt->w, pt->h, viewport, pt->force_restart);
    pt->force_restart = false;
    assert(p->display.size()[0] == pt->w);
    assert(p->display.size()[1] == pt->h);
    if (changes) pt->status = PT_RUNNING;

    if (changes & CHANGE_CAMERA) {
        make_preview(pt);
        pt->progress = 0;
        return;
    }

    while (try_pop(p->trace_queue, p->trace_queuem, region)) {
        tonemap(p->display, p->image, region, p->tonemap_prms);
        for (i = region.min[1]; i < region.max[1]; i++)
        for (j = region.min[0]; j < region.max[0]; j++) {
            v = float_to_byte(p->display[{j, i}]);
            memcpy(&pt->buf[(i * pt->w + j) * 4], &v, 4);
        }
        size += region.size().x * region.size().y;
        if (size >= p->image.size().x * p->image.size().y) break;
    }
    pt->progress = (float)p->trace_sample / p->trace_prms.samples;

    if (pt->status != PT_FINISHED &&
            p->trace_sample == p->trace_prms.samples) {
        pt->status = PT_FINISHED;
    }
}


/*
 * Stop the pathtracer thread if it is running.
 */
void pathtracer_stop(pathtracer_t *pt)
{
    pathtracer_internal_t *p = pt->p;
    if (!p) return;
    stop_render(p->trace_futures, p->trace_queue, p->trace_queuem,
                &p->trace_stop);
    delete p;
    pt->p = nullptr;
}

#else // Dummy implementation.

extern "C" {
#include "goxel.h"
}

void pathtracer_iter(pathtracer_t *pt, const float viewport[4]) {}
void pathtracer_stop(pathtracer_t *pt) {}

#endif // YOCTO
