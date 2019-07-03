//
// # Yocto/Trace: Tiny path tracer
//
//
// Yocto/Trace is a simple path tracing library with support for microfacet
// materials, area and environment lights, and advacned sampling.
//
//
// ## Physically-based Path Tracing
//
// Yocto/Trace includes a tiny, but fully featured, path tracer with support for
// textured mesh area lights, GGX materials, environment mapping. The algorithm
// makes heavy use of MIS for fast convergence.
// The interface supports progressive parallel execution both synchronously,
// for CLI applications, and asynchronously for interactive viewing.
//
// Materials are represented as sums of an emission term, a diffuse term and
// a specular microfacet term (GGX or Phong), and a transmission term for
// this sheet glass.
// Lights are defined as any shape with a material emission term. Additionally
// one can also add environment maps. But even if you can, you might want to
// add a large triangle mesh with inward normals instead. The latter is more
// general (you can even more an arbitrary shape sun). For now only the first
// environment is used.
//
// 1. prepare the ray-tracing acceleration structure with `build_bvh()`
// 2. prepare lights for rendering with `init_trace_lights()`
// 3. create the random number generators with `init_trace_state()`
// 4. render blocks of samples with `trace_samples()`
// 5. you can also start an asynchronous renderer with `trace_asynch_start()`
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

#ifndef _YOCTO_TRACE_H_
#define _YOCTO_TRACE_H_

#ifndef YOCTO_QUADS_AS_TRIANGLES
#define YOCTO_QUADS_AS_TRIANGLES 1
#endif

#ifndef YOCTO_TRACE_THINSHEET
#define YOCTO_TRACE_THINSHEET 0
#endif

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_bvh.h"
#include "yocto_math.h"
#include "yocto_random.h"
#include "yocto_scene.h"

#include <atomic>

// -----------------------------------------------------------------------------
// PATH TRACING
// -----------------------------------------------------------------------------
namespace yocto {

// Default trace seed
const auto trace_default_seed = 961748941ull;

// Trace lights used during rendering.
struct trace_lights {
  vector<int>           instances        = {};
  vector<int>           environments     = {};
  vector<vector<float>> shape_cdfs       = {};
  vector<vector<float>> environment_cdfs = {};

  bool empty() const { return instances.empty() && environments.empty(); }
};

// Initialize lights.
trace_lights make_trace_lights(const yocto_scene& scene);
void         make_trace_lights(trace_lights& lights, const yocto_scene& scene);

// State of a pixel during tracing
struct trace_pixel {
  vec3f     radiance = zero3f;
  int       hits     = 0;
  int       samples  = 0;
  rng_state rng      = {};
};
struct trace_state {
  vec2i               image_size = {0, 0};
  vector<trace_pixel> pixels     = {};
};

// Initialize state of the renderer.
trace_state make_trace_state(
    const vec2i& image_size, uint64_t random_seed = trace_default_seed);
void make_trace_state(trace_state& state, const vec2i& image_size,
    uint64_t random_seed = trace_default_seed);

// Options for trace functions
struct trace_params {
  // clang-format off
  // Type of tracing algorithm to use
  enum struct sampler_type {
    path,        // path tracing
    naive,       // naive path tracing
    eyelight,    // eyelight rendering
    falsecolor,  // false color rendering
  };
  enum struct falsecolor_type {
    normal, frontfacing, gnormal, gfrontfacing, texcoord, color, emission,    
    diffuse, specular, transmission, roughness, material, shape, instance, 
    element, highlight };
  // clang-format on

  int                camera     = 0;
  int                resolution = 1280;
  sampler_type       sampler    = sampler_type::path;
  falsecolor_type    falsecolor = falsecolor_type::diffuse;
  int                samples    = 512;
  int                bounces    = 8;
  int                batch      = 16;
  int                region     = 16;
  float              clamp      = 10;
  bool               envhidden  = false;
  bool               tentfilter = false;
  uint64_t           seed       = trace_default_seed;
  std::atomic<bool>* cancel     = nullptr;
  bool               noparallel = false;
};

const auto trace_sampler_names = vector<string>{
    "path", "naive", "eyelight", "falsecolor"};

const auto trace_falsecolor_names = vector<string>{"normal", "frontfacing",
    "gnormal", "gfrontfacing", "texcoord", "color", "emission", "diffuse",
    "specular", "transmission", "roughness", "material", "shape", "instance",
    "element", "highlight"};

// Equality operators
inline bool operator==(const trace_params& a, const trace_params& b) {
  return memcmp(&a, &b, sizeof(a)) == 0;
}
inline bool operator!=(const trace_params& a, const trace_params& b) {
  return memcmp(&a, &b, sizeof(a)) != 0;
}

// Progressively compute an image by calling trace_samples multiple times.
image<vec4f> trace_image(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const trace_params& params);

// Progressively compute an image by calling trace_samples multiple times.
// Start with an empty state and then successively call this function to
// render the next batch of samples.
int trace_samples(image<vec4f>& image, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    int current_sample, const trace_params& params);

// Progressively compute an image by calling trace_region multiple times.
// Compared to `trace_samples` this always runs serially and is helpful
// when building async applications.
void trace_region(image<vec4f>& image, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    const image_region& region, int num_samples, const trace_params& params);

// Check is a sampler requires lights
bool is_sampler_lit(const trace_params& params);

// Trace statistics for last run used for fine tuning implementation.
// For now returns number of paths and number of rays.
pair<uint64_t, uint64_t> get_trace_stats();
void                     reset_trace_stats();

}  // namespace yocto

// -----------------------------------------------------------------------------
// PATH TRACING SUPPORT FUNCTION
// -----------------------------------------------------------------------------
namespace yocto {

// Phong exponent to roughness.
float exponent_to_roughness(float n);

// Specular to fresnel eta.
vec3f              reflectivity_to_eta(const vec3f& reflectivity);
vec3f              eta_to_reflectivity(const vec3f& eta);
pair<vec3f, vec3f> reflectivity_to_eta(
    const vec3f& reflectivity, const vec3f& edge_tint);
vec3f eta_to_reflectivity(const vec3f& eta, const vec3f& etak);
vec3f eta_to_edge_tint(const vec3f& eta, const vec3f& etak);
// Compute the fresnel term for dielectrics.
vec3f fresnel_dielectric(const vec3f& eta, float direction_cosine);
// Compute the fresnel term for metals.
vec3f fresnel_conductor(
    const vec3f& eta, const vec3f& etak, float direction_cosine);
// Schlick approximation of Fresnel term, optionally weighted by roughness;
vec3f fresnel_schlick(const vec3f& specular, float direction_cosine);
vec3f fresnel_schlick(
    const vec3f& specular, float direction_cosine, float roughness);

// Evaluates the microfacet distribution and geometric term (ggx or beckman).
float eval_microfacetD(float roughness, const vec3f& normal,
    const vec3f& half_vector, bool ggx = true);
float eval_microfacetG(float roughness, const vec3f& normal,
    const vec3f& half_vector, const vec3f& outgoing, const vec3f& incoming,
    bool ggx = true);
vec3f sample_microfacet(
    float roughness, const vec3f& normal, const vec2f& rn, bool ggx = true);
float sample_microfacet_pdf(float roughness, const vec3f& normal,
    const vec3f& half_vector, bool ggx = true);

// Evaluate and sample volume phase function.
vec3f sample_phasefunction(float vg, const vec2f& u);
float eval_phasefunction(float cos_theta, float vg);

// Get complex ior from metal names (eta, etak).
// Return zeros if not available.
pair<vec3f, vec3f> get_conductor_eta(const string& element);

// Get subsurface params
pair<vec3f, vec3f> get_subsurface_params(const string& name);

}  // namespace yocto

#endif
