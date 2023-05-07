//
// # Yocto/Scene: Scene reepresentation
//
// Yocto/Scene defines a simple scene representation, and related utilities,
// mostly geared towards scene creation and serialization. Scene serialization
// is implemented in Yocto/SceneIO.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2022 Fabio Pellacini
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

#ifndef _YOCTO_SCENE_H_
#define _YOCTO_SCENE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "yocto_geometry.h"
#include "yocto_image.h"
#include "yocto_math.h"
#include "yocto_shape.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::pair;
using std::string;
using std::vector;

}  // namespace yocto

// -----------------------------------------------------------------------------
// SCENE DATA
// -----------------------------------------------------------------------------
namespace yocto {

// Handles to refer to scene elements
inline const int invalidid = -1;

// Camera based on a simple lens model. The camera is placed using a frame.
// Camera projection is described in photographic terms. In particular,
// we specify film size (35mm by default), film aspect ration,
// the lens' focal length, the focus distance and the lens aperture.
// All values are in meters. Here are some common aspect ratios used in video
// and still photography.
// 3:2    on 35 mm:  0.036 x 0.024
// 16:9   on 35 mm:  0.036 x 0.02025 or 0.04267 x 0.024
// 2.35:1 on 35 mm:  0.036 x 0.01532 or 0.05640 x 0.024
// 2.39:1 on 35 mm:  0.036 x 0.01506 or 0.05736 x 0.024
// 2.4:1  on 35 mm:  0.036 x 0.015   or 0.05760 x 0.024 (approx. 2.39 : 1)
// To compute good apertures, one can use the F-stop number from photography
// and set the aperture to focal length over f-stop.
struct camera_data {
  frame3f frame        = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
  bool    orthographic = false;
  float   lens         = 0.050f;
  float   film         = 0.036f;
  float   aspect       = 1.500f;
  float   focus        = 10000;
  float   aperture     = 0;
};

// Texture data as array of float or byte pixels. Textures can be stored in
// linear or non linear color space.
struct texture_data {
  int           width   = 0;
  int           height  = 0;
  bool          linear  = false;
  bool          nearest = false;
  bool          clamp   = false;
  vector<vec4f> pixelsf = {};
  vector<vec4b> pixelsb = {};
};

// Material type
enum struct material_type {
  // clang-format off
  matte, glossy, reflective, transparent, refractive, subsurface, volumetric, 
  gltfpbr
  // clang-format on
};

// Enum labels
inline const auto material_type_names = std::vector<std::string>{"matte",
    "glossy", "reflective", "transparent", "refractive", "subsurface",
    "volumetric", "gltfpbr"};

// Material for surfaces, lines and triangles.
// For surfaces, uses a microfacet model with thin sheet transmission.
// The model is based on OBJ, but contains glTF compatibility.
// For the documentation on the values, please see the OBJ format.
struct material_data {
  // material
  material_type type         = material_type::matte;
  vec3f         emission     = {0, 0, 0};
  vec3f         color        = {0, 0, 0};
  float         roughness    = 0;
  float         metallic     = 0;
  float         ior          = 1.5f;
  vec3f         scattering   = {0, 0, 0};
  float         scanisotropy = 0;
  float         trdepth      = 0.01f;
  float         opacity      = 1;

  // textures
  int emission_tex   = invalidid;
  int color_tex      = invalidid;
  int roughness_tex  = invalidid;
  int scattering_tex = invalidid;
  int normal_tex     = invalidid;
};

// Instance.
struct instance_data {
  // instance data
  frame3f frame    = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
  int     shape    = invalidid;
  int     material = invalidid;
};

// Environment map.
struct environment_data {
  // environment data
  frame3f frame        = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
  vec3f   emission     = {0, 0, 0};
  int     emission_tex = invalidid;
};

// Subdiv data represented as face-varying primitives where
// each vertex data has its own topology.
struct subdiv_data {
  // face-varying primitives
  vector<vec4i> quadspos      = {};
  vector<vec4i> quadsnorm     = {};
  vector<vec4i> quadstexcoord = {};

  // vertex data
  vector<vec3f> positions = {};
  vector<vec3f> normals   = {};
  vector<vec2f> texcoords = {};

  // subdivision data
  int  subdivisions = 0;
  bool catmullclark = true;
  bool smooth       = true;

  // displacement data
  float displacement     = 0;
  int   displacement_tex = invalidid;

  // shape reference
  int shape = invalidid;
};

// Scene comprised an array of objects whose memory is owened by the scene.
// All members are optional,Scene objects (camera, instances, environments)
// have transforms defined internally. A scene can optionally contain a
// node hierarchy where each node might point to a camera, instance or
// environment. In that case, the element transforms are computed from
// the hierarchy. Animation is also optional, with keyframe data that
// updates node transformations only if defined.
struct scene_data {
  // scene elements
  vector<camera_data>      cameras      = {};
  vector<instance_data>    instances    = {};
  vector<environment_data> environments = {};
  vector<shape_data>       shapes       = {};
  vector<texture_data>     textures     = {};
  vector<material_data>    materials    = {};
  vector<subdiv_data>      subdivs      = {};

  // names (this will be cleanup significantly later)
  vector<string> camera_names      = {};
  vector<string> texture_names     = {};
  vector<string> material_names    = {};
  vector<string> shape_names       = {};
  vector<string> instance_names    = {};
  vector<string> environment_names = {};
  vector<string> subdiv_names      = {};

  // copyright info preserve in IO
  string copyright = "";
};

}  // namespace yocto

// -----------------------------------------------------------------------------
// CAMERA PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Generates a ray from a camera.
ray3f eval_camera(
    const camera_data& camera, const vec2f& image_uv, const vec2f& lens_uv);

}  // namespace yocto

// -----------------------------------------------------------------------------
// TEXTURE PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Evaluates a texture
vec4f eval_texture(
    const texture_data& texture, const vec2f& uv, bool as_linear = false);
vec4f eval_texture(const scene_data& scene, int texture, const vec2f& uv,
    bool as_linear = false);
vec4f eval_texture(const texture_data& texture, const vec2f& uv, bool as_linear,
    bool no_interpolation, bool clamp_to_edge);
vec4f eval_texture(const scene_data& scene, int texture, const vec2f& uv,
    bool as_linear, bool no_interpolation, bool clamp_to_edge);

// pixel access
vec4f lookup_texture(
    const texture_data& texture, int i, int j, bool as_linear = false);

// conversion from image
texture_data image_to_texture(const image_data& image);

}  // namespace yocto

// -----------------------------------------------------------------------------
// MATERIAL PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Material parameters evaluated at a point on the surface
struct material_point {
  material_type type         = material_type::gltfpbr;
  vec3f         emission     = {0, 0, 0};
  vec3f         color        = {0, 0, 0};
  float         opacity      = 1;
  float         roughness    = 0;
  float         metallic     = 0;
  float         ior          = 1;
  vec3f         density      = {0, 0, 0};
  vec3f         scattering   = {0, 0, 0};
  float         scanisotropy = 0;
  float         trdepth      = 0.01f;
};

// Eval material to obtain emission, brdf and opacity.
material_point eval_material(const scene_data& scene,
    const material_data& material, const vec2f& texcoord,
    const vec4f& shape_color = {1, 1, 1, 1});

// check if a material is a delta
bool is_delta(const material_data& material);
bool is_delta(const material_point& material);

// check if a material has a volume
bool is_volumetric(const material_data& material);
bool is_volumetric(const material_point& material);
bool is_volumetric(const scene_data& scene, const instance_data& instance);

}  // namespace yocto

// -----------------------------------------------------------------------------
// INSTANCE PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Evaluate instance properties
vec3f eval_position(const scene_data& scene, const instance_data& instance,
    int element, const vec2f& uv);
vec3f eval_element_normal(
    const scene_data& scene, const instance_data& instance, int element);
vec3f eval_normal(const scene_data& scene, const instance_data& instance,
    int element, const vec2f& uv);
vec2f eval_texcoord(const scene_data& scene, const instance_data& instance,
    int element, const vec2f& uv);
pair<vec3f, vec3f> eval_element_tangents(
    const scene_data& scene, const instance_data& instance, int element);
vec3f eval_normalmap(const scene_data& scene, const instance_data& instance,
    int element, const vec2f& uv);
vec3f eval_shading_position(const scene_data& scene,
    const instance_data& instance, int element, const vec2f& uv,
    const vec3f& outgoing);
vec3f eval_shading_normal(const scene_data& scene,
    const instance_data& instance, int element, const vec2f& uv,
    const vec3f& outgoing);
vec4f eval_color(const scene_data& scene, const instance_data& instance,
    int element, const vec2f& uv);

// Eval material to obtain emission, brdf and opacity.
material_point eval_material(const scene_data& scene,
    const instance_data& instance, int element, const vec2f& uv);
// check if a material has a volume
bool is_volumetric(const scene_data& scene, const instance_data& instance);

}  // namespace yocto

// -----------------------------------------------------------------------------
// ENVIRONMENT PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Environment
vec3f eval_environment(const scene_data& scene,
    const environment_data& environment, const vec3f& direction);
vec3f eval_environment(const scene_data& scene, const vec3f& direction);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// compute scene bounds
bbox3f compute_bounds(const scene_data& scene);

// add missing elements
void add_camera(scene_data& scene);
void add_sky(scene_data& scene, float sun_angle = pif / 4);

// get named camera or default if name is empty
int find_camera(const scene_data& scene, const string& name);

// Check if lights are present
bool has_lights(const scene_data& scene);

// create a scene from a shape
scene_data make_shape_scene(const shape_data& shape, bool add_sky = false);

// Return scene statistics as list of strings.
vector<string> scene_stats(const scene_data& scene, bool verbose = false);
// Return validation errors as list of strings.
vector<string> scene_validation(
    const scene_data& scene, bool notextures = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SCENE TESSELATION
// -----------------------------------------------------------------------------
namespace yocto {

// Apply subdivision and displacement rules.
void tesselate_subdivs(scene_data& scene);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE SCENES
// -----------------------------------------------------------------------------
namespace yocto {

// Make Cornell Box scene
scene_data make_cornellbox();

}  // namespace yocto

// -----------------------------------------------------------------------------
// BACKWARDS COMPATIBILITY
// -----------------------------------------------------------------------------
namespace yocto {

using sceneio_scene [[deprecated]]       = scene_data;
using scene_model [[deprecated]]         = scene_data;
using sceneio_camera [[deprecated]]      = camera_data;
using scene_camera [[deprecated]]        = camera_data;
using sceneio_texture [[deprecated]]     = texture_data;
using scene_texture [[deprecated]]       = texture_data;
using sceneio_material [[deprecated]]    = material_data;
using scene_material_type [[deprecated]] = material_type;
using sceneio_material [[deprecated]]    = material_data;
using sceneio_shape [[deprecated]]       = shape_data;
using scene_shape [[deprecated]]         = shape_data;
using scene_fvshape [[deprecated]]       = fvshape_data;
using sceneio_instance [[deprecated]]    = instance_data;
using scene_instance [[deprecated]]      = instance_data;
using sceneio_environment [[deprecated]] = environment_data;
using scene_environment [[deprecated]]   = environment_data;
using scene_subdiv [[deprecated]]        = subdiv_data;

}  // namespace yocto

#endif
