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
// 4. for ray-intersection and closest point queries, use
//    'make_bvh()`/`refit_bvh()`
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
// we specify fil size (35mm by default), the lens' focal length, the focus
// distance and the lens aperture. All values are in meters.
// Here are some common aspect ratios used in video and still photography.
// 3:2    on 35 mm:  0.036 x 0.024
// 16:9   on 35 mm:  0.036 x 0.02025 or 0.04267 x 0.024
// 2.35:1 on 35 mm:  0.036 x 0.01532 or 0.05640 x 0.024
// 2.39:1 on 35 mm:  0.036 x 0.01506 or 0.05736 x 0.024
// 2.4:1  on 35 mm:  0.036 x 0.015   or 0.05760 x 0.024 (approx. 2.39 : 1)
// To compute good apertures, one can use the F-stop number from phostography
// and set the aperture to focal_leangth/f_stop.
struct yocto_camera {
  string  uri          = "";
  frame3f frame        = identity3x4f;
  bool    orthographic = false;
  float   lens         = 0.050;
  vec2f   film         = {0.036, 0.024};
  float   focus        = flt_max;
  float   aperture     = 0;
};

// Texture containing either an LDR or HDR image. Textures are rendered
// using linear interpolation (unless `no_interoilation` is set) and
// weith tiling (unless `clamp_to_edge` is set). HdR images are encoded
// in linear color space, while LDRs are encoded as sRGB. The latter
// conversion can be disabled with `ldr_as_linear` for example to render
// normal maps.
struct yocto_texture {
  string       uri = "";
  image<vec4f> hdr = {};
  image<vec4b> ldr = {};
};

// Volumetric texture containing a float only volume data. See texture
// above for other propoerties.
struct yocto_voltexture {
  string        uri = "";
  volume<float> vol = {};
};

// Material for surfaces, lines and triangles.
// For surfaces, uses a microfacet model with thin sheet transmission.
// The model is based on OBJ, but contains glTF compatibility.
// For the documentation on the values, please see the OBJ format.
struct yocto_material {
  string uri = "";

  // lobes
  vec3f emission        = {0, 0, 0};
  vec3f diffuse         = {0, 0, 0};
  vec3f specular        = {0, 0, 0};
  float roughness       = 0;
  float metallic        = 0;
  vec3f coat            = {0, 0, 0};
  vec3f transmission    = {0, 0, 0};
  vec3f voltransmission = {0, 0, 0};
  vec3f volmeanfreepath = {0, 0, 0};
  vec3f volemission     = {0, 0, 0};
  vec3f volscatter      = {0, 0, 0};
  float volanisotropy   = 0;
  float volscale        = 0.01;
  float opacity         = 1;
  bool  thin            = false;

  // textures
  int  emission_tex     = -1;
  int  diffuse_tex      = -1;
  int  specular_tex     = -1;
  int  metallic_tex     = -1;
  int  roughness_tex    = -1;
  int  transmission_tex = -1;
  int  subsurface_tex   = -1;
  int  coat_tex         = -1;
  int  opacity_tex      = -1;
  int  normal_tex       = -1;
  bool gltf_textures    = false;  // glTF packed textures

  // volume textures
  int voldensity_tex = -1;
};

// Shape data represented as an indexed meshes of elements.
// May contain either points, lines, triangles and quads.
// Additionally, we support faceavarying primitives where each verftex data
// has its own topology.
struct yocto_shape {
  // shape data
  string uri = "";

  // primitives
  vector<int>   points    = {};
  vector<vec2i> lines     = {};
  vector<vec3i> triangles = {};
  vector<vec4i> quads     = {};

  // face-varying primitives
  vector<vec4i> quadspos      = {};
  vector<vec4i> quadsnorm     = {};
  vector<vec4i> quadstexcoord = {};

  // vertex data
  vector<vec3f> positions = {};
  vector<vec3f> normals   = {};
  vector<vec2f> texcoords = {};
  vector<vec4f> colors    = {};
  vector<float> radius    = {};
  vector<vec4f> tangents  = {};
};

// Shape data represented as an indexed meshes of elements.
// This object exists only to allow for further subdivision. The current
// subdiviion data is stored in the pointed to shape, so the rest of the system
// does not need to known about subdivs. While this is mostly helpful for
// subdivision surfaces, we store here all data that we possibly may want to
// subdivide, for later use.
struct yocto_subdiv {
  // shape data
  string uri = "";

  // tesselated shape
  int shape = -1;

  // subdision properties
  int  subdivisions = 0;
  bool catmullclark = false;
  bool smooth       = false;
  bool facevarying  = false;

  // displacement information
  float displacement     = 0;
  int   displacement_tex = -1;

  // primitives
  vector<int>   points    = {};
  vector<vec2i> lines     = {};
  vector<vec3i> triangles = {};
  vector<vec4i> quads     = {};

  // face-varying primitives
  vector<vec4i> quadspos      = {};
  vector<vec4i> quadsnorm     = {};
  vector<vec4i> quadstexcoord = {};

  // vertex data
  vector<vec3f> positions = {};
  vector<vec3f> normals   = {};
  vector<vec2f> texcoords = {};
  vector<vec4f> colors    = {};
  vector<float> radius    = {};
};

// Instance of a visible shape in the scene.
struct yocto_instance {
  string  uri      = "";
  frame3f frame    = identity3x4f;
  int     shape    = -1;
  int     material = -1;
};

// Environment map.
struct yocto_environment {
  string  uri          = "";
  frame3f frame        = identity3x4f;
  vec3f   emission     = {0, 0, 0};
  int     emission_tex = -1;
};

// Node in a transform hierarchy.
struct yocto_scene_node {
  string        uri         = "";
  int           parent      = -1;
  frame3f       local       = identity3x4f;
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

// Keyframe data.
struct yocto_animation {
  enum struct interpolation_type { linear, step, bezier };
  string                uri           = "";
  string                filename      = "";
  string                group         = "";
  interpolation_type    interpolation = interpolation_type::linear;
  vector<float>         times         = {};
  vector<vec3f>         translations  = {};
  vector<vec4f>         rotations     = {};
  vector<vec3f>         scales        = {};
  vector<vector<float>> morphs        = {};
  vector<int>           targets       = {};
};

// Scene comprised an array of objects whose memory is owened by the scene.
// All members are optional,Scene objects (camera, instances, environments)
// have transforms defined internally. A scene can optionally contain a
// node hierarchy where each node might point to a camera, instance or
// environment. In that case, the element transforms are computed from
// the hierarchy. Animation is also optional, with keyframe data that
// updates node transformations only if defined.
struct yocto_scene {
  string                    uri          = "";
  vector<yocto_camera>      cameras      = {};
  vector<yocto_shape>       shapes       = {};
  vector<yocto_instance>    instances    = {};
  vector<yocto_material>    materials    = {};
  vector<yocto_texture>     textures     = {};
  vector<yocto_environment> environments = {};
  vector<yocto_subdiv>      subdivs      = {};
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
void merge_scene(yocto_scene& scene, const yocto_scene& merge);

// Print scene statistics.
string format_stats(
    const yocto_scene& scene, const string& prefix = "", bool verbose = false);

// Add missing names, normals, tangents and hierarchy.
void add_normals(yocto_scene& scene);
void add_tangent_spaces(yocto_scene& scene);
void add_materials(yocto_scene& scene);
void add_cameras(yocto_scene& scene);
void add_radius(yocto_scene& scene, float radius = 0.001f);

// Normalize URIs and add missing ones. Assumes names are unique.
void normalize_uris(yocto_scene& sceme);
void rename_instances(yocto_scene& scene);

// Add a sky environment
void add_sky(yocto_scene& scene, float sun_angle = pif / 4);

// Reduce memory usage
void trim_memory(yocto_scene& scene);

// Checks for validity of the scene.
void print_validation(const yocto_scene& scene, bool notextures = false);

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
bbox3f compute_bounds(const yocto_shape& shape);
bbox3f compute_bounds(const yocto_scene& scene);

// Compute shape vertex normals
vector<vec3f> compute_normals(const yocto_shape& shape);
void          compute_normals(const yocto_shape& shape, vector<vec3f>& normals);

// Apply subdivision and displacement rules.
void subdivide_shape(yocto_shape& shape, int subdivisions, bool catmullclark,
    bool compute_normals);
void displace_shape(yocto_shape& shape, const yocto_texture& displacement,
    float scale, bool compute_normals);
void tesselate_subdiv(yocto_scene& scene, yocto_subdiv& subdiv);
void tesselate_subdivs(yocto_scene& scene);

// Build/refit the bvh acceleration structure.
bvh_scene make_bvh(const yocto_scene& scene, const bvh_params& params);
void      make_bvh(
         bvh_scene& bvh, const yocto_scene& scene, const bvh_params& params);
void refit_bvh(bvh_scene& bvh, const yocto_scene& scene,
    const vector<int>& updated_shapes, const bvh_params& params);

// Shape values interpolated by interpoalting vertex values of the `eid` element
// with its barycentric coordinates `euv`.
vec3f eval_position(const yocto_shape& shape, int element, const vec2f& uv);
vec3f eval_normal(const yocto_shape& shape, int element, const vec2f& uv);
vec2f eval_texcoord(const yocto_shape& shape, int element, const vec2f& uv);
vec4f eval_color(const yocto_shape& shape, int element, const vec2f& uv);
float eval_radius(const yocto_shape& shape, int element, const vec2f& uv);
pair<mat3f, bool> eval_tangent_basis(
    const yocto_shape& shape, int element, const vec2f& uv);
// Shape element values.
vec3f              eval_element_normal(const yocto_shape& shape, int element);
pair<vec3f, vec3f> eval_element_tangents(
    const yocto_shape& shape, int element, const vec2f& uv = zero2f);
pair<mat3f, bool> eval_element_tangent_basis(
    const yocto_shape& shape, int element, const vec2f& uv = zero2f);

// Sample a shape element based on area/length.
pair<int, vec2f> sample_shape(const yocto_shape& shape,
    const vector<float>& cdf, float re, const vec2f& ruv);
vector<float>    sample_shape_cdf(const yocto_shape& shape);
void             sample_shape_cdf(const yocto_shape& shape, vector<float>& cdf);
float sample_shape_pdf(const yocto_shape& shape, const vector<float>& cdf,
    int element, const vec2f& uv);

// Evaluate a texture.
vec2i texture_size(const yocto_texture& texture);
vec4f lookup_texture(
    const yocto_texture& texture, int i, int j, bool ldr_as_linear = false);
vec4f eval_texture(const yocto_texture& texture, const vec2f& texcoord,
    bool ldr_as_linear = false, bool no_interpolation = false,
    bool clamp_to_edge = false);
float lookup_voltexture(
    const yocto_voltexture& texture, int i, int j, int k, bool ldr_as_linear);
float eval_voltexture(const yocto_voltexture& texture, const vec3f& texcoord,
    bool ldr_as_linear = false, bool no_interpolation = false,
    bool clamp_to_edge = false);

// Set and evaluate camera parameters. Setters take zeros as default values.
vec2f camera_fov(const yocto_camera& camera);
float camera_yfov(const yocto_camera& camera);
float camera_aspect(const yocto_camera& camera);
vec2i camera_resolution(const yocto_camera& camera, int resolution);
void  set_yperspective(yocto_camera& camera, float yfov, float aspect,
     float focus, float film = 0.036f);
// Sets camera field of view to enclose all the bbox. Camera view direction
// fiom size and forcal lemgth can be overridden if we pass non zero values.
void set_view(yocto_camera& camera, const bbox3f& bbox,
    const vec3f& view_direction = zero3f);

// Generates a ray from the image coordinates `uv` and lens coordinates `luv`.
ray3f eval_camera(
    const yocto_camera& camera, const vec2f& uv, const vec2f& luv);
// Generates a ray from a camera for pixel `ij`, the image size `resolution`,
// the sub-pixel coordinates `puv` and the lens coordinates `luv`.
ray3f eval_camera(const yocto_camera& camera, const vec2i& ij,
    const vec2i& resolution, const vec2f& puv, const vec2f& luv);

// Material values packed into a convenience structure.
struct material_point {
  vec3f emission      = {0, 0, 0};
  vec3f diffuse       = {0, 0, 0};
  vec3f specular      = {0, 0, 0};
  vec3f coat          = {0, 0, 0};
  vec3f transmission  = {0, 0, 0};
  float roughness     = 0;
  vec3f voldensity    = {0, 0, 0};
  vec3f volemission   = {0, 0, 0};
  vec3f volscatter    = {0, 0, 0};
  float volanisotropy = 0;
  float opacity       = 1;
  bool  thin          = false;
};
material_point eval_material(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texcoord,
    const vec4f& shape_color);

// Instance values interpolated using barycentric coordinates.
// Handles defaults if data is missing.
vec3f eval_position(const yocto_scene& scene, const yocto_instance& instance,
    int element, const vec2f& uv);
vec3f eval_normal(const yocto_scene& scene, const yocto_instance& instance,
    int element, const vec2f& uv, bool non_rigid_frame = false);
vec3f eval_shading_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element, const vec2f& uv,
    const vec3f& direction, bool non_rigid_frame = false);
vec3f eval_element_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element, bool non_rigid_frame = false);
material_point eval_material(const yocto_scene& scene,
    const yocto_instance& instance, int element, const vec2f& uv);

// Environment texture coordinates from the incoming direction.
vec2f eval_texcoord(
    const yocto_environment& environment, const vec3f& direction);
// Evaluate the incoming direction from the uv.
vec3f eval_direction(
    const yocto_environment& environment, const vec2f& environment_uv);
// Evaluate the environment emission.
vec3f eval_environment(const yocto_scene& scene,
    const yocto_environment& environment, const vec3f& direction);
// Evaluate all environment emission.
vec3f eval_environment(const yocto_scene& scene, const vec3f& direction);

// Sample an environment based on either texel values of uniform
vec3f         sample_environment(const yocto_scene& scene,
            const yocto_environment& environment, const vector<float>& texels_cdf,
            float re, const vec2f& ruv);
vector<float> sample_environment_cdf(
    const yocto_scene& scene, const yocto_environment& environment);
void  sample_environment_cdf(const yocto_scene& scene,
     const yocto_environment& environment, vector<float>& texels_cdf);
float sample_environment_pdf(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    const vec3f& direction);

}  // namespace yocto

// -----------------------------------------------------------------------------
// ANIMATION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Find the first keyframe value that is greater than the argument.
inline int keyframe_index(const vector<float>& times, const float& time);

// Evaluates a keyframed value using step interpolation.
template <typename T>
inline T keyframe_step(
    const vector<float>& times, const vector<T>& vals, float time);

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline vec4f keyframe_slerp(
    const vector<float>& times, const vector<vec4f>& vals, float time);

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline T keyframe_linear(
    const vector<float>& times, const vector<T>& vals, float time);

// Evaluates a keyframed value using Bezier interpolation.
template <typename T>
inline T keyframe_bezier(
    const vector<float>& times, const vector<T>& vals, float time);

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF ANIMATION UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Find the first keyframe value that is greater than the argument.
inline int keyframe_index(const vector<float>& times, const float& time) {
  for (auto i = 0; i < times.size(); i++)
    if (times[i] > time) return i;
  return (int)times.size();
}

// Evaluates a keyframed value using step interpolation.
template <typename T>
inline T keyframe_step(
    const vector<float>& times, const vector<T>& vals, float time) {
  if (time <= times.front()) return vals.front();
  if (time >= times.back()) return vals.back();
  time     = clamp(time, times.front(), times.back() - 0.001f);
  auto idx = keyframe_index(times, time);
  return vals.at(idx - 1);
}

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline vec4f keyframe_slerp(
    const vector<float>& times, const vector<vec4f>& vals, float time) {
  if (time <= times.front()) return vals.front();
  if (time >= times.back()) return vals.back();
  time     = clamp(time, times.front(), times.back() - 0.001f);
  auto idx = keyframe_index(times, time);
  auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
  return slerp(vals.at(idx - 1), vals.at(idx), t);
}

// Evaluates a keyframed value using linear interpolation.
template <typename T>
inline T keyframe_linear(
    const vector<float>& times, const vector<T>& vals, float time) {
  if (time <= times.front()) return vals.front();
  if (time >= times.back()) return vals.back();
  time     = clamp(time, times.front(), times.back() - 0.001f);
  auto idx = keyframe_index(times, time);
  auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
  return vals.at(idx - 1) * (1 - t) + vals.at(idx) * t;
}

// Evaluates a keyframed value using Bezier interpolation.
template <typename T>
inline T keyframe_bezier(
    const vector<float>& times, const vector<T>& vals, float time) {
  if (time <= times.front()) return vals.front();
  if (time >= times.back()) return vals.back();
  time     = clamp(time, times.front(), times.back() - 0.001f);
  auto idx = keyframe_index(times, time);
  auto t   = (time - times.at(idx - 1)) / (times.at(idx) - times.at(idx - 1));
  return interpolate_bezier(
      vals.at(idx - 3), vals.at(idx - 2), vals.at(idx - 1), vals.at(idx), t);
}

}  // namespace yocto

#endif
