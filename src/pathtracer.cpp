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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define STB_IMAGE_STATIC

#include "../ext_src/yocto/yocto_bvh.h"
#include "../ext_src/yocto/yocto_scene.h"
#include "../ext_src/yocto/yocto_trace.h"

#pragma GCC diagnostic pop

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

enum {
    CHANGE_VOLUME       = 1 << 0,
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
    uint64_t volume_key;
    uint64_t camera_key;
    uint64_t world_key;
    uint64_t light_key;
    uint64_t floor_key;
    uint64_t options_key;

    int to_sync; // Mask of changes waiting to be applied.

    scene_data scene;
    trace_bvh bvh;
    trace_context context;

    // image_data image;
    // image_data display;

    trace_state state;
    trace_lights lights;
    trace_params params;
    float exposure;

    int trace_sample;
};

// Add a material to the scene and return its id.
static int add_material(pathtracer_t *pt, const material_t *mat)
{
    const material_t default_mat = MATERIAL_DEFAULT;
    if (!mat) mat = &default_mat;
    pt->p->scene.materials.push_back({
        .emission = {mat->emission[0], mat->emission[1], mat->emission[2]},
        .color = {mat->base_color[0], mat->base_color[1], mat->base_color[2]},
        .roughness = mat->roughness,
        .metallic = mat->metallic,
        .opacity = mat->base_color[3],
    });
    return pt->p->scene.materials.size() - 1;
}

static shape_data create_shape_for_tile(
        const volume_t *volume, const int tile_pos[3])
{
    voxel_vertex_t* vertices;
    int i, nb, size, subdivide;
    shape_data shape = {};

    vertices = (voxel_vertex_t*)calloc(
                TILE_SIZE * TILE_SIZE * TILE_SIZE * 6 * 4,
                sizeof(*vertices));
    nb = volume_generate_vertices(volume, tile_pos,
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


static int check_changes(pathtracer_t *pt)
{
    uint32_t key, k;
    const layer_t *layers, *layer;
    float light_dir[3];
    const camera_t *camera;
    int changes = 0;
    pathtracer_internal_t *p = pt->p;

    // Volume changes.
    key = 0;
    layers = goxel_get_render_layers(false);
    DL_FOREACH(layers, layer) {
        if (!layer->visible || !layer->volume) continue;
        k = volume_get_key(layer->volume);
        key = XXH32(&k, sizeof(k), key);
    }
    key = XXH32(goxel.back_color, sizeof(goxel.back_color), key);
    key = XXH32(&goxel.rend.settings.effects,
                sizeof(goxel.rend.settings.effects), key);
    key = XXH32(&pt->floor.type, sizeof(pt->floor.type), key);
    key = XXH32(&pt->floor, sizeof(pt->floor), key);
    if (key != p->volume_key) {
        p->volume_key = key;
        changes |= CHANGE_VOLUME;
    }

    // Env changes.
    key = 0;
    key = XXH32(&pt->world.type, sizeof(pt->world.type), key);
    key = XXH32(&pt->world.energy, sizeof(pt->world.energy), key);
    key = XXH32(&pt->world.color, sizeof(pt->world.color), key);
    if (key != p->world_key) {
        p->world_key = key;
        changes |= CHANGE_WORLD;
    }

    // Lights changes.
    key = 0;
    render_get_light_dir(&goxel.rend, light_dir);
    key = XXH32(&pt->world, sizeof(pt->world), key);
    key = XXH32(&goxel.rend.light.intensity,
                sizeof(goxel.rend.light.intensity), key);
    key = XXH32(light_dir, sizeof(light_dir), key);
    if (key != p->light_key) {
        p->light_key = key;
        changes |= CHANGE_LIGHT;
    }

    // Options changes.
    key = 0;
    key = XXH32(&pt->num_samples, sizeof(pt->num_samples), key);
    if (key != p->options_key) {
        p->options_key = key;
        changes |= CHANGE_OPTIONS;
    }

    // Camera changes.
    key = 0;
    camera = goxel.image->active_camera;
    key = XXH32(camera->view_mat, sizeof(camera->view_mat), 0);
    key = XXH32(camera->proj_mat, sizeof(camera->proj_mat), key);
    key = XXH32(&pt->w, sizeof(pt->w), key);
    key = XXH32(&pt->h, sizeof(pt->h), key);
    if (key != p->camera_key) {
        p->camera_key = key;
        changes |= CHANGE_CAMERA;
    }

    return changes;
}


static void update_camera(pathtracer_t *pt)
{
    camera_data *cam;
    camera_t *camera;
    float m[4][4], aspect, fovy, distance;
    pathtracer_internal_t *p = pt->p;

    if (p->scene.cameras.empty()) {
        add_camera(p->scene);
    }
    cam = &p->scene.cameras[0];
    camera = goxel.image->active_camera;

    mat4_copy(camera->mat, m);
    cam->frame = frame3f{{m[0][0], m[0][1], m[0][2]},
                         {m[1][0], m[1][1], m[1][2]},
                         {m[2][0], m[2][1], m[2][2]},
                         {m[3][0], m[3][1], m[3][2]}};
    aspect = (float)pt->w / pt->h;
    if (!camera->ortho) {
        fovy = camera->fovy / 180 * M_PI;
        cam->focus = camera->dist;
        distance = cam->film / (2 * tanf(fovy / 2));
        if (aspect > 1) distance /= aspect;
        cam->lens = cam->focus * distance / (cam->focus + distance);
        cam->aspect = aspect;
    } else {
        cam->orthographic = true;
        cam->film = pt->w;
        cam->aspect = aspect;
        cam->lens = cam->film / camera->dist;
        cam->focus = camera->dist;
    }
}

static void update_scene(pathtracer_t *pt)
{
    volume_iterator_t iter;
    const volume_t *volume;
    int tile_pos[3];
    shape_data shape;
    pathtracer_internal_t *p = pt->p;
    const layer_t *layers, *layer;
    float light_dir[3];
    float ke;
    const float d = 10000;
    bool has_sun = false;
    float turbidity = 3;
    float pos[3];
    vec4f color;
    image_data image;

    p->scene = {};
    p->lights = {};

    layers = goxel_get_render_layers(false);
    DL_FOREACH(layers, layer) {
        if (!layer->visible || !layer->volume) continue;
        volume = layer->volume;
        iter = volume_get_iterator(volume,
                        VOLUME_ITER_TILES | VOLUME_ITER_INCLUDES_NEIGHBORS);
        while (volume_iter(&iter, tile_pos)) {
            shape = create_shape_for_tile(volume, tile_pos);
            if (shape.positions.empty()) continue;
            p->scene.shapes.push_back(shape);
            p->scene.instances.push_back({
                .frame = translation_frame({
                        tile_pos[0], tile_pos[1], tile_pos[2]}),
                .shape = p->scene.shapes.size() - 1,
                .material = add_material(pt, layer->material),
            });
        }
    }

    // Add the floor.
    if (pt->floor.type != PT_FLOOR_NONE) {
        color[0] = pt->floor.color[0] / 255.f;
        color[1] = pt->floor.color[1] / 255.f;
        color[2] = pt->floor.color[2] / 255.f;
        color[3] = 1.0;
        if (!box_is_null(goxel.image->box)) {
            pos[0] = goxel.image->box[3][0];
            pos[1] = goxel.image->box[3][1];
            pos[2] = goxel.image->box[3][2] - goxel.image->box[2][2];
        } else {
            pos[0] = 0;
            pos[1] = 0;
            pos[2] = 0;
        }
        p->scene.shapes.push_back({
            .quads = {{0, 1, 3, 2}},
            .positions = {
                {-0.5, -0.5, 0}, {0.5, -0.5, 0}, {-0.5, 0.5, 0}, {0.5, 0.5, 0}
            },
            .normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}},
            .colors = {color, color, color, color},
        });
        p->scene.instances.push_back({
            .frame = translation_frame({pos[0], pos[1], pos[2]}) *
                      scaling_frame({
                              pt->floor.size[0], pt->floor.size[1], 1.f}),
            .shape = p->scene.shapes.size() - 1,
            .material = add_material(pt, pt->floor.material),
        });
    }

    // Add the env.
    if (pt->world.type != PT_WORLD_NONE) {
        color[0] = pt->world.color[0] / 255.f;
        color[1] = pt->world.color[1] / 255.f;
        color[2] = pt->world.color[2] / 255.f;
        color[3] = 1.0;
        switch (pt->world.type) {
        case PT_WORLD_UNIFORM:
            image = image_data {1, 1, true, vector<vec4f>(1, color)};
            break;
        case PT_WORLD_SKY:
            image = make_sunsky(512, 256, M_PI / 4, turbidity, has_sun,
                                1.0f, 0, {color.x, color.y, color.z});
            break;
        default:
            assert(false);
            break;
        }
        p->scene.textures.push_back(image_to_texture(image));
        p->scene.environments.push_back({
            .frame = rotation_frame(vec3f{1, 0, 0}, M_PI / 2),
            .emission = vec3f{1, 1, 1} * pt->world.energy,
            .emission_tex = p->scene.textures.size() - 1,
        });
    }

    // Add the light.
    ke = goxel.rend.light.intensity;
    render_get_light_dir(&goxel.rend, light_dir);
    p->scene.materials.push_back({
        .emission = {ke *d * d, ke * d * d, ke * d * d}});
    p->scene.shapes.push_back({
        .triangles = {{0, 1, 2}},
        .positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 2.0}},
    });
    p->scene.instances.push_back({
        .frame = translation_frame(
            {light_dir[0] * d, light_dir[1] * d, light_dir[2] * d}),
        .shape = p->scene.shapes.size() - 1,
        .material = p->scene.materials.size() - 1,
    });

    p->bvh = make_trace_bvh(p->scene, p->params);
    p->lights = make_trace_lights(p->scene, p->params);
}

static void update_preview(pathtracer_t *pt, const image_data &img)
{
    int i, j, pi, pj;
    vec4b v;

    for (i = 0; i < pt->h; i++) {
        for (j = 0; j < pt->w; j++) {
            pi = i * img.height / pt->h;
            pj = j * img.width / pt->w;
            v = float_to_byte(img[{pj, pi}]);
            memcpy(&pt->buf[(i * pt->w + j) * 4], &v, 4);
        }
    }
}

/*
 * Function: pathtracer_iter
 * Iter the rendering process of the current volume.
 *
 * Parameters:
 *   pt     - A pathtracer instance.
 *   viewport - The full view viewport.
 */
void pathtracer_iter(pathtracer_t *pt, const float viewport[4])
{
    pathtracer_internal_t *p;
    int changes;
    image_data image;

    if (!pt->p) {
        pt->p = new pathtracer_internal_t {
            .context = make_trace_context({}),
        };
    }
    p = pt->p;
    changes = check_changes(pt);

    if (pt->force_restart) {
        changes |= CHANGE_OPTIONS;
        pt->force_restart = false;
    }

    if (changes) {
        p->to_sync |= changes;
        trace_cancel(p->context);
    }

    if (!p->context.stop && !p->context.done) return;

    pt->status = PT_RUNNING;

    if (p->to_sync & (CHANGE_VOLUME | CHANGE_WORLD | CHANGE_LIGHT)) {
        update_scene(pt);
        p->to_sync |= CHANGE_CAMERA;
    }

    if (p->to_sync) {
        update_camera(pt);
        p->params.samples = pt->num_samples;
        p->params.resolution = max(pt->w, pt->h);
        p->state = make_trace_state(p->scene, p->params);
        image = make_image(p->state.width, p->state.height, true);
        trace_preview(image, p->context, p->state, p->scene, p->bvh,
                      p->lights, p->params);
        update_preview(pt, image);
        p->to_sync = 0;
    } else {
        image = get_image(p->state);
        update_preview(pt, image);
    }
    trace_start(p->context, p->state, p->scene, p->bvh, p->lights, p->params);

    pt->samples = p->state.samples;
    if (pt->samples == pt->num_samples) {
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
    trace_cancel(p->context);
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
