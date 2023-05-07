//
// # Yocto/Bvh: Accelerated ray-intersection and point-overlap
//
// Yocto/Bvh provides ray-intersection and point-overlap queries accelerated
// by a two-level BVH using an internal or wrapping Embree.
// Yocto/Bvh is implemented in `yocto_bvh.h` and `yocto_bvh.cpp`.
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
//

#ifndef _YOCTO_BVH_H_
#define _YOCTO_BVH_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "yocto_geometry.h"
#include "yocto_math.h"
#include "yocto_scene.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::array;
using std::string;
using std::unique_ptr;
using std::vector;

}  // namespace yocto

// -----------------------------------------------------------------------------
// BVH, RAY INTERSECTION AND OVERLAP QUERIES
// -----------------------------------------------------------------------------
namespace yocto {

// Shape BVHs are just the bvh for the shape.
struct shape_bvh {
  bvh_tree bvh = {};
};

// Scene BVHs store the bvh for instances and shapes.
// Application data is not stored explicitly.
struct scene_bvh {
  bvh_tree          bvh    = {};
  vector<shape_bvh> shapes = {};
};

// Build the bvh acceleration structure.
shape_bvh make_shape_bvh(const shape_data& shape, bool highquality = false);
scene_bvh make_scene_bvh(
    const scene_data& scene, bool highquality = false, bool noparallel = false);

// Refit bvh data
void update_shape_bvh(shape_bvh& bvh, const shape_data& shape);
void update_scene_bvh(scene_bvh& bvh, const scene_data& scene,
    const vector<int>& updated_instances, const vector<int>& updated_shapes);

// Results of intersect_xxx and overlap_xxx functions that include hit flag,
// instance id, shape element id, shape element uv and intersection distance.
// The values are all set for scene intersection. Shape intersection does not
// set the instance id and element intersections do not set shape element id
// and the instance id. Results values are set only if hit is true.
struct scene_intersection {
  int   instance = -1;
  int   element  = -1;
  vec2f uv       = {0, 0};
  float distance = 0;
  bool  hit      = false;
};

// Intersect ray with a bvh returning either the first or any intersection
// depending on `find_any`. Returns the ray distance , the instance id,
// the shape element index and the element barycentric coordinates.
shape_intersection intersect_shape_bvh(const shape_bvh& bvh,
    const shape_data& shape, const ray3f& ray, bool find_any = false);
scene_intersection intersect_scene_bvh(const scene_bvh& bvh,
    const scene_data& scene, const ray3f& ray, bool find_any = false);
scene_intersection intersect_instance_bvh(const scene_bvh& bvh,
    const scene_data& scene, int instance, const ray3f& ray,
    bool find_any = false);

// Find a shape element that overlaps a point within a given distance
// max distance, returning either the closest or any overlap depending on
// `find_any`. Returns the point distance, the instance id, the shape element
// index and the element barycentric coordinates.
shape_intersection overlap_shape_bvh(const shape_bvh& bvh,
    const shape_data& shape, const vec3f& pos, float max_distance,
    bool find_any = false);
scene_intersection overlap_scene_bvh(const scene_bvh& bvh,
    const scene_data& scene, const vec3f& pos, float max_distance,
    bool find_any = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EMBREE WRAPPER
// -----------------------------------------------------------------------------
namespace yocto {

// Wrapper for Intel's Embree
using ebvh_tree = unique_ptr<void, void (*)(void*)>;

// Wrapper for Intel Embree.
struct shape_ebvh {
  ebvh_tree ebvh = {nullptr, nullptr};
};

// Wrapper for Intel Embree.
struct scene_ebvh {
  ebvh_tree          ebvh   = {nullptr, nullptr};  // instances
  vector<shape_ebvh> shapes = {};                  // shapes
};

// Check if embree is supported
bool embree_supported();

// Build the bvh acceleration structure.
shape_ebvh make_shape_ebvh(const shape_data& shape, bool highquality = false);
scene_ebvh make_scene_ebvh(
    const scene_data& scene, bool highquality = false, bool noparallel = false);

// Refit bvh data
void update_shape_ebvh(shape_ebvh& bvh, const shape_data& shape);
void update_scene_ebvh(scene_ebvh& bvh, const scene_data& scene,
    const vector<int>& updated_instances, const vector<int>& updated_shapes);

// Intersect ray with a bvh returning either the first or any intersection
// depending on `find_any`. Returns the ray distance , the instance id,
// the shape element index and the element barycentric coordinates.
shape_intersection intersect_shape_ebvh(const shape_ebvh& bvh,
    const shape_data& shape, const ray3f& ray, bool find_any = false);
scene_intersection intersect_scene_ebvh(const scene_ebvh& bvh,
    const scene_data& scene, const ray3f& ray, bool find_any = false);
scene_intersection intersect_instance_ebvh(const scene_ebvh& bvh,
    const scene_data& scene, int instance, const ray3f& ray,
    bool find_any = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// BACKWARD COMPATIBILITY
// -----------------------------------------------------------------------------
namespace yocto {

// backward compatibility
using bvh_shape [[deprecated]] = shape_bvh;
using bvh_scene [[deprecated]] = scene_bvh;

// backward compatibility
[[deprecated]] inline shape_bvh make_bvh(
    const shape_data& shape, bool embree = false, bool highquality = false) {
  return make_shape_bvh(shape, highquality);
}
[[deprecated]] inline scene_bvh make_bvh(const scene_data& scene,
    bool embree = false, bool highquality = false, bool noparallel = false) {
  return make_scene_bvh(scene, highquality, noparallel);
}

// backward compatibility
[[deprecated]] inline void update_bvh(shape_bvh& bvh, const shape_data& shape) {
  return update_shape_bvh(bvh, shape);
}
[[deprecated]] inline void update_bvh(scene_bvh& bvh, const scene_data& scene,
    const vector<int>& updated_instances, const vector<int>& updated_shapes) {
  return update_scene_bvh(bvh, scene, updated_instances, updated_shapes);
}

// backward compatibility
[[deprecated]] inline shape_intersection intersect_bvh(const shape_bvh& bvh,
    const shape_data& shape, const ray3f& ray, bool find_any = false) {
  return intersect_shape_bvh(bvh, shape, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_bvh(const scene_bvh& bvh,
    const scene_data& scene, const ray3f& ray, bool find_any = false) {
  return intersect_scene_bvh(bvh, scene, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_bvh(const scene_bvh& bvh,
    const scene_data& scene, int instance, const ray3f& ray,
    bool find_any = false) {
  return intersect_instance_bvh(bvh, scene, instance, ray, find_any);
}

// backward compatibility
[[deprecated]] inline shape_intersection overlap_bvh(const shape_bvh& bvh,
    const shape_data& shape, const vec3f& pos, float max_distance,
    bool find_any = false) {
  return overlap_shape_bvh(bvh, shape, pos, max_distance, find_any);
}
[[deprecated]] inline scene_intersection overlap_bvh(const scene_bvh& bvh,
    const scene_data& scene, const vec3f& pos, float max_distance,
    bool find_any = false) {
  return overlap_scene_bvh(bvh, scene, pos, max_distance, find_any);
}

// backward compatibility
[[deprecated]] inline shape_intersection intersect_shape(const shape_bvh& bvh,
    const shape_data& shape, const ray3f& ray, bool find_any = false) {
  return intersect_shape_bvh(bvh, shape, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_scene(const scene_bvh& bvh,
    const scene_data& scene, const ray3f& ray, bool find_any) {
  return intersect_scene_bvh(bvh, scene, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_instance(
    const scene_bvh& bvh, const scene_data& scene, int instance,
    const ray3f& ray, bool find_any) {
  return intersect_instance_bvh(bvh, scene, instance, ray, find_any);
}

// backward compatibility
[[deprecated]] inline shape_intersection overlap_shape(const shape_bvh& bvh,
    const shape_data& shape, const vec3f& pos, float max_distance,
    bool find_any = false) {
  return overlap_shape_bvh(bvh, shape, pos, max_distance, find_any);
}
[[deprecated]] inline scene_intersection overlap_scene(const scene_bvh& bvh,
    const scene_data& scene, const vec3f& pos, float max_distance,
    bool find_any = false) {
  return overlap_scene_bvh(bvh, scene, pos, max_distance, find_any);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// EMBREE BACKWARD COMPATIBILITY
// -----------------------------------------------------------------------------
namespace yocto {

// backward compatibility
using shape_embree_bvh = shape_ebvh;
using scene_embree_bvh = scene_ebvh;

// Build the bvh acceleration structure.
[[deprecated]] inline shape_embree_bvh make_shape_embree_bvh(
    const shape_data& shape, bool highquality = false) {
  return make_shape_ebvh(shape, highquality);
}
[[deprecated]] inline scene_embree_bvh make_scene_embree_bvh(
    const scene_data& scene, bool highquality = false,
    bool noparallel = false) {
  return make_scene_ebvh(scene, highquality, noparallel);
}

// Refit bvh data
[[deprecated]] inline void update_shape_embree_bvh(
    shape_embree_bvh& bvh, const shape_data& shape) {
  return update_shape_ebvh(bvh, shape);
}
[[deprecated]] inline void update_scene_embree_bvh(scene_ebvh& bvh,
    const scene_data& scene, const vector<int>& updated_instances,
    const vector<int>& updated_shapes) {
  return update_scene_ebvh(bvh, scene, updated_instances, updated_shapes);
}

// Intersect ray with a bvh returning either the first or any intersection
// depending on `find_any`. Returns the ray distance , the instance id,
// the shape element index and the element barycentric coordinates.
[[deprecated]] inline shape_intersection intersect_shape_embree_bvh(
    const shape_ebvh& bvh, const shape_data& shape, const ray3f& ray,
    bool find_any = false) {
  return intersect_shape_ebvh(bvh, shape, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_scene_embree_bvh(
    const scene_ebvh& bvh, const scene_data& scene, const ray3f& ray,
    bool find_any = false) {
  return intersect_scene_ebvh(bvh, scene, ray, find_any);
}
[[deprecated]] inline scene_intersection intersect_instance_embree_bvh(
    const scene_ebvh& bvh, const scene_data& scene, int instance,
    const ray3f& ray, bool find_any = false) {
  return intersect_instance_ebvh(bvh, scene, instance, ray, find_any);
}

}  // namespace yocto

#endif
