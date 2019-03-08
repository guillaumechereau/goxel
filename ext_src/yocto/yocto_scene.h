//
// # Yocto/Scene: Tiny library for scene representation
//
//
// Yocto/Scene is a library to represent 3D scenes using a simple data-driven
// and value oriented design.
//
//
// ## Simple scene representation
//
// Yocto/Scene define a simple scene data structure useful to create quick demos
// and as the repsetnation upon which the path tracer works.
//
// In Yocto scenes, shapes are represented as indexed collections of points,
// lines, triangles, quads and bezier segments. Each shape may contain
// only one element type. Shapes are organized into a scene by creating shape
// instances, each its own transform. Materials are specified like in OBJ and
// glTF and include emission, base-metallic and diffuse-specular
// parametrization, normal, occlusion and displacement mapping. Finally, the
// scene containers cameras and environment maps. Quad support in shapes is
// experimental and mostly supported for loading and saving. Lights in
// Yocto/Scene are pointers to either instances or environments. The scene
// supports an optional node hierarchy with animation modeled on the glTF model.
//
// 1. load a scene with Yocto/SceneIO,
// 2. use `compute_shape_box()/compute_scene_box()` to compute element bounds
// 3. compute interpolated values over scene elements with `evaluate_XXX()`
//    functions
// 4. for ray-intersection and closest point queries, create a BVH with
//    `build_scene_bvh()` and intersect with with `intersect_scene_bvh()`;
//     you can also update the BVH with `refit_scene_bvh()`
//
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2019 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#ifndef _YOCTO_SCENE_H_
#define _YOCTO_SCENE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_bvh.h"
#include "yocto_image.h"
#include "yocto_math.h"

// -----------------------------------------------------------------------------
// SCENE DATA
// -----------------------------------------------------------------------------
namespace yocto {

// Camera based on a simple lens model. The camera is placed using a frame.
// Camera projection is described in photorgaphics terms. In particular,
// we specify fil size (35mm by default), the focal lengthm the focus
// distance and the lens_aperture. All values are in meters.
// Here are some common aspect ratios used in video and still photography.
// 3:2    on 35 mm:  0.036 x 0.024
// 16:9   on 35 mm:  0.036 x 0.02025 or 0.04267 x 0.024
// 2.35:1 on 35 mm:  0.036 x 0.01532 or 0.05640 x 0.024
// 2.39:1 on 35 mm:  0.036 x 0.01506 or 0.05736 x 0.024
// 2.4:1  on 35 mm:  0.036 x 0.015   or 0.05760 x 0.024 (approx. 2.39 : 1)
// To compute good apertures, one can use the F-stop number from phostography
// and set the aperture to focal_leangth/f_stop.
struct yocto_camera {
    string  name           = "";
    frame3f frame          = identity_frame3f;
    bool    orthographic   = false;
    float   film_width     = 0.036f;
    float   film_height    = 0.024f;
    float   focal_length   = 0.050f;
    float   focus_distance = float_max;
    float   lens_aperture  = 0;
};

// Texture containing either an LDR or HDR image. Textures are rendered
// using linear interpolation (unless `no_interoilation` is set) and
// weith tiling (unless `clamp_to_edge` is set). HdR images are encoded
// in linear color space, while LDRs are encoded as sRGB. The latter
// conversion can be disabled with `ldr_as_linear` for example to render
// normal maps.
struct yocto_texture {
    string  name             = "";
    string  filename         = "";
    image4f hdr_image        = {};
    image4b ldr_image        = {};
    bool    clamp_to_edge    = false;
    bool    no_interpolation = false;
    float   height_scale     = 1;
    bool    ldr_as_linear    = false;
};

// Volumetric texture containing a float only volume data. See texture
// above for other propoerties.
struct yocto_voltexture {
    string   name             = "";
    string   filename         = "";
    volume1f volume_data      = {};
    bool     clamp_to_edge    = false;
    bool     no_interpolation = false;
};

// Material for surfaces, lines and triangles.
// For surfaces, uses a microfacet model with thin sheet transmission.
// The model is based on OBJ, but contains glTF compatibility.
// For the documentation on the values, please see the OBJ format.
struct yocto_material {
    string name          = "";
    bool   base_metallic = false;  // base-metallic parametrization
    bool   gltf_textures = false;  // glTF packed textures

    // base values
    vec3f emission     = {0, 0, 0};
    vec3f diffuse      = {0, 0, 0};
    vec3f specular     = {0, 0, 0};
    vec3f transmission = {0, 0, 0};
    float roughness    = 0.0001;
    float opacity      = 1;
    bool  fresnel      = true;
    bool  refract      = false;

    // textures
    int emission_texture     = -1;
    int diffuse_texture      = -1;
    int specular_texture     = -1;
    int transmission_texture = -1;
    int roughness_texture    = -1;
    int displacement_texture = -1;
    int normal_texture       = -1;

    // volume properties
    // albedo = scattering / (absorption + scattering)
    // density = absorption + scattering
    vec3f volume_emission = {0, 0, 0};
    vec3f volume_albedo   = {0, 0, 0};
    vec3f volume_density  = {0, 0, 0};
    float volume_phaseg   = 0;

    // volume textures
    int volume_density_texture = -1;
};

// Shape data represented as an indexed meshes of elements.
// May contain either points, lines, triangles and quads.
// Additionally, we support faceavarying primitives where each verftex data
// has its own topology.
struct yocto_shape {
    // shape data
    string name     = "";
    string filename = "";
    int    material = -1;

    // subdision properties
    int  subdivision_level    = 0;
    bool catmull_clark        = false;
    bool compute_normals      = false;
    bool preserve_facevarying = false;

    // primitives
    vector<int>   points    = {};
    vector<vec2i> lines     = {};
    vector<vec3i> triangles = {};
    vector<vec4i> quads     = {};

    // face-varying primitives
    vector<vec4i> quads_positions     = {};
    vector<vec4i> quads_normals       = {};
    vector<vec4i> quads_texturecoords = {};

    // vertex data
    vector<vec3f> positions     = {};
    vector<vec3f> normals       = {};
    vector<vec2f> texturecoords = {};
    vector<vec4f> colors        = {};
    vector<float> radius        = {};
    vector<vec4f> tangentspaces = {};
};

// Instance of a visible shape in the scene.
struct yocto_instance {
    string  name  = "";
    frame3f frame = identity_frame3f;
    int     shape = -1;
};

// Environment map.
struct yocto_environment {
    string  name             = "";
    frame3f frame            = identity_frame3f;
    vec3f   emission         = {0, 0, 0};
    int     emission_texture = -1;
};

// Node in a transform hierarchy.
struct yocto_scene_node {
    string        name        = "";
    int           parent      = -1;
    frame3f       local       = identity_frame3f;
    vec3f         translation = {0, 0, 0};
    vec4f         rotation    = {0, 0, 0, 1};
    vec3f         scale       = {1, 1, 1};
    vector<float> weights     = {};
    int           camera      = -1;
    int           instance    = -1;
    int           environment = -1;

    // compute properties
    vector<int> children = {};
};

// Keyframe type.
enum struct yocto_interpolation_type { linear, step, bezier };

// Keyframe data.
struct yocto_animation {
    string                   name            = "";
    string                   filename        = "";
    string                   animation_group = "";
    yocto_interpolation_type interpolation_type =
        yocto_interpolation_type::linear;
    vector<float>         keyframes_times         = {};
    vector<vec3f>         translation_keyframes   = {};
    vector<vec4f>         rotation_keyframes      = {};
    vector<vec3f>         scale_keyframes         = {};
    vector<vector<float>> morph_weights_keyframes = {};
    vector<int>           node_targets            = {};
};

// Scene comprised an array of objects whose memory is owened by the scene.
// All members are optional,Scene objects (camera, instances, environments)
// have transforms defined internally. A scene can optionally contain a
// node hierarchy where each node might point to a camera, instance or
// environment. In that case, the element transforms are computed from
// the hierarchy. Animation is also optional, with keyframe data that
// updates node transformations only if defined.
struct yocto_scene {
    string                    name         = "";
    vector<yocto_camera>      cameras      = {};
    vector<yocto_shape>       shapes       = {};
    vector<yocto_instance>    instances    = {};
    vector<yocto_material>    materials    = {};
    vector<yocto_texture>     textures     = {};
    vector<yocto_environment> environments = {};
    vector<yocto_voltexture>  voltextures  = {};
    vector<yocto_scene_node>  nodes        = {};
    vector<yocto_animation>   animations   = {};
};

}  // namespace yocto

// -----------------------------------------------------------------------------
// SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Merge a scene into another
void merge_scene_into(yocto_scene& scene, const yocto_scene& merge);

// Print scene statistics.
string print_scene_stats(const yocto_scene& scene);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EVALUATION OF SCENE PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Update node transforms.
void update_transforms(
    yocto_scene& scene, float time = 0, const string& anim_group = "");

// Compute animation range.
vec2f compute_animation_range(
    const yocto_scene& scene, const string& anim_group = "");

// Computes shape/scene approximate bounds.
bbox3f compute_shape_bounds(const yocto_shape& shape);
bbox3f compute_scene_bounds(const yocto_scene& scene);

// Compute shape vertex normals
void compute_shape_normals(const yocto_shape& shape, vector<vec3f>& normals);

// Low level make/update bvh functions.
void build_scene_bvh(const yocto_scene& scene, bvh_scene& bvh,
    const build_bvh_options& options = {});
void refit_scene_bvh(const yocto_scene& scene, bvh_scene& bvh,
    const vector<int>& updated_instances, const vector<int>& updated_shapes);

// Apply subdivision and displacement rules.
void tesselate_shapes(yocto_scene& scene);

// Add missing names, normals, tangents and hierarchy.
void add_missing_names(yocto_scene& scene);
void add_missing_normals(yocto_scene& scene);
void add_missing_tangent_space(yocto_scene& scene);
void add_missing_materials(yocto_scene& scene);
void add_missing_cameras(yocto_scene& scene);

// Add a sky environment
void add_sky_environment(yocto_scene& scene, float sun_angle = pif / 4);

// Checks for validity of the scene.
void print_validation_errors(
    const yocto_scene& scene, bool skip_textures = false);

// Shape values interpolated using barycentric coordinates.
vec3f evaluate_shape_position(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
vec3f evaluate_shape_normal(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
vec2f evaluate_shape_texturecoord(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
vec4f evaluate_shape_color(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
float evaluate_shape_radius(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
pair<vec3f, bool> evaluate_shape_tangentspace(
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
vec3f evaluate_shape_perturbed_normal(const yocto_scene& scene,
    const yocto_shape& shape, int element_id, const vec2f& element_uv);
// Shape element values.
vec3f evaluate_shape_element_normal(const yocto_shape& shape, int element_id);
pair<vec3f, bool> evaluate_shape_element_tangentspace(
    const yocto_shape& shape, int element_id, const vec2f& element_uv = zero2f);

// Sample a shape element based on area/length.
void compute_shape_elements_cdf(const yocto_shape& shape, vector<float>& cdf);
pair<int, vec2f> sample_shape_element(const yocto_shape& shape,
    const vector<float>& elem_cdf, float re, const vec2f& ruv);
float            sample_shape_element_pdf(const yocto_shape& shape,
               const vector<float>& elem_cdf, int element_id, const vec2f& element_uv);

// Evaluate a texture.
vec2i evaluate_texture_size(const yocto_texture& texture);
vec4f lookup_texture(const yocto_texture& texture, int i, int j);
vec4f evaluate_texture(const yocto_texture& texture, const vec2f& texcoord);
float lookup_voltexture(const yocto_voltexture& texture, int i, int j, int k);
float evaluate_voltexture(
    const yocto_voltexture& texture, const vec3f& texcoord);

// Set and evaluate camera parameters. Setters take zeros as default values.
float get_camera_fovx(const yocto_camera& camera);
float get_camera_fovy(const yocto_camera& camera);
float get_camera_aspect(const yocto_camera& camera);
vec2i get_camera_image_size(const yocto_camera& camera, const vec2i& size);
void  set_camera_perspective(yocto_camera& camera, float fovy, float aspect,
     float focus, float height = 0.024f);
// Sets camera field of view to enclose all the bbox. Camera view direction
// fiom size and forcal lemgth can be overridden if we pass non zero values.
void set_camera_view_from_bbox(yocto_camera& camera, const bbox3f& bbox,
    const vec3f& view_direction = zero3f, float width = 0, float height = 0,
    float focal = 0);

// Generates a ray from a camera image coordinate and lens coordinates.
ray3f evaluate_camera_ray(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv);
// Generates a ray from a camera for pixel `image_ij`, the image size,
// the sub-pixel coordinates `pixel_uv` and the lens coordinates `lens_uv`
// and the image resolution `image_size`.
ray3f evaluate_camera_ray(const yocto_camera& camera, const vec2i& image_ij,
    const vec2i& image_size, const vec2f& pixel_uv, const vec2f& lens_uv);
// Generates a ray from a camera for pixel index `idx`, the image size,
// the sub-pixel coordinates `pixel_uv` and the lens coordinates `lens_uv`.
ray3f evaluate_camera_ray(const yocto_camera& camera, int idx,
    const vec2i& image_size, const vec2f& pixel_uv, const vec2f& lens_uv);

// Evaluates material parameters: emission, diffuse, specular, transmission,
// roughness and opacity.
vec3f evaluate_material_emission(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord);
// float evaluate_material_opacity(const yocto_scene& scene,
//     const yocto_material& material, const vec2f& texturecoord);
vec3f evaluate_material_normalmap(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord);

// Material values packed into a convenience structure.
struct microfacet_brdf {
    vec3f diffuse      = zero3f;
    vec3f specular     = zero3f;
    vec3f transmission = zero3f;
    float roughness    = 1;
    float opacity      = 1;
    bool  fresnel      = true;
    bool  refract      = false;
};
microfacet_brdf evaluate_material_brdf(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord);
bool            is_brdf_delta(const microfacet_brdf& f);
bool            is_brdf_zero(const microfacet_brdf& f);

// Check volume properties.
bool is_material_volume_homogeneus(const yocto_material& vol);
bool is_material_volume_colored(const yocto_material& vol);

// Instance values interpolated using barycentric coordinates.
// Handles defaults if data is missing.
vec3f evaluate_instance_position(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv);
vec3f evaluate_instance_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv);
vec3f evaluate_instance_perturbed_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv);
// Instance element values.
vec3f evaluate_instance_element_normal(
    const yocto_scene& scene, const yocto_instance& instance, int element_id);

// Environment texture coordinates from the incoming direction.
vec2f evaluate_environment_texturecoord(
    const yocto_environment& environment, const vec3f& direction);
// Evaluate the incoming direction from the element_uv.
vec3f evaluate_environment_direction(
    const yocto_environment& environment, const vec2f& environment_uv);
// Evaluate the environment emission.
vec3f evaluate_environment_emission(const yocto_scene& scene,
    const yocto_environment& environment, const vec3f& direction);
// Evaluate all environment emission.
vec3f evaluate_environment_emission(
    const yocto_scene& scene, const vec3f& direction);

// Sample an environment based on either texel values of uniform
void  compute_environment_texels_cdf(const yocto_scene& scene,
     const yocto_environment& environment, vector<float>& texels_cdf);
vec3f sample_environment_direction(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    float re, const vec2f& ruv);
float sample_environment_direction_pdf(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    const vec3f& direction);

}  // namespace yocto

// -----------------------------------------------------------------------------
// ANIMATION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Find the first keyframe value that is greater than the argument.
inline int evaluate_keyframed_index(
    const vector<float>& times, const float& time);

// Evaluates a keyframed value using step interpolation.
template <typename T>
inline T evaluate_keyframed_step(
    const vector<float>& times, const vector<T>& vals, float time);

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline vec4f evaluate_keyframed_slerp(
    const vector<float>& times, const vector<vec4f>& vals, float time);

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline T evaluate_keyframed_linear(
    const vector<float>& times, const vector<T>& vals, float time);

// Evaluates a keyframed value using Bezier interpolation.
template <typename T>
inline T evaluate_keyframed_bezier(
    const vector<float>& times, const vector<T>& vals, float time);

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF ANIMATION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Find the first keyframe value that is greater than the argument.
inline int evaluate_keyframed_index(
    const vector<float>& times, const float& time) {
    for (auto i = 0; i < times.size(); i++)
        if (times[i] > time) return i;
    return (int)times.size();
}

// Evaluates a keyframed value using step interpolation.
template <typename T>
inline T evaluate_keyframed_step(
    const vector<float>& times, const vector<T>& vals, float time) {
    if (time <= times.front()) return vals.front();
    if (time >= times.back()) return vals.back();
    time     = clamp(time, times.front(), times.back() - 0.001f);
    auto idx = evaluate_keyframed_index(times, time);
    return vals.at(idx - 1);
}

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline vec4f evaluate_keyframed_slerp(
    const vector<float>& times, const vector<vec4f>& vals, float time) {
    if (time <= times.front()) return vals.front();
    if (time >= times.back()) return vals.back();
    time     = clamp(time, times.front(), times.back() - 0.001f);
    auto idx = evaluate_keyframed_index(times, time);
    auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
    return slerp(vals.at(idx - 1), vals.at(idx), t);
}

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline T evaluate_keyframed_linear(
    const vector<float>& times, const vector<T>& vals, float time) {
    if (time <= times.front()) return vals.front();
    if (time >= times.back()) return vals.back();
    time     = clamp(time, times.front(), times.back() - 0.001f);
    auto idx = evaluate_keyframed_index(times, time);
    auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
    return vals.at(idx - 1) * (1 - t) + vals.at(idx) * t;
}

// Evaluates a keyframed value using Bezier interpolation.
template <typename T>
inline T evaluate_keyframed_bezier(
    const vector<float>& times, const vector<T>& vals, float time) {
    if (time <= times.front()) return vals.front();
    if (time >= times.back()) return vals.back();
    time     = clamp(time, times.front(), times.back() - 0.001f);
    auto idx = evaluate_keyframed_index(times, time);
    auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
    return interpolate_bezier(
        vals.at(idx - 3), vals.at(idx - 2), vals.at(idx - 1), vals.at(idx), t);
}

}  // namespace yocto

#endif
