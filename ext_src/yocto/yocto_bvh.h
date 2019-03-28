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
// We support working either on the whole scene or on a single shape. In the
// description below yoi will see this dual API defined.
//
// 1. create shape/scene bvhs using `init_shape_bvh()`/`build_scene_bvh()`
// 3. build the shape/scene BVH with `build_shape_bvh()`/`build_scene_bvh()`;
//    for the scene build, we will build appropriately the shape bvhs
// 4. perform ray-shape intersection with `intersect_shape_bvh()` and
//    ray-scene intersection with `intersect_scene_bvh()`
// 5. perform point overlap queries in shapes with `overlap_shape_bvh()` and on
//    scenes with `overlap_scene_bvh()`
// 6. a shape bvh can be updated if change to its positions occurs;
//    to do so, update the bvh data with `update_shape_bvh()` and then call
//    `refit_shape_bvh()`
// 7. a scene bvh can be updated if changes to its instances or shapes occur;
//    to do so update the relevant shape bvhs with `update_shape_bvh()` and
//    update the bvh data with `update_shape_bvh()` and then call
//    `refit_shape_bvh()`; you can retrive a shape bvh by index with
//    `get_shape_bvh()`
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
#include "yocto_utils.h"

// -----------------------------------------------------------------------------
// BVH FOR RAY INTERSECTION AND CLOSEST ELEMENT
// -----------------------------------------------------------------------------
namespace yocto {

// Maximum number of primitives per BVH node.
const int bvh_max_prims = 4;

// BVH tree node containing its bounds, indices to the BVH arrays of either
// primitives or internal nodes, the node element type,
// and the split axis. Leaf and internal nodes are identical, except that
// indices refer to primitives for leaf nodes or other nodes for internal
// nodes. See bvh_scene for more details.
struct bvh_node {
    bbox3f bbox;
    short  num_primitives;
    bool   is_internal;
    byte   split_axis;
    int    primitive_ids[bvh_max_prims];
};

// BVH for shapes made of points, lines, triangles or quads. Only one primitive
// type can be used.
// To build, fill in the shape data, then call `build_shape_bvh()`.
// The BVH is stored as a node array with the tree structure is encoded using
// array indices. BVH nodes indices refer to either the node array,
// for internal nodes, or the primitive arrays, for leaf nodes.
struct bvh_shape {
    // data for shape BVH
    vector<vec3f> positions;
    vector<float> radius;
    vector<int>   points;
    vector<vec2i> lines;
    vector<vec3i> triangles;
    vector<vec4i> quads;

    // bvh internal nodes
    vector<bvh_node> nodes;

    // Embree opaque data
    void* embree_bvh = nullptr;
};

// Instance for a scene BVH.
struct bvh_instance {
    frame3f frame         = identity_frame3f;
    frame3f frame_inverse = identity_frame3f;
    int     shape_id      = -1;
};

// BVH for scenes made of instances to shapes.
// To build, first build the shape BVHs, then fill in the instance data,
// then call `build_scene_bvh()`.
// The BVH is stored as a node array with the tree structure is encoded using
// array indices. BVH nodes indices refer to either the node array,
// for internal nodes, or the primitive arrays, for leaf nodes.
struct bvh_scene {
    // data for instance BVH
    vector<bvh_instance> instances;
    vector<bvh_shape>    shape_bvhs;

    // bvh internal nodes
    vector<bvh_node> nodes;

    // Embree opaque data
    void* embree_bvh       = nullptr;
    bool  embree_flattened = false;
};

// Options for build bvh
struct build_bvh_options {
    bool          high_quality   = true;
    bool          use_embree     = false;
    bool          flatten_embree = true;
    bool          run_serially   = false;
    atomic<bool>* cancel_flag    = nullptr;
};

// Build a BVH from the given set of shape primitives.
void init_shape_bvh(bvh_shape& bvh, const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius);
void init_shape_bvh(bvh_shape& bvh, const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius);
void init_shape_bvh(bvh_shape& bvh, const vector<vec3i>& triangles,
    const vector<vec3f>& positions);
void init_shape_bvh(
    bvh_shape& bvh, const vector<vec4i>& quads, const vector<vec3f>& positions);

// Build a BVH from the given set of instances.
void init_scene_bvh(bvh_scene& bvh, const vector<bvh_instance>& instances,
    const vector<bvh_shape>& shape_bvhs);

// Build the bvh acceleration structure.
void build_shape_bvh(bvh_shape& bvh, const build_bvh_options& options = {});
void build_scene_bvh(bvh_scene& bvh, const build_bvh_options& options = {});

// Update the node data for shape and scene bvhs.
void update_shape_bvh(bvh_shape& bvh, const vector<vec3f>& positions);
void update_shape_bvh(bvh_shape& bvh, const vector<vec3f>& positions,
    const vector<float>& radius);
void update_scene_bvh(bvh_scene& bvh, const vector<bvh_instance>& instances);
bvh_shape& get_shape_bvh(bvh_scene& bvh, int shape_id);

// Refit bvh data
void refit_shape_bvh(bvh_shape& bvh);
void refit_scene_bvh(bvh_scene& bvh, const vector<int>& updated_instances,
    const vector<int>& updated_shapes);

// Results of intersect_xxx and overlap_xxx functions that include hit flag,
// instance id, shape element id, shape element uv and intersection distance.
// The values are all set for scene intersection. Shape intersection does not
// set the instance id and element intersections do not set shape element id
// and the instance id. Results values are set only if hit is true.
struct bvh_shape_intersection {
    int   element_id = -1;
    vec2f element_uv = {0, 0};
    float distance   = 0;
    bool  hit        = false;
};
struct bvh_scene_intersection {
    int   instance_id = -1;
    int   element_id  = -1;
    vec2f element_uv  = {0, 0};
    float distance    = 0;
    bool  hit         = false;
};

// Intersect ray with a bvh returning either the first or any intersection
// depending on `find_any`. Returns the ray distance , the instance id,
// the shape element index and the element barycentric coordinates.
bvh_shape_intersection intersect_shape_bvh(
    const bvh_shape& bvh, const ray3f& ray, bool find_any = false);
bvh_scene_intersection intersect_scene_bvh(
    const bvh_scene& bvh, const ray3f& ray, bool find_any = false);
bvh_scene_intersection intersect_instance_bvh(const bvh_scene& bvh,
    int instance_id, const ray3f& ray, bool find_any = false);

// Find a shape element that overlaps a point within a given distance
// max distance, returning either the closest or any overlap depending on
// `find_any`. Returns the point distance, the instance id, the shape element
// index and the element barycentric coordinates.
bvh_shape_intersection overlap_shape_bvh(const bvh_shape& bvh, const vec3f& pos,
    float max_distance, bool find_any = false);
bvh_scene_intersection overlap_scene_bvh(const bvh_scene& bvh, const vec3f& pos,
    float max_distance, bool find_any = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// RAY INTERSECTION AND CLOSEST POINT FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Element intersection
struct bvh_element_intersection {
    vec2f element_uv = {0, 0};
    float distance   = 0;
    bool  hit        = false;
};

// Intersect a ray with a point (approximate).
// Based on http://geomalgorithms.com/a02-lines.html.
bvh_element_intersection intersect_point(
    const ray3f& ray, const vec3f& p, float r);

// Intersect a ray with a line (approximate).
// Based on http://geomalgorithms.com/a05-intersect-1.html and
// http://geomalgorithms.com/a07-distance.html#
//     dist3D_Segment_to_Segment
bvh_element_intersection intersect_line(
    const ray3f& ray, const vec3f& p0, const vec3f& p1, float r0, float r1);

// Intersect a ray with a triangle.
bvh_element_intersection intersect_triangle(
    const ray3f& ray, const vec3f& p0, const vec3f& p1, const vec3f& p2);

// Intersect a ray with a quad represented as two triangles (0,1,3) and
// (2,3,1), with the uv coordinates of the second triangle corrected by u =
// 1-u' and v = 1-v' to produce a quad parametrization where u and v go from 0
// to 1. This is equivalent to Intel's Embree.
bvh_element_intersection intersect_quad(const ray3f& ray, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec3f& p3);

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
bvh_element_intersection overlap_point(
    const vec3f& pos, float dist_max, const vec3f& p0, float r0);

// Find closest line point to a position.
float closestuv_line(const vec3f& pos, const vec3f& p0, const vec3f& p1);

// Check if a line overlaps a position within a max distance.
bvh_element_intersection overlap_line(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, float r0, float r1);

// Find closest triangle point to a position.
vec2f closestuv_triangle(
    const vec3f& pos, const vec3f& p0, const vec3f& p1, const vec3f& p2);

// Check if a triangle overlaps a position within a max distance.
bvh_element_intersection overlap_triangle(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, const vec3f& p2, float r0, float r1,
    float r2);

// Check if a quad overlaps a position within a max distance.
bvh_element_intersection overlap_quad(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, const vec3f& p2, const vec3f& p3,
    float r0, float r1, float r2, float r3);

// Check if a bounding box overlaps a position within a max distance.
bool overlap_bbox(const vec3f& pos, float dist_max, const bbox3f& bbox);

// Check if two bounding boxes overlap.
bool overlap_bbox(const bbox3f& bbox1, const bbox3f& bbox2);

}  // namespace yocto

#endif
