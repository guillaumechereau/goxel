/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifdef WITH_CYCLES

#include "device/device.h"
#include "render/background.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/graph.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/session.h"
#include "render/shader.h"
#include "util/util_transform.h"
#include "util/util_foreach.h"
#include "device/device_memory.h"

#include <memory> // for make_unique

extern "C" {
#include "goxel.h"
}

// Convenience macro for cycles string creation.
#define S(v) ccl::ustring(v)

static ccl::Session *g_session = NULL;
static ccl::BufferParams g_buffer_params;
static ccl::SessionParams g_session_params;

static ccl::Shader *create_cube_shader(void)
{
    ccl::Shader *shader = new ccl::Shader();
    shader->name = "cubeShader";
    ccl::ShaderGraph *shaderGraph = new ccl::ShaderGraph();

    const ccl::NodeType *colorNodeType = ccl::NodeType::find(S("attribute"));
    ccl::ShaderNode *colorShaderNode = static_cast<ccl::ShaderNode*>(
            colorNodeType->create(colorNodeType));
    colorShaderNode->name = "colorNode";
    colorShaderNode->set(*colorShaderNode->type->find_input(S("attribute")),
                         S("Col"));
    shaderGraph->add(colorShaderNode);

    const ccl::NodeType *diffuseBSDFNodeType = ccl::NodeType::find(S("diffuse_bsdf"));
    ccl::ShaderNode *diffuseBSDFShaderNode = static_cast<ccl::ShaderNode*>(
            diffuseBSDFNodeType->create(diffuseBSDFNodeType));
    diffuseBSDFShaderNode->name = "diffuseBSDFNode";
    shaderGraph->add(diffuseBSDFShaderNode);

    shaderGraph->connect(
        colorShaderNode->output("Color"),
        diffuseBSDFShaderNode->input("Color")
    );
    shaderGraph->connect(
        diffuseBSDFShaderNode->output("BSDF"),
        shaderGraph->output()->input("Surface")
    );

    shader->set_graph(shaderGraph);
    return shader;
}

static ccl::Shader *create_light_shader(void)
{
    ccl::Shader *shader = new ccl::Shader();
    shader->name = "lightShader";
    ccl::ShaderGraph *shaderGraph = new ccl::ShaderGraph();

    const ccl::NodeType *emissionNodeType = ccl::NodeType::find(S("emission"));
    ccl::ShaderNode *emissionShaderNode = static_cast<ccl::ShaderNode*>(
            emissionNodeType->create(emissionNodeType));
    emissionShaderNode->name = "emissionNode";
    emissionShaderNode->set(
        *emissionShaderNode->type->find_input(S("strength")),
        1.0f
    );
    emissionShaderNode->set(
        *emissionShaderNode->type->find_input(S("color")),
        ccl::make_float3(1.0, 1.0, 1.0)
    );

    shaderGraph->add(emissionShaderNode);

    shaderGraph->connect(
        emissionShaderNode->output("Emission"),
        shaderGraph->output()->input("Surface")
    );

    shader->set_graph(shaderGraph);
    return shader;
}

static ccl::Shader *create_background_shader(void)
{
    ccl::Shader *shader = new ccl::Shader();
    shader->name = "backgroundShader";
    ccl::ShaderGraph *shaderGraph = new ccl::ShaderGraph();

    const ccl::NodeType *backgroundNodeType =
            ccl::NodeType::find(S("background_shader"));
    ccl::ShaderNode *backgroundShaderNode = static_cast<ccl::BackgroundNode*>(
            backgroundNodeType->create(backgroundNodeType));
    backgroundShaderNode->name = "backgroundNode";
    backgroundShaderNode->set(
        *backgroundShaderNode->type->find_input(S("color")),
        ccl::make_float3(goxel->back_color[0] / 255.0f,
                         goxel->back_color[1] / 255.0f,
                         goxel->back_color[2] / 255.0f)
    );
    backgroundShaderNode->set(
        *backgroundShaderNode->type->find_input(S("strength")),
        0.5f
    );

    shaderGraph->add(backgroundShaderNode);

    shaderGraph->connect(
        backgroundShaderNode->output("Background"),
        shaderGraph->output()->input("Surface")
    );

    shader->set_graph(shaderGraph);
    return shader;
}

static ccl::Mesh *create_mesh_for_block(
        const mesh_t *mesh, const int block_pos[3])
{
    ccl::Mesh *ret = NULL;
    int nb = 0, i, j;
    voxel_vertex_t* vertices;
    ccl::Attribute *attr;

    ret = new ccl::Mesh();
    ret->subdivision_type = ccl::Mesh::SUBDIVISION_NONE;

    vertices = (voxel_vertex_t*)calloc(
            BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE * 6 * 4, sizeof(*vertices));
    nb = mesh_generate_vertices(mesh, block_pos, 0, vertices);
    if (!nb) goto end;

    ret->reserve_mesh(nb * 4, nb * 2);
    for (i = 0; i < nb; i++) { // Once per quad.
        for (j = 0; j < 4; j++) {
            ret->add_vertex(ccl::make_float3(
                        vertices[i * 4 + j].pos[0],
                        vertices[i * 4 + j].pos[1],
                        vertices[i * 4 + j].pos[2]));
        }
        ret->add_triangle(i * 4 + 0, i * 4 + 1, i * 4 + 2, 0, false);
        ret->add_triangle(i * 4 + 2, i * 4 + 3, i * 4 + 0, 0, false);
    }

    // Set color attribute.
    attr = ret->attributes.add(S("Col"), ccl::TypeDesc::TypeColor,
            ccl::ATTR_ELEMENT_CORNER_BYTE);
    for (i = 0; i < nb * 6; i++) {
        attr->data_uchar4()[i] = ccl::make_uchar4(
                vertices[i / 6 * 4].color[0],
                vertices[i / 6 * 4].color[1],
                vertices[i / 6 * 4].color[2],
                vertices[i / 6 * 4].color[3]
        );
    }

end:
    free(vertices);
    return ret;
}

static void sync_scene(ccl::Scene *scene, int w, int h)
{
    mesh_t *gmesh = goxel->render_mesh;
    int block_pos[3];
    mesh_iterator_t iter;

    scene->camera->width = w;
    scene->camera->height = h;
    scene->camera->fov = 20.0 * DD2R;
    scene->camera->type = ccl::CameraType::CAMERA_PERSPECTIVE;
    scene->camera->full_width = scene->camera->width;
    scene->camera->full_height = scene->camera->height;
    scene->film->exposure = 1.0f;

    ccl::Shader *object_shader = create_cube_shader();
    object_shader->tag_update(scene);
    scene->shaders.push_back(object_shader);

    iter = mesh_get_iterator(gmesh,
            MESH_ITER_BLOCKS | MESH_ITER_INCLUDES_NEIGHBORS);
    while (mesh_iter(&iter, block_pos)) {
        ccl::Mesh *mesh = create_mesh_for_block(gmesh, block_pos);
        mesh->used_shaders.push_back(object_shader);
        scene->meshes.push_back(mesh);
        ccl::Object *object = new ccl::Object();
        object->name = "mesh";
        object->mesh = mesh;
        object->tfm = ccl::transform_identity() *
            ccl::transform_translate(ccl::make_float3(
                    block_pos[0], block_pos[1], block_pos[2]));
        scene->objects.push_back(object);
    }

    ccl::Light *light;

    light = new ccl::Light();

    ccl::Shader *light_shader = create_light_shader();
    light_shader->tag_update(scene);
    scene->shaders.push_back(light_shader);
    light->shader = light_shader;
    scene->lights.push_back(light);

    ccl::Shader *back_shader = create_background_shader();
    back_shader->tag_update(scene);
    scene->shaders.push_back(back_shader);
    scene->background->shader = back_shader;

    scene->camera->compute_auto_viewplane();
    scene->camera->need_update = true;
    scene->camera->need_device_update = true;
}

void cycles_init(void)
{
    ccl::DeviceType device_type;
    ccl::DeviceInfo device_info;
    ccl::vector<ccl::DeviceInfo>& devices = ccl::Device::available_devices();

    device_type = ccl::Device::type_from_string("CPU");
    for (const ccl::DeviceInfo& device : devices) {
        if (device_type == device.type) {
            device_info = device;
            break;
        }
    }
    g_session_params.progressive = true;
    g_session_params.start_resolution = 64;
    g_session_params.device = device_info;
    g_session_params.samples = 20;
    // g_session_params.threads = 1;
}

static bool sync_mesh(int w, int h)
{
    static uint64_t last_key = 0;
    uint64_t key;
    ccl::SceneParams scene_params;

    key = mesh_get_key(goxel->render_mesh);
    key = crc64(key, goxel->back_color, sizeof(goxel->back_color));
    key = crc64(key, (const uint8_t*)&w, sizeof(w));
    key = crc64(key, (const uint8_t*)&h, sizeof(h));

    if (key == last_key) return false;
    last_key = key;
    // For the moment I don't see how to update the mesh without crashing
    // the application, except by creating a new session!
    if (g_session) {
        delete g_session;
        g_session = NULL;
    }
    if (!g_session) {
        // scene_params.shadingsystem = ccl::SHADINGSYSTEM_OSL;
        scene_params.shadingsystem = ccl::SHADINGSYSTEM_SVM;
        // scene_params.persistent_data = true;
        g_session = new ccl::Session(g_session_params);
        g_session->scene = new ccl::Scene(scene_params, g_session->device);
        g_session->start();
    }
    if (!g_session->ready_to_reset()) return false;
    g_session->scene->mutex.lock();
    sync_scene(g_session->scene, w, h);
    g_session->scene->mutex.unlock();
    g_session->reset(g_buffer_params, g_session_params.samples);
    return true;
}

static bool sync_lights(int w, int h, bool force)
{
    static uint64_t last_key = 0;
    uint64_t key;
    float light_dir[3];
    ccl::Light *light;
    ccl::Scene *scene = g_session->scene;
    render_get_light_dir(&goxel->rend, light_dir);

    key = mesh_get_key(goxel->render_mesh);
    key = crc64(key, (uint8_t*)&goxel->rend.light, sizeof(goxel->rend.light));
    key = crc64(key, (uint8_t*)light_dir, sizeof(light_dir));

    if (!force && key == last_key) return false;

    light = scene->lights[0];
    light->type = ccl::LIGHT_DISTANT;
    light->size = 0.01f;
    light->dir = ccl::make_float3(-light_dir[0], -light_dir[1], -light_dir[2]);
    light->tag_update(scene);
    g_session->reset(g_buffer_params, g_session_params.samples);
    last_key = key;
    return true;
}

static bool sync_camera(int w, int h, const camera_t *camera, bool force)
{
    static uint64_t last_key = 0;
    uint64_t key;
    ccl::Scene *scene;
    float mat[4][4];
    float rot[4];

    key = crc64(0, (uint8_t*)camera->view_mat, sizeof(camera->view_mat));
    key = crc64(key, (uint8_t*)camera->proj_mat, sizeof(camera->proj_mat));
    key = crc64(key, (uint8_t*)&w, sizeof(w));
    key = crc64(key, (uint8_t*)&h, sizeof(h));

    if (!force && key == last_key) return false;

    scene = g_session->scene;
    scene->camera->width = w;
    scene->camera->height = h;
    scene->camera->fov = 20.0 * DD2R;
    scene->camera->type = ccl::CameraType::CAMERA_PERSPECTIVE;
    scene->camera->full_width = scene->camera->width;
    scene->camera->full_height = scene->camera->height;
    assert(sizeof(scene->camera->matrix) == sizeof(mat));
    mat4_set_identity(mat);
    mat4_itranslate(mat, -camera->ofs[0],
                         -camera->ofs[1],
                         -camera->ofs[2]);
    quat_copy(camera->rot, rot);
    rot[0] *= -1;
    mat4_imul_quat(mat, rot);
    mat4_itranslate(mat, 0, 0, camera->dist);
    mat4_iscale(mat, 1, 1, -1);
    mat4_transpose(mat, mat);
    memcpy(&scene->camera->matrix, mat, sizeof(mat));
    scene->camera->compute_auto_viewplane();
    scene->camera->need_update = true;
    scene->camera->need_device_update = true;
    g_session->reset(g_buffer_params, g_session_params.samples);

    last_key = key;
    return true;
}

static bool sync(int w, int h, const camera_t *cam)
{
    bool mesh_changed;
    mesh_changed = sync_mesh(w, h);
    sync_lights(w, h, mesh_changed);
    sync_camera(w, h, cam, mesh_changed);
    return true;
}

void cycles_render(uint8_t *buffer, int *w, int *h, const camera_t *cam,
                   float *progress)
{
    static ccl::DeviceDrawParams draw_params = ccl::DeviceDrawParams();

    g_buffer_params.width = *w;
    g_buffer_params.height = *h;
    g_buffer_params.full_width = *w;
    g_buffer_params.full_height = *h;

    sync(*w, *h, cam);
    if (!g_session) return;

    std::unique_lock<std::mutex> lock(g_session->display_mutex);
    if (!g_session->display->draw_ready()) return;

    *w = g_session->display->draw_width;
    *h = g_session->display->draw_height;
    uint8_t *rgba = (uint8_t*)g_session->display->rgba_byte.host_pointer;
    memcpy(buffer, rgba, (*w) * (*h) * 4);

    std::string status;
    std::string substatus;
    g_session->progress.get_status(status, substatus);
    if (progress) *progress = g_session->progress.get_progress();
}

void cycles_release(void)
{
    if (!g_session) return;
    delete g_session;
    g_session = NULL;
}

#else
// Dummy implementations.
extern "C" {
#include "goxel.h"
}
void cycles_init(void) {}
void cycles_release(void) {}
void cycles_render(uint8_t *buffer, int *w, int *h, const camera_t *cam,
                   float *progress) {}

#endif
