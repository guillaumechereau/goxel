//
// # Yocto/BVH: Tiny library for ray-object intersection using a BVH
//
//
// Yocto/BVH is a simple implementation of ray intersection and
// closest queries using a two-level BVH data structure. We also include
// low-level intersection and closet point primitives.
// Alternatively the library also support wrapping Intel's Embree.
//
//
// ## Ray-Scene and Closest-Point Queries
//
// Yocto/GL provides ray-scene intersection for points, lines, triangles and
// quads accelerated by a two-level BVH data structure. Our BVH is written for
// minimal code and not maximum speed, but still gives reasonable results. We
// suggest the use of Intel's Embree as a more efficient alternative.
//
// In Yocto/Bvh, shapes are described as collections of indexed primitives
// (points/lines/triangles/quads) like the standard triangle mesh used in
// real-time graphics. A scene if represented as transformed instances of
// shapes. The internal data structure is a two-level BVH, with a BVH for each
// shape and one top-level BVH for the whole scene. This design support
// instancing for large scenes and easy BVH refitting for interactive
// applications.
//
// In these functions triangles are parameterized with uv written
// w.r.t the (p1-p0) and (p2-p0) axis respectively. Quads are internally handled
// as pairs of two triangles p0,p1,p3 and p2,p3,p1, with the u/v coordinates
// of the second triangle corrected as 1-u and 1-v to produce a quad
// parametrization where u and v go from 0 to 1. Degenerate quads with p2==p3
// represent triangles correctly, an this convention is used throught the
// library. This is equivalent to Intel's Embree.
//
// Shape and scene data is not copied from the application to the BVH to
// improve memory footprint at the price of convenience. Shape data is
// explixitly passed on evey call, while instance data uses callbacks,
// since each application has its own conventions for storing those.
// To make usage more convenite, we provide `bvh_XXX_data` to hold application
// data and convenience wrappers for all functions.
//
// We support working either on the whole scene or on a single shape. In the
// description below yoi will see this dual API defined.
//
// 1. build the shape/scene BVH with `build_bvh()`;
// 2. perform ray-shape intersection with `intersect_bvh()`
// 3. perform point overlap queries with `overlap_bvh()`
// 4. refit BVH for dynamic applications with `refit_bvh`
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

#ifndef _YOCTO_BVH_H_
#define _YOCTO_BVH_H_

#ifndef YOCTO_EMBREE
#define YOCTO_EMBREE 1
#endif

#ifndef YOCTO_QUADS_AS_TRIANGLES
#define YOCTO_QUADS_AS_TRIANGLES 1
#endif

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"

#include <atomic>

// -----------------------------------------------------------------------------
// BVH FOR RAY INTERSECTION AND CLOSEST ELEMENT
// -----------------------------------------------------------------------------
namespace yocto {

// Maximum number of primitives per BVH node.
const int bvh_max_prims = 4;

// BVH array view
template <typename T>
struct bvh_span {
  bvh_span() : ptr{nullptr}, count{0} {}
  bvh_span(const T* ptr, int count) : ptr{ptr}, count{count} {}
  bvh_span(const vector<T>& vec) : ptr{vec.data()}, count{(int)vec.size()} {}

  bool empty() const { return count == 0; }
  int  size() const { return count; }

  const T& operator[](int idx) const { return ptr[idx]; }
  const T* data() const { return ptr; }
  const T* begin() const { return ptr; }
  const T* end() const { return ptr + count; }

 private:
  const T* ptr   = nullptr;
  int      count = 0;
};

// BVH array view with stride
template <typename T>
struct bvh_sspan {
  bvh_sspan() : ptr{nullptr}, count{0}, stride{0} {}
  bvh_sspan(const void* ptr, int count, int stride)
      : ptr{ptr}, count{count}, stride{stride} {}

  bool empty() const { return count == 0; }
  int  size() const { return count; }

  const T& operator[](int idx) const {
    return *(const T*)((const char*)ptr + (size_t)idx * (size_t)stride);
  }

 private:
  const void* ptr    = nullptr;
  int         count  = 0;
  int         stride = 0;
};

// BVH tree node containing its bounds, indices to the BVH arrays of either
// primitives or internal nodes, the node element type,
// and the split axis. Leaf and internal nodes are identical, except that
// indices refer to primitives for leaf nodes or other nodes for internal nodes.
struct bvh_node {
  bbox3f bbox;
  short  num;
  bool   internal;
  byte   axis;
  int    prims[bvh_max_prims];
};

// BVH tree stored as a node array with the tree structure is encoded using
// array indices. BVH nodes indices refer to either the node array,
// for internal nodes, or the primitive arrays, for leaf nodes.
// Applicxation data is not stored explicitly.
struct bvh_shape {
  // elements
  bvh_span<int>   points    = {};
  bvh_span<vec2i> lines     = {};
  bvh_span<vec3i> triangles = {};
  bvh_span<vec4i> quads     = {};
  bvh_span<vec4i> quadspos  = {};

  // vertices
  bvh_span<vec3f> positions = {};
  bvh_span<float> radius    = {};

  // nodes
  vector<bvh_node> nodes = {};

#if YOCTO_EMBREE
  // Embree opaque data
  void* embree_bvh       = nullptr;
  bool  embree_flattened = false;
  // Cleanup for embree data
  ~bvh_shape();
#endif
};

// Instance for a scene BVH.
struct bvh_instance {
  frame3f frame = identity3x4f;
  int     shape = -1;
};

struct bvh_scene {
  // instances and shapes
  bvh_sspan<bvh_instance> instances = {};
  vector<bvh_shape>       shapes    = {};

  // nodes
  vector<bvh_node> nodes = {};

#if YOCTO_EMBREE
  // Embree opaque data
  void* embree_bvh       = nullptr;
  bool  embree_flattened = false;
  // Cleanup for embree data
  ~bvh_scene();
#endif
};

// bvh build params
struct bvh_params {
  bool high_quality = false;
#if YOCTO_EMBREE
  bool use_embree     = false;
  bool embree_flatten = false;
  bool embree_compact = false;
#endif
  bool               noparallel = false;
  std::atomic<bool>* cancel     = nullptr;
};

// Initialize bvh data
inline bvh_shape make_points_bvh(
    bvh_span<int> points, bvh_span<vec3f> positions, bvh_span<float> radius) {
  return bvh_shape{points, {}, {}, {}, {}, positions, radius};
}
inline bvh_shape make_lines_bvh(
    bvh_span<vec2i> lines, bvh_span<vec3f> positions, bvh_span<float> radius) {
  return bvh_shape{{}, lines, {}, {}, {}, positions, radius};
}
inline bvh_shape make_triangles_bvh(bvh_span<vec3i> triangles,
    bvh_span<vec3f> positions, bvh_span<float> radius) {
  return bvh_shape{{}, {}, triangles, {}, {}, positions, radius};
}
inline bvh_shape make_quads_bvh(
    bvh_span<vec4i> quads, bvh_span<vec3f> positions, bvh_span<float> radius) {
  return bvh_shape{{}, {}, {}, quads, {}, positions, radius};
}
inline bvh_shape make_quadspos_bvh(bvh_span<vec4i> quadspos,
    bvh_span<vec3f> positions, bvh_span<float> radius) {
  return bvh_shape{{}, {}, {}, {}, quadspos, positions, radius};
}
inline bvh_scene make_instances_bvh(
    bvh_sspan<bvh_instance> instances, const vector<bvh_shape>& shapes) {
  return bvh_scene{instances, shapes};
}

// Build the bvh acceleration structure.
void build_bvh(bvh_shape& bvh, const bvh_params& params);
void build_bvh(bvh_scene& bvh, const bvh_params& params);

// Refit bvh data
void refit_bvh(bvh_shape& bvh, const bvh_params& params);
void refit_bvh(bvh_scene& bvh, const vector<int>& updated_shapes,
    const bvh_params& params);

// Intersect ray with a bvh returning either the first or any intersection
// depending on `find_any`. Returns the ray distance , the instance id,
// the shape element index and the element barycentric coordinates.
bool intersect_bvh(const bvh_shape& bvh, const ray3f& ray, int& element,
    vec2f& uv, float& distance, bool find_any = false);
bool intersect_bvh(const bvh_scene& bvh, const ray3f& ray, int& instance,
    int& element, vec2f& uv, float& distance, bool find_any = false,
    bool non_rigid_frames = true);
// Intersects a single instance.
bool intersect_bvh(const bvh_scene& bvh, int instance, const ray3f& ray,
    int& element, vec2f& uv, float& distance, bool find_any = false,
    bool non_rigid_frames = true);

// Find a shape element that overlaps a point within a given distance
// max distance, returning either the closest or any overlap depending on
// `find_any`. Returns the point distance, the instance id, the shape element
// index and the element barycentric coordinates.
bool overlap_bvh(const bvh_shape& bvh, const vec3f& pos, float max_distance,
    int& element, vec2f& uv, float& distance, bool find_any = false);
bool overlap_bvh(const bvh_scene& bvh, const vec3f& pos, float max_distance,
    int& instance, int& element, vec2f& uv, float& distance,
    bool find_any = false, bool non_rigid_frames = true);

// Results of intersect_xxx and overlap_xxx functions that include hit flag,
// instance id, shape element id, shape element uv and intersection distance.
// The values are all set for scene intersection. Shape intersection does not
// set the instance id and element intersections do not set shape element id
// and the instance id. Results values are set only if hit is true.
struct bvh_intersection {
  int   instance = -1;
  int   element  = -1;
  vec2f uv       = {0, 0};
  float distance = 0;
  bool  hit      = false;
};

bvh_intersection intersect_bvh(
    const bvh_shape& bvh, const ray3f& ray, bool find_any = false);
bvh_intersection intersect_bvh(const bvh_scene& bvh, const ray3f& ray,
    bool find_any = false, bool non_rigid_frames = true);
bvh_intersection intersect_bvh(const bvh_scene& bvh, int instance,
    const ray3f& ray, bool find_any = false, bool non_rigid_frames = true);

bvh_intersection overlap_bvh(const bvh_shape& bvh, const vec3f& pos,
    float max_distance, bool find_any = false);
bvh_intersection overlap_bvh(const bvh_scene& bvh, const vec3f& pos,
    float max_distance, bool find_any = false, bool non_rigid_frames = true);

}  // namespace yocto

// -----------------------------------------------------------------------------
// BVH UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Print bvh statistics.
string format_stats(const bvh_shape& bvh);
string format_stats(const bvh_scene& bvh);

}  // namespace yocto

// -----------------------------------------------------------------------------
// RAY INTERSECTION AND CLOSEST POINT FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Intersect a ray with a point (approximate).
// Based on http://geomalgorithms.com/a02-lines.html.
bool intersect_point(
    const ray3f& ray, const vec3f& p, float r, vec2f& uv, float& dist);

// Intersect a ray with a line (approximate).
// Based on http://geomalgorithms.com/a05-intersect-1.html and
// http://geomalgorithms.com/a07-distance.html#
//     dist3D_Segment_to_Segment
bool intersect_line(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    float r0, float r1, vec2f& uv, float& dist);

// Intersect a ray with a triangle.
bool intersect_triangle(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    const vec3f& p2, vec2f& uv, float& dist);

// Intersect a ray with a quad represented as two triangles (0,1,3) and
// (2,3,1), with the uv coordinates of the second triangle corrected by u =
// 1-u' and v = 1-v' to produce a quad parametrization where u and v go from 0
// to 1. This is equivalent to Intel's Embree.
bool intersect_quad(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    const vec3f& p2, const vec3f& p3, vec2f& uv, float& dist);

// Intersect a ray with a axis-aligned bounding box.
bool intersect_bbox(const ray3f& ray, const bbox3f& bbox);

// Intersect a ray with a axis-aligned bounding box, implemented as
// "Robust BVH Ray Traversal" by T. Ize published at
// http://jcgt.org/published/0002/02/02/paper.pdf
bool intersect_bbox(const ray3f& ray, const vec3f& ray_dinv,
    const vec3i& ray_dsign, const bbox3f& bbox);

// Intersect a ray with a axis-aligned bounding box, implemented as
// "A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering" at
// http://jcgt.org/published/0007/03/04/
// but using the Wald implementation
bool intersect_bbox(const ray3f& ray, const vec3f& ray_dinv,
    const vec3i& ray_dsign, const bbox3f& bbox);

// Check if a point overlaps a position within a max distance.
bool overlap_point(const vec3f& pos, float dist_max, const vec3f& p0, float r0,
    vec2f& uv, float& dist);

// Find closest line point to a position.
float closestuv_line(const vec3f& pos, const vec3f& p0, const vec3f& p1);

// Check if a line overlaps a position within a max distance.
bool overlap_line(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, float r0, float r1, vec2f& uv, float& dist);

// Find closest triangle point to a position.
vec2f closestuv_triangle(
    const vec3f& pos, const vec3f& p0, const vec3f& p1, const vec3f& p2);

// Check if a triangle overlaps a position within a max distance.
bool overlap_triangle(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, float r0, float r1, float r2, vec2f& uv,
    float& dist);

// Check if a quad overlaps a position within a max distance.
bool overlap_quad(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec3f& p3, float r0, float r1,
    float r2, float r3, vec2f& uv, float& dist);

// Check if a bounding box overlaps a position within a max distance.
bool overlap_bbox(const vec3f& pos, float dist_max, const bbox3f& bbox);

// Check if two bounding boxes overlap.
bool overlap_bbox(const bbox3f& bbox1, const bbox3f& bbox2);

}  // namespace yocto

#endif
