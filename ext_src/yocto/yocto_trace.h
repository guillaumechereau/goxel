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
// 1. prepare the ray-tracing acceleration structure with `build_scene_bvh()`
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
#include "yocto_utils.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

using std::function;

}

// -----------------------------------------------------------------------------
// PATH TRACING
// -----------------------------------------------------------------------------
namespace yocto {

// Default trace seed
const auto trace_default_seed = 961748941ull;

// Trace lights used during rendering.
struct trace_lights {
    vector<int>           instances               = {};
    vector<int>           environments            = {};
    vector<vector<float>> shape_elements_cdf      = {};
    vector<vector<float>> environment_texture_cdf = {};

    bool empty() const { return instances.empty() && environments.empty(); }
};

// Initialize lights.
void init_trace_lights(trace_lights& lights, const yocto_scene& scene);

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
void init_trace_state(trace_state& state, const vec2i& image_size,
    uint64_t random_seed = trace_default_seed);

// Type of tracing algorithm to use
enum struct trace_sampler_type {
    path,                // path tracing
    naive,               // naive path tracing
    split,               // path tracing with split heuristic
    eyelight,            // eyelight rendering
    debug_normal,        // debug - normal
    debug_albedo,        // debug - albedo
    debug_texcoord,      // debug - texcoord
    debug_color,         // debug - color
    debug_frontfacing,   // debug - faceforward
    debug_emission,      // debug - emission
    debug_diffuse,       // debug - diffuse
    debug_specular,      // debug - specular
    debug_transmission,  // debug - transmission
    debug_roughness,     // debug - roughness
    debug_highlight,     // debug - highlight
};

const auto trace_sampler_type_names = vector<string>{"path", "naive", "split",
    "eyelight", "debug_normal", "debug_albedo", "debug_texcoord", "debug_color",
    "debug_frontfacing", "debug_emission", "debug_diffuse", "debug_specular",
    "debug_transmission", "debug_roughness", "debug_highlight"};

// Tracer function
using trace_sampler_func = function<pair<vec3f, bool>(const yocto_scene& scene,
    const bvh_scene& bvh, const trace_lights& lights, const vec3f& position,
    const vec3f& direction, rng_state& rng, int max_bounces,
    bool environments_hidden)>;
trace_sampler_func get_trace_sampler_func(trace_sampler_type type);

// Options for trace functions
struct trace_image_options {
    int                camera_id           = 0;
    vec2i              image_size          = {1280, 720};
    trace_sampler_type sampler_type        = trace_sampler_type::path;
    trace_sampler_func custom_sampler      = {};
    int                num_samples         = 512;
    int                max_bounces         = 8;
    int                samples_per_batch   = 16;
    int                region_size         = 16;
    float              pixel_clamp         = 10;
    bool               environments_hidden = false;
    bool               double_sided        = false;
    uint64_t           random_seed         = trace_default_seed;
    std::atomic<bool>* cancel_flag         = nullptr;
    bool               run_serially        = false;
};

// Progressively compute an image by calling trace_samples multiple times.
image4f trace_image(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const trace_image_options& options);

// Progressively compute an image by calling trace_samples multiple times.
// Start with an empty state and then successively call this function to
// render the next batch of samples.
int trace_image_samples(image4f& image, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    int current_sample, const trace_image_options& options);

// Starts an anyncrhounous renderer. The function will keep a reference to
// options.
void trace_image_async_start(image4f& image, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    vector<future<void>>& futures, atomic<int>& current_sample,
    concurrent_queue<image_region>& queue, const trace_image_options& options);
// Stop the asynchronous renderer.
void trace_image_async_stop(vector<future<void>>& futures,
    concurrent_queue<image_region>& queue, const trace_image_options& options);

// Check is a sampler requires lights
bool is_trace_sampler_lit(const trace_image_options& options);

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
float convert_specular_exponent_to_roughness(float n);

// Specular to fresnel eta.
void compute_fresnel_from_specular(
    const vec3f& specular, vec3f& es, vec3f& esk);
float convert_specular_to_eta(const vec3f& specular);
// Compute the fresnel term for dielectrics.
vec3f evaluate_fresnel_dielectric(float direction_cosine, const vec3f& eta);
// Compute the fresnel term for metals.
vec3f evaluate_fresnel_metal(
    float direction_cosine, const vec3f& eta, const vec3f& etak);
// Schlick approximation of Fresnel term, optionally weighted by roughness;
vec3f evaluate_fresnel_schlick(const vec3f& specular, float direction_cosine);
vec3f evaluate_fresnel_schlick(
    const vec3f& specular, float direction_cosine, float roughness);

// Evaluates the microfacet distribution and geometric term (ggx or beckman).
float evaluate_microfacet_distribution(float roughness, const vec3f& normal,
    const vec3f& half_vector, bool ggx = true);
float evaluate_microfacet_shadowing(float roughness, const vec3f& normal,
    const vec3f& half_vector, const vec3f& outgoing, const vec3f& incoming,
    bool ggx = true);
vec3f sample_microfacet_distribution(
    float roughness, const vec3f& normal, const vec2f& rn, bool ggx = true);
float sample_microfacet_distribution_pdf(float roughness, const vec3f& normal,
    const vec3f& half_vector, bool ggx = true);

// Evaluate and sample volume phase function.
vec3f sample_phase_function(float vg, const vec2f& u);
float evaluate_phase_function(float cos_theta, float vg);

// Get a complex ior table with keys the metal name and values (eta, etak)
bool get_metal_ior(const string& element, vec3f& eta, vec3f& etak);

}  // namespace yocto

#endif
