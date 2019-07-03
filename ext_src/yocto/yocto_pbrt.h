//
// # Yocto/Pbrt: Tiny library for Pbrt parsing
//
// Yocto/Pbrt is a simple pbrt parser that works with callbacks.
// We make no attempt to provide a simple interface for pbrt but just the
// low level parsing code.
//
// Error reporting is done through exceptions using the `io_error`
// exception.
//
// ## Parse an pbrt file
//
// 1. define callbacks in `pbrt_callback` structure using lambda with capture
//    if desired
// 2. run the parse with `load_pbrt()`
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

#ifndef _YOCTO_PBRT_H_
#define _YOCTO_PBRT_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"

// -----------------------------------------------------------------------------
// SIMPLE PBRT LOADER
// -----------------------------------------------------------------------------
namespace yocto {

// pbrt pbrt_spectrum as rgb color
struct pbrt_spectrum3f {
  float x = 0;
  float y = 0;
  float z = 0;

  pbrt_spectrum3f() : x{0}, y{0}, z{0} {}
  pbrt_spectrum3f(float x, float y, float z) : x{x}, y{y}, z{z} {}
  explicit pbrt_spectrum3f(float v) : x{v}, y{v}, z{v} {}
  explicit operator vec3f() const { return {x, y, z}; };

  float&       operator[](int i) { return (&x)[i]; }
  const float& operator[](int i) const { return (&x)[i]; }
};

// pbrt cameras
struct pbrt_camera {
  struct perspective_t {
    float fov              = 90;
    float frameaspectratio = -1;  // or computed from film
    float lensradius       = 0;
    float focaldistance    = 1e30;
    vec4f screenwindow     = {-1, 1, -1, 1};
    float shutteropen      = 0;
    float shutterclose     = 1;
  };
  struct orthographic_t {
    float frameaspectratio = -1;  // or computed from film
    float lensradius       = 0;
    float focaldistance    = 1e30;
    vec4f screenwindow     = {-1, 1, -1, 1};
    float shutteropen      = 0;
    float shutterclose     = 1;
  };
  struct environment_t {
    float shutteropen  = 0;
    float shutterclose = 1;
  };
  struct realistic_t {
    string lensfile           = "";
    float  aperturediameter   = 1;
    float  focusdistance      = 10;
    bool   simpleweighting    = true;
    float  shutteropen        = 0;
    float  shutterclose       = 1;
    float  approx_focallength = 0;
  };
  enum struct type_t { perspective, orthographic, environment, realistic };
  type_t         type         = type_t::perspective;
  perspective_t  perspective  = {};
  orthographic_t orthographic = {};
  environment_t  environment  = {};
  realistic_t    realistic    = {};
};

// pbrt samplers
struct pbrt_sampler {
  struct random_t {
    int pixelsamples = 16;
  };
  struct halton_t {
    int pixelsamples = 16;
  };
  struct sobol_t {
    int pixelsamples = 16;
  };
  struct zerotwosequence_t {
    int pixelsamples = 16;
  };
  struct maxmindist_t {
    int pixelsamples = 16;
  };
  struct stratified_t {
    bool jitter   = true;
    int  xsamples = 2;
    int  ysamples = 2;
  };
  enum struct type_t {
    random,
    halton,
    sobol,
    zerotwosequence,
    maxmindist,
    stratified
  };
  type_t            type            = type_t::random;
  random_t          random          = {};
  halton_t          halton          = {};
  sobol_t           sobol           = {};
  zerotwosequence_t zerotwosequence = {};
  maxmindist_t      maxmindist      = {};
  stratified_t      stratified      = {};
};

// pbrt film
struct pbrt_film {
  struct image_t {
    int    xresolution        = 640;
    int    yresolution        = 480;
    vec4f  cropwindow         = {0, 1, 0, 1};
    float  scale              = 1;
    float  maxsampleluminance = flt_max;
    float  diagonal           = 35;
    string filename           = "pbrt.exr";
  };
  enum struct type_t { image };
  type_t  type  = type_t::image;
  image_t image = {};
};

// pbrt filters
struct pbrt_filter {
  struct box_t {
    float xwidth = 0.5;
    float ywidth = 0.5;
  };
  struct gaussian_t {
    float xwidth = 2;
    float ywidth = 2;
    float alpha  = 2;
  };
  struct mitchell_t {
    float xwidth = 2;
    float ywidth = 2;
    float B      = 1.0f / 3.0f;
    float C      = 1.0f / 3.0f;
  };
  struct sinc_t {
    float xwidth = 4;
    float ywidth = 4;
    float tau    = 3;
  };
  struct triangle_t {
    float xwidth = 2;
    float ywidth = 2;
  };
  enum struct type_t { box, gaussian, mitchell, sinc, triangle };
  type_t     type     = type_t::box;
  box_t      box      = {};
  gaussian_t gaussian = {};
  mitchell_t mitchell = {};
  sinc_t     sinc     = {};
  triangle_t triangle = {};
};

// pbrt integrators
struct pbrt_integrator {
  struct path_t {
    enum struct lightsamplestrategy_t { uniform, power, spatial };
    int                   maxdepth            = 5;
    vec4i                 pixelbounds         = {0, 0, int_max, int_max};
    float                 rrthreshold         = 1;
    lightsamplestrategy_t lightsamplestrategy = lightsamplestrategy_t::spatial;
  };
  struct volpath_t {
    enum struct lightsamplestrategy_t { uniform, power, spatial };
    int                   maxdepth            = 5;
    vec4i                 pixelbounds         = {0, 0, int_max, int_max};
    float                 rrthreshold         = 1;
    lightsamplestrategy_t lightsamplestrategy = lightsamplestrategy_t::spatial;
  };
  struct bdpt_t {
    enum struct lightsamplestrategy_t { uniform, power, spatial };
    int                   maxdepth            = 5;
    vec4i                 pixelbounds         = {0, 0, int_max, int_max};
    lightsamplestrategy_t lightsamplestrategy = lightsamplestrategy_t::power;
    bool                  visualizestrategies = false;
    bool                  visualizeweights    = false;
  };
  struct directlighting_t {
    enum struct strategy_t { all, one };
    strategy_t strategy    = strategy_t::all;
    int        maxdepth    = 5;
    vec4i      pixelbounds = {0, 0, int_max, int_max};
  };
  struct mlt_t {
    int   maxdepth             = 5;
    vec4i pixelbounds          = {0, 0, int_max, int_max};
    int   bootstrapsamples     = 100000;
    int   chains               = 1000;
    int   mutationsperpixel    = 100;
    float largestepprobability = 0.3;
    float sigma                = 0.01;
  };
  struct sppm_t {
    int   maxdepth            = 5;
    vec4i pixelbounds         = {0, 0, int_max, int_max};
    int   iterations          = 64;
    int   photonsperiteration = -1;
    int   imagewritefrequency = pow2(31);
    float radius              = 5;
  };
  struct whitted_t {
    int   maxdepth    = 5;
    vec4i pixelbounds = {0, 0, int_max, int_max};
  };
  enum struct type_t {
    path,
    volpath,
    bdpt,
    directlighting,
    mlt,
    sppm,
    whitted
  };
  type_t           type           = type_t::path;
  path_t           path           = {};
  volpath_t        volpath        = {};
  bdpt_t           bdpt           = {};
  directlighting_t directlighting = {};
  mlt_t            mlt            = {};
  sppm_t           sppm           = {};
  whitted_t        whitted        = {};
};

// pbrt accellerators
struct pbrt_accelerator {
  struct bvh_t {
    enum struct splitmethod_t { sah, equal, middle, hlbvh };
    int           maxnodeprims = 4;
    splitmethod_t splitmethod  = splitmethod_t::sah;
  };
  struct kdtree_t {
    int   intersectcost = 80;
    int   traversalcost = 1;
    float emptybonus    = 0.2;
    int   maxprims      = 1;
    int   maxdepth      = -1;
  };
  enum struct type_t { bvh, kdtree };
  type_t   type   = type_t::bvh;
  bvh_t    bvh    = {};
  kdtree_t kdtree = {};
};

// pbrt texture or value
struct pbrt_textured1f {
  float  value   = 0;
  string texture = "";
  pbrt_textured1f() : value{0}, texture{} {}
  pbrt_textured1f(float v) : value{v}, texture{} {}
};
struct pbrt_textured3f {
  pbrt_spectrum3f value   = {0, 0, 0};
  string          texture = "";
  pbrt_textured3f() : value{0, 0, 0}, texture{} {}
  pbrt_textured3f(float x, float y, float z) : value{x, y, z}, texture{} {}
};

// pbrt textures
struct pbrt_texture {
  struct constant_t {
    pbrt_textured3f value = {1, 1, 1};
  };
  struct bilerp_t {
    pbrt_textured3f v00 = {0, 0, 0};
    pbrt_textured3f v01 = {1, 1, 1};
    pbrt_textured3f v10 = {0, 0, 0};
    pbrt_textured3f v11 = {1, 1, 1};
    enum struct mapping_type { uv, spherical, cylindrical, planar };
    mapping_type mapping = mapping_type::uv;
    float        uscale  = 1;
    float        vscale  = 1;
    float        udelta  = 0;
    float        vdelta  = 0;
    vec3f        v1      = {1, 0, 0};
    vec3f        v2      = {0, 1, 0};
  };
  struct checkerboard_t {
    enum struct aamode_type { closedform, none };
    int             dimension = 2;
    pbrt_textured3f tex1      = {1, 1, 1};
    pbrt_textured3f tex2      = {0, 0, 0};
    aamode_type     aamode    = aamode_type::closedform;
    enum struct mapping_type { uv, spherical, cylindrical, planar };
    mapping_type mapping = mapping_type::uv;
    float        uscale  = 1;
    float        vscale  = 1;
    float        udelta  = 0;
    float        vdelta  = 0;
    vec3f        v1      = {1, 0, 0};
    vec3f        v2      = {0, 1, 0};
  };
  struct dots_t {
    pbrt_textured3f inside  = {1, 1, 1};
    pbrt_textured3f outside = {0, 0, 0};
    enum struct mapping_type { uv, spherical, cylindrical, planar };
    mapping_type mapping = mapping_type::uv;
    float        uscale  = 1;
    float        vscale  = 1;
    float        udelta  = 0;
    float        vdelta  = 0;
    vec3f        v1      = {1, 0, 0};
    vec3f        v2      = {0, 1, 0};
  };
  struct fbm_t {
    int   octaves   = 8;
    float roughness = 0.5;
  };
  struct imagemap_t {
    enum wrap_type { repeat, black, clamp };
    string    filename      = "";
    wrap_type wrap          = wrap_type::repeat;
    float     maxanisotropy = 8;
    bool      trilinear     = false;
    float     scale         = 1;
    bool      gamma         = true;
    enum struct mapping_type { uv, spherical, cylindrical, planar };
    mapping_type mapping = mapping_type::uv;
    float        uscale  = 1;
    float        vscale  = 1;
    float        udelta  = 0;
    float        vdelta  = 0;
    vec3f        v1      = {1, 0, 0};
    vec3f        v2      = {0, 1, 0};
  };
  struct marble_t {
    int   octaves   = 8;
    float roughness = 0.5;
    float scale     = 1;
    float variation = 0.2;
  };
  struct mix_t {
    pbrt_textured3f tex1   = {1, 1, 1};
    pbrt_textured3f tex2   = {1, 1, 1};
    pbrt_textured1f amount = 0.5;
  };
  struct scale_t {
    pbrt_textured3f tex1 = {1, 1, 1};
    pbrt_textured3f tex2 = {1, 1, 1};
  };
  struct uv_t {
    enum struct mapping_type { uv, spherical, cylindrical, planar };
    mapping_type mapping = mapping_type::uv;
    float        uscale  = 1;
    float        vscale  = 1;
    float        udelta  = 0;
    float        vdelta  = 0;
    vec3f        v1      = {1, 0, 0};
    vec3f        v2      = {0, 1, 0};
  };
  struct windy_t {
    // TODO: missing parameters
  };
  struct wrinkled_t {
    int   octaves   = 8;
    float roughness = 0.5;
  };
  enum struct type_t {
    constant,
    bilerp,
    checkerboard,
    dots,
    fbm,
    imagemap,
    marble,
    mix,
    scale,
    uv,
    windy,
    wrinkled
  };
  type_t         type         = type_t::constant;
  constant_t     constant     = {};
  bilerp_t       bilerp       = {};
  checkerboard_t checkerboard = {};
  dots_t         dots         = {};
  fbm_t          fbm          = {};
  imagemap_t     imagemap     = {};
  marble_t       marble       = {};
  mix_t          mix          = {};
  scale_t        scale        = {};
  uv_t           uv           = {};
  windy_t        windy        = {};
  wrinkled_t     wrinkled     = {};
};

// pbrt materials
struct pbrt_material {
  struct matte_t {
    pbrt_textured3f Kd      = {0.5, 0.5, 0.5};
    pbrt_textured1f sigma   = 0;
    pbrt_textured1f bumpmap = 0;
  };
  struct mirror_t {
    pbrt_textured3f Kr      = {0.9, 0.9, 0.9};
    pbrt_textured1f bumpmap = 0;
  };
  struct plastic_t {
    pbrt_textured3f Kd             = {0.25, 0.25, 0.25};
    pbrt_textured3f Ks             = {0.25, 0.25, 0.25};
    pbrt_textured1f uroughness     = 0.1;
    pbrt_textured1f vroughness     = 0.1;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct metal_t {
    pbrt_textured3f eta        = {0.2004376970f, 0.9240334304f, 1.1022119527f};
    pbrt_textured3f k          = {3.9129485033f, 2.4528477015f, 2.1421879552f};
    pbrt_textured1f uroughness = 0.01;
    pbrt_textured1f vroughness = 0.01;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct glass_t {
    pbrt_textured3f Kr             = {1, 1, 1};
    pbrt_textured3f Kt             = {1, 1, 1};
    pbrt_textured1f eta            = 1.5;
    pbrt_textured1f uroughness     = 0;
    pbrt_textured1f vroughness     = 0;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct translucent_t {
    pbrt_textured3f Kd             = {0.25, 0.25, 0.25};
    pbrt_textured3f Ks             = {0.25, 0.25, 0.25};
    pbrt_textured3f reflect        = {0.5, 0.5, 0.5};
    pbrt_textured3f transmit       = {0.5, 0.5, 0.5};
    pbrt_textured1f uroughness     = 0.1;
    pbrt_textured1f vroughness     = 0.1;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct uber_t {
    pbrt_textured3f Kd             = {0.25, 0.25, 0.25};
    pbrt_textured3f Ks             = {0.25, 0.25, 0.25};
    pbrt_textured3f Kr             = {0, 0, 0};
    pbrt_textured3f Kt             = {0, 0, 0};
    pbrt_textured1f uroughness     = 0.1;
    pbrt_textured1f vroughness     = 0.1;
    pbrt_textured1f eta            = 1.5;
    pbrt_textured3f opacity        = {1, 1, 1};
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct disney_t {
    pbrt_textured3f color           = {0.5, 0.5, 0.5};
    pbrt_textured1f anisotropic     = 0;
    pbrt_textured1f clearcoat       = 0;
    pbrt_textured1f clearcoatgloss  = 1;
    pbrt_textured1f eta             = 1.5;
    pbrt_textured1f metallic        = 0;
    pbrt_textured1f uroughness      = 0.5;
    pbrt_textured1f vroughness      = 0.5;
    pbrt_textured3f scatterdistance = {0, 0, 0};
    pbrt_textured1f sheen           = 0;
    pbrt_textured1f sheentint       = 0.5;
    pbrt_textured1f spectrans       = 0;
    pbrt_textured1f speculartint    = 0;
    bool            thin            = false;
    pbrt_textured3f difftrans       = {1, 1, 1};
    pbrt_textured3f flatness        = {0, 0, 0};
    bool            remaproughness  = true;
    pbrt_textured1f bumpmap         = 0;
  };
  struct fourier_t {
    string          bsdffile = "";
    pbrt_textured1f bumpmap  = 0;
    enum struct approx_type_t { plastic, metal, glass };
    approx_type_t approx_type    = approx_type_t::plastic;
    plastic_t     approx_plastic = {};
    metal_t       approx_metal   = {};
    glass_t       approx_glass   = {};
  };
  struct hair_t {
    pbrt_textured3f color       = {0, 0, 0};  // TODO: missing default
    pbrt_textured3f sigma_a     = {0, 0, 0};  // TODO: missing default
    pbrt_textured1f eumelanin   = 0;          // TODO: missing default
    pbrt_textured1f pheomelanin = 0;          // TODO: missing default
    pbrt_textured1f eta         = 1.55f;
    pbrt_textured1f beta_m      = 0.3f;
    pbrt_textured1f beta_n      = 0.3f;
    pbrt_textured1f alpha       = 2;
    pbrt_textured1f bumpmap     = 0;
  };
  struct kdsubsurface_t {
    pbrt_textured3f Kd             = {0.5, 0.5, 0.5};
    pbrt_textured3f mfp            = {1, 1, 1};
    pbrt_textured1f eta            = 1.3;
    pbrt_textured3f Kr             = {1, 1, 1};
    pbrt_textured3f Kt             = {1, 1, 1};
    pbrt_textured1f uroughness     = 0;
    pbrt_textured1f vroughness     = 0;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct mix_t {
    pbrt_textured3f amount         = {0, 0, 0};
    string          namedmaterial1 = "";
    string          namedmaterial2 = "";
    pbrt_textured1f bumpmap        = 0;
  };
  struct substrate_t {
    pbrt_textured3f Kd             = {0.5, 0.5, 0.5};
    pbrt_textured3f Ks             = {0.5, 0.5, 0.5};
    pbrt_textured1f uroughness     = 0.1;
    pbrt_textured1f vroughness     = 0.1;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  struct subsurface_t {
    string          name           = "";
    pbrt_textured3f sigma_a        = {.0011, .0024, .014};
    pbrt_textured3f sigma_prime_s  = {2.55, 3.12, 3.77};
    float           scale          = 1;
    pbrt_textured1f eta            = 1;
    pbrt_textured3f Kr             = {1, 1, 1};
    pbrt_textured3f Kt             = {1, 1, 1};
    pbrt_textured1f uroughness     = 0;
    pbrt_textured1f vroughness     = 0;
    bool            remaproughness = true;
    pbrt_textured1f bumpmap        = 0;
  };
  enum struct type_t {
    matte,
    mirror,
    plastic,
    metal,
    glass,
    translucent,
    uber,
    disney,
    fourier,
    hair,
    kdsubsurface,
    mix,
    substrate,
    subsurface
  };
  type_t         type         = type_t::matte;
  matte_t        matte        = {};
  mirror_t       mirror       = {};
  plastic_t      plastic      = {};
  metal_t        metal        = {};
  glass_t        glass        = {};
  translucent_t  translucent  = {};
  uber_t         uber         = {};
  disney_t       disney       = {};
  fourier_t      fourier      = {};
  hair_t         hair         = {};
  kdsubsurface_t kdsubsurface = {};
  mix_t          mix          = {};
  substrate_t    substrate    = {};
  subsurface_t   subsurface{};
};

// pbrt shapes
struct pbrt_shape {
  struct trianglemesh_t {
    vector<vec3i>   indices     = {};
    vector<vec3f>   P           = {};
    vector<vec3f>   N           = {};
    vector<vec3f>   S           = {};
    vector<vec2f>   uv          = {};
    pbrt_textured1f alpha       = 1;
    pbrt_textured1f shadowalpha = 1;
  };
  struct plymesh_t {
    string          filename    = {};
    pbrt_textured1f alpha       = 1;
    pbrt_textured1f shadowalpha = 1;
  };
  struct curve_t {
    enum struct type_t { flat, ribbon, cylinder };
    enum struct basis_t { bezier, bspline };
    vector<vec3f> P          = {};
    basis_t       basis      = basis_t::bezier;
    int           degree     = 3;
    type_t        type       = type_t::flat;
    vector<vec3f> N          = {};
    float         width0     = 1;
    float         width1     = 1;
    int           splitdepth = 3;
  };
  struct loopsubdiv_t {
    int           levels  = 3;
    vector<vec3i> indices = {};
    vector<vec3f> P       = {};
  };
  struct nurbs_t {
    int           nu     = -1;
    int           nv     = -1;
    vector<float> uknots = {};
    vector<float> vknots = {};
    float         u0     = -1;
    float         v0     = -1;
    float         u1     = -1;
    float         v1     = -1;
    vector<vec3f> P      = {};
    vector<float> Pw     = {};
  };
  struct sphere_t {
    float radius = 1;
    float zmin   = -radius;
    float zmax   = radius;
    float phimax = 360;
  };
  struct disk_t {
    float height      = 0;
    float radius      = 1;
    float innerradius = 0;
    float phimax      = 360;
  };
  struct cone_t {
    float radius = 1;
    float height = 1;
    float phimax = 360;
  };
  struct cylinder_t {
    float radius = 1;
    float zmin   = -1;
    float zmax   = 1;
    float phimax = 360;
  };
  struct hyperboloid_t {
    vec3f p1     = {0, 0, 0};
    vec3f p2     = {1, 1, 1};
    float phimax = 360;
  };
  struct paraboloid_t {
    float radius = 1;
    float zmin   = 0;
    float zmax   = 1;
    float phimax = 360;
  };
  struct heightfield_t {
    int           nu = 0;
    int           nv = 0;
    vector<float> Pz = {};
  };
  enum struct type_t {
    trianglemesh,
    plymesh,
    curve,
    loopsubdiv,
    nurbs,
    sphere,
    disk,
    cone,
    cylinder,
    hyperboloid,
    paraboloid,
    heightfield
  };
  type_t         type         = type_t::trianglemesh;
  trianglemesh_t trianglemesh = {};
  plymesh_t      plymesh      = {};
  curve_t        curve        = {};
  loopsubdiv_t   loopsubdiv   = {};
  nurbs_t        nurbs        = {};
  sphere_t       sphere       = {};
  disk_t         disk         = {};
  cone_t         cone         = {};
  cylinder_t     cylinder     = {};
  hyperboloid_t  hyperboloid  = {};
  paraboloid_t   paraboloid   = {};
  heightfield_t  heightfield  = {};
};

// pbrt lights
struct pbrt_light {
  struct distant_t {
    pbrt_spectrum3f scale = {1, 1, 1};
    pbrt_spectrum3f L     = {1, 1, 1};
    vec3f           from  = {0, 0, 0};
    vec3f           to    = {0, 0, 1};
  };
  struct goniometric_t {
    pbrt_spectrum3f scale   = {1, 1, 1};
    pbrt_spectrum3f I       = {1, 1, 1};
    string          mapname = "";
  };
  struct infinite_t {
    pbrt_spectrum3f scale   = {1, 1, 1};
    pbrt_spectrum3f L       = {1, 1, 1};
    int             samples = 1;
    string          mapname = "";
  };
  struct point_t {
    pbrt_spectrum3f scale = {1, 1, 1};
    pbrt_spectrum3f I     = {1, 1, 1};
    vec3f           from  = {0, 0, 0};
  };
  struct projection_t {
    pbrt_spectrum3f scale   = {1, 1, 1};
    pbrt_spectrum3f I       = {1, 1, 1};
    float           fov     = 45;
    string          mapname = "";
  };
  struct spot_t {
    pbrt_spectrum3f scale          = {1, 1, 1};
    pbrt_spectrum3f I              = {1, 1, 1};
    vec3f           from           = {0, 0, 0};
    vec3f           to             = {0, 0, 1};
    float           coneangle      = 30;
    float           conedeltaangle = 5;
  };
  enum struct type_t {
    distant,
    goniometric,
    infinite,
    point,
    projection,
    spot
  };
  type_t        type        = type_t::distant;
  distant_t     distant     = {};
  goniometric_t goniometric = {};
  infinite_t    infinite    = {};
  point_t       point       = {};
  projection_t  projection  = {};
  spot_t        spot        = {};
};

// pbrt area lights
struct pbrt_arealight {
  struct none_t {};
  struct diffuse_t {
    pbrt_spectrum3f scale    = {1, 1, 1};
    pbrt_spectrum3f L        = {1, 1, 1};
    bool            twosided = false;
    int             samples  = 1;
  };
  enum struct type_t { none, diffuse };
  type_t    type    = type_t::none;
  none_t    none    = {};
  diffuse_t diffuse = {};
};

// pbrt mediums
struct pbrt_medium {
  struct homogeneous_t {
    pbrt_spectrum3f sigma_a = {0.0011, 0.0024, 0.014};
    pbrt_spectrum3f sigma_s = {2.55, 3.21, 3.77};
    string          preset  = "";
    float           g       = 0;
    float           scale   = 1;
  };
  struct heterogeneous_t {
    pbrt_spectrum3f sigma_a = {0.0011f, 0.0024f, 0.014f};
    pbrt_spectrum3f sigma_s = {2.55f, 3.21f, 3.77f};
    string          preset  = "";
    float           g       = 0;
    float           scale   = 1;
    vec3f           p0      = {0, 0, 0};
    vec3f           p1      = {1, 1, 1};
    int             nx      = 1;
    int             ny      = 1;
    int             nz      = 1;
    vector<float>   density = {};
  };
  enum struct type_t { homogeneous, heterogeneous };
  type_t          type          = type_t::homogeneous;
  homogeneous_t   homogeneous   = {};
  heterogeneous_t heterogeneous = {};
};

// pbrt medium interface
struct pbrt_mediuminterface {
  string interior = "";
  string exterior = "";
};

// pbrt insstance
struct pbrt_object {
  string name = "";
};

// pbrt stack ctm
struct pbrt_context {
  frame3f transform_start        = identity3x4f;
  frame3f transform_end          = identity3x4f;
  string  material               = "";
  string  arealight              = "";
  string  medium_interior        = "";
  string  medium_exterior        = "";
  bool    reverse                = false;
  bool    active_transform_start = true;
  bool    active_transform_end   = true;
  float   last_lookat_distance   = 0;
};

// pbrt callbacks
struct pbrt_callbacks {
  virtual void sampler(const pbrt_sampler& value, const pbrt_context& ctx) {}
  virtual void integrator(
      const pbrt_integrator& value, const pbrt_context& ctx) {}
  virtual void accelerator(
      const pbrt_accelerator& value, const pbrt_context& ctx) {}
  virtual void film(const pbrt_film& value, const pbrt_context& ctx) {}
  virtual void filter(const pbrt_filter& value, const pbrt_context& ctx) {}
  virtual void camera(const pbrt_camera& value, const pbrt_context& ctx) {}
  virtual void texture(
      const pbrt_texture& value, const string& name, const pbrt_context& ctx) {}
  virtual void material(const pbrt_material& value, const string& name,
      const pbrt_context& ctx) {}
  virtual void medium(
      const pbrt_medium& value, const string& name, const pbrt_context& ctx) {}
  virtual void shape(const pbrt_shape& value, const pbrt_context& ctx) {}
  virtual void light(const pbrt_light& value, const pbrt_context& ctx) {}
  virtual void arealight(const pbrt_arealight& value, const string& name,
      const pbrt_context& ctx) {}
  virtual void object_instance(
      const pbrt_object& value, const pbrt_context& ctx) {}
  virtual void begin_object(const pbrt_object& value, const pbrt_context& ctx) {
  }
  virtual void end_object(const pbrt_object& value, const pbrt_context& ctx) {}
};

// Load pbrt scene
void load_pbrt(const string& filename, pbrt_callbacks& cb, bool flipv = true);

}  // namespace yocto

#endif
