//
// Implementation for Yocto/BVH
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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_bvh.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <future>
#include <thread>

#if YOCTO_EMBREE
#include <embree3/rtcore.h>
#endif

// -----------------------------------------------------------------------------
// IMPLEMENRTATION OF RAY-PRIMITIVE INTERSECTION FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Intersect a ray with a point (approximate)
bool intersect_point(
    const ray3f& ray, const vec3f& p, float r, vec2f& uv, float& dist) {
  // find parameter for line-point minimum distance
  auto w = p - ray.o;
  auto t = dot(w, ray.d) / dot(ray.d, ray.d);

  // exit if not within bounds
  if (t < ray.tmin || t > ray.tmax) return false;

  // test for line-point distance vs point radius
  auto rp  = ray.o + ray.d * t;
  auto prp = p - rp;
  if (dot(prp, prp) > r * r) return false;

  // intersection occurred: set params and exit
  uv   = {0, 0};
  dist = t;
  return true;
}

// Intersect a ray with a line
bool intersect_line(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    float r0, float r1, vec2f& uv, float& dist) {
  // setup intersection params
  auto u = ray.d;
  auto v = p1 - p0;
  auto w = ray.o - p0;

  // compute values to solve a linear system
  auto a   = dot(u, u);
  auto b   = dot(u, v);
  auto c   = dot(v, v);
  auto d   = dot(u, w);
  auto e   = dot(v, w);
  auto det = a * c - b * b;

  // check determinant and exit if lines are parallel
  // (could use EPSILONS if desired)
  if (det == 0) return false;

  // compute Parameters on both ray and segment
  auto t = (b * e - c * d) / det;
  auto s = (a * e - b * d) / det;

  // exit if not within bounds
  if (t < ray.tmin || t > ray.tmax) return false;

  // clamp segment param to segment corners
  s = clamp(s, (float)0, (float)1);

  // compute segment-segment distance on the closest points
  auto pr  = ray.o + ray.d * t;
  auto pl  = p0 + (p1 - p0) * s;
  auto prl = pr - pl;

  // check with the line radius at the same point
  auto d2 = dot(prl, prl);
  auto r  = r0 * (1 - s) + r1 * s;
  if (d2 > r * r) return {};

  // intersection occurred: set params and exit
  uv   = {s, sqrt(d2) / r};
  dist = t;
  return true;
}

// Intersect a ray with a triangle
bool intersect_triangle(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    const vec3f& p2, vec2f& uv, float& dist) {
  // compute triangle edges
  auto edge1 = p1 - p0;
  auto edge2 = p2 - p0;

  // compute determinant to solve a linear system
  auto pvec = cross(ray.d, edge2);
  auto det  = dot(edge1, pvec);

  // check determinant and exit if triangle and ray are parallel
  // (could use EPSILONS if desired)
  if (det == 0) return false;
  auto inv_det = 1.0f / det;

  // compute and check first bricentric coordinated
  auto tvec = ray.o - p0;
  auto u    = dot(tvec, pvec) * inv_det;
  if (u < 0 || u > 1) return false;

  // compute and check second bricentric coordinated
  auto qvec = cross(tvec, edge1);
  auto v    = dot(ray.d, qvec) * inv_det;
  if (v < 0 || u + v > 1) return false;

  // compute and check ray parameter
  auto t = dot(edge2, qvec) * inv_det;
  if (t < ray.tmin || t > ray.tmax) return false;

  // intersection occurred: set params and exit
  uv   = {u, v};
  dist = t;
  return true;
}

// Intersect a ray with a quad.
bool intersect_quad(const ray3f& ray, const vec3f& p0, const vec3f& p1,
    const vec3f& p2, const vec3f& p3, vec2f& uv, float& dist) {
  if (p2 == p3) {
    return intersect_triangle(ray, p0, p1, p3, uv, dist);
  }
  auto hit  = false;
  auto tray = ray;
  if (intersect_triangle(tray, p0, p1, p3, uv, dist)) {
    hit       = true;
    tray.tmax = dist;
  }
  if (intersect_triangle(tray, p2, p3, p1, uv, dist)) {
    hit       = true;
    uv        = 1 - uv;
    tray.tmax = dist;
  }
  return hit;
}

// Intersect a ray with a axis-aligned bounding box
inline bool intersect_bbox(const ray3f& ray, const bbox3f& bbox) {
  // determine intersection ranges
  auto invd = 1.0f / ray.d;
  auto t0   = (bbox.min - ray.o) * invd;
  auto t1   = (bbox.max - ray.o) * invd;
  // flip based on range directions
  if (invd.x < 0.0f) swap(t0.x, t1.x);
  if (invd.y < 0.0f) swap(t0.y, t1.y);
  if (invd.z < 0.0f) swap(t0.z, t1.z);
  auto tmin = max(t0.z, max(t0.y, max(t0.x, ray.tmin)));
  auto tmax = min(t1.z, min(t1.y, min(t1.x, ray.tmax)));
  tmax *= 1.00000024f;  // for double: 1.0000000000000004
  return tmin <= tmax;
}

// Intersect a ray with a axis-aligned bounding box
inline bool intersect_bbox(
    const ray3f& ray, const vec3f& ray_dinv, const bbox3f& bbox) {
  auto it_min = (bbox.min - ray.o) * ray_dinv;
  auto it_max = (bbox.max - ray.o) * ray_dinv;
  auto tmin   = min(it_min, it_max);
  auto tmax   = max(it_min, it_max);
  auto t0     = max(max(tmin), ray.tmin);
  auto t1     = min(min(tmax), ray.tmax);
  t1 *= 1.00000024f;  // for double: 1.0000000000000004
  return t0 <= t1;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENRTATION OF POINT-PRIMITIVE DISTANCE FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// TODO: documentation
bool overlap_point(const vec3f& pos, float dist_max, const vec3f& p, float r,
    vec2f& uv, float& dist) {
  auto d2 = dot(pos - p, pos - p);
  if (d2 > (dist_max + r) * (dist_max + r)) return false;
  uv   = {0, 0};
  dist = sqrt(d2);
  return true;
}

// TODO: documentation
float closestuv_line(const vec3f& pos, const vec3f& p0, const vec3f& p1) {
  auto ab = p1 - p0;
  auto d  = dot(ab, ab);
  // Project c onto ab, computing parameterized position d(t) = a + t*(b –
  // a)
  auto u = dot(pos - p0, ab) / d;
  u      = clamp(u, (float)0, (float)1);
  return u;
}

// TODO: documentation
bool overlap_line(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, float r0, float r1, vec2f& uv, float& dist) {
  auto u = closestuv_line(pos, p0, p1);
  // Compute projected position from the clamped t d = a + t * ab;
  auto p  = p0 + (p1 - p0) * u;
  auto r  = r0 + (r1 - r0) * u;
  auto d2 = dot(pos - p, pos - p);
  // check distance
  if (d2 > (dist_max + r) * (dist_max + r)) return false;
  // done
  uv   = {u, 0};
  dist = sqrt(d2);
  return true;
}

// TODO: documentation
// this is a complicated test -> I probably "--"+prefix to use a sequence of
// test (triangle body, and 3 edges)
vec2f closestuv_triangle(
    const vec3f& pos, const vec3f& p0, const vec3f& p1, const vec3f& p2) {
  auto ab = p1 - p0;
  auto ac = p2 - p0;
  auto ap = pos - p0;

  auto d1 = dot(ab, ap);
  auto d2 = dot(ac, ap);

  // corner and edge cases
  if (d1 <= 0 && d2 <= 0) return {0, 0};

  auto bp = pos - p1;
  auto d3 = dot(ab, bp);
  auto d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3) return {1, 0};

  auto vc = d1 * d4 - d3 * d2;
  if ((vc <= 0) && (d1 >= 0) && (d3 <= 0)) return {d1 / (d1 - d3), 0};

  auto cp = pos - p2;
  auto d5 = dot(ab, cp);
  auto d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6) return {0, 1};

  auto vb = d5 * d2 - d1 * d6;
  if ((vb <= 0) && (d2 >= 0) && (d6 <= 0)) return {0, d2 / (d2 - d6)};

  auto va = d3 * d6 - d5 * d4;
  if ((va <= 0) && (d4 - d3 >= 0) && (d5 - d6 >= 0)) {
    auto w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return {1 - w, w};
  }

  // face case
  auto denom = 1 / (va + vb + vc);
  auto u     = vb * denom;
  auto v     = vc * denom;
  return {u, v};
}

// TODO: documentation
bool overlap_triangle(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, float r0, float r1, float r2, vec2f& uv,
    float& dist) {
  auto cuv = closestuv_triangle(pos, p0, p1, p2);
  auto p   = p0 * (1 - cuv.x - cuv.y) + p1 * cuv.x + p2 * cuv.y;
  auto r   = r0 * (1 - cuv.x - cuv.y) + r1 * cuv.x + r2 * cuv.y;
  auto dd  = dot(p - pos, p - pos);
  if (dd > (dist_max + r) * (dist_max + r)) return false;
  uv   = cuv;
  dist = sqrt(dd);
  return true;
}

// TODO: documentation
bool overlap_quad(const vec3f& pos, float dist_max, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec3f& p3, float r0, float r1,
    float r2, float r3, vec2f& uv, float& dist) {
  if (p2 == p3) {
    return overlap_triangle(pos, dist_max, p0, p1, p3, r0, r1, r2, uv, dist);
  }
  auto hit = false;
  if (overlap_triangle(pos, dist_max, p0, p1, p3, r0, r1, r2, uv, dist)) {
    hit      = true;
    dist_max = dist;
  }
  if (!overlap_triangle(pos, dist_max, p2, p3, p1, r2, r3, r1, uv, dist)) {
    hit = true;
    uv  = 1 - uv;
    // dist_max = dist;
  }
  return hit;
}

// TODO: documentation
inline bool distance_check_bbox(
    const vec3f& pos, float dist_max, const bbox3f& bbox) {
  // computing distance
  auto dd = 0.0f;

  // For each axis count any excess distance outside box extents
  if (pos.x < bbox.min.x) dd += (bbox.min.x - pos.x) * (bbox.min.x - pos.x);
  if (pos.x > bbox.max.x) dd += (pos.x - bbox.max.x) * (pos.x - bbox.max.x);
  if (pos.y < bbox.min.y) dd += (bbox.min.y - pos.y) * (bbox.min.y - pos.y);
  if (pos.y > bbox.max.y) dd += (pos.y - bbox.max.y) * (pos.y - bbox.max.y);
  if (pos.z < bbox.min.z) dd += (bbox.min.z - pos.z) * (bbox.min.z - pos.z);
  if (pos.z > bbox.max.z) dd += (pos.z - bbox.max.z) * (pos.z - bbox.max.z);

  // check distance
  return dd < dist_max * dist_max;
}

// TODO: doc
inline bool overlap_bbox(const bbox3f& bbox1, const bbox3f& bbox2) {
  if (bbox1.max.x < bbox2.min.x || bbox1.min.x > bbox2.max.x) return false;
  if (bbox1.max.y < bbox2.min.y || bbox1.min.y > bbox2.max.y) return false;
  if (bbox1.max.z < bbox2.min.z || bbox1.min.z > bbox2.max.z) return false;
  return true;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR BVH
// -----------------------------------------------------------------------------
namespace yocto {

#if YOCTO_EMBREE
// Cleanup
bvh_shape::~bvh_shape() {
  if (embree_bvh) {
    rtcReleaseScene((RTCScene)embree_bvh);
  }
}

// Cleanup
bvh_scene::~bvh_scene() {
  if (embree_bvh) {
    rtcReleaseScene((RTCScene)embree_bvh);
  }
}

static void embree_error(void* ctx, RTCError code, const char* str) {
  switch (code) {
    case RTC_ERROR_UNKNOWN:
      throw std::runtime_error("RTC_ERROR_UNKNOWN: "s + str);
      break;
    case RTC_ERROR_INVALID_ARGUMENT:
      throw std::runtime_error("RTC_ERROR_INVALID_ARGUMENT: "s + str);
      break;
    case RTC_ERROR_INVALID_OPERATION:
      throw std::runtime_error("RTC_ERROR_INVALID_OPERATION: "s + str);
      break;
    case RTC_ERROR_OUT_OF_MEMORY:
      throw std::runtime_error("RTC_ERROR_OUT_OF_MEMORY: "s + str);
      break;
    case RTC_ERROR_UNSUPPORTED_CPU:
      throw std::runtime_error("RTC_ERROR_UNSUPPORTED_CPU: "s + str);
      break;
    case RTC_ERROR_CANCELLED:
      throw std::runtime_error("RTC_ERROR_CANCELLED: "s + str);
      break;
    default: throw std::runtime_error("invalid error code"); break;
  }
}

// Embree memory
std::atomic<ssize_t> embree_memory = 0;
static bool embree_memory_monitor(void* userPtr, ssize_t bytes, bool post) {
  embree_memory += bytes;
  return true;
}

// Get Embree device
static RTCDevice get_embree_device() {
  static RTCDevice device = nullptr;
  if (!device) {
    device = rtcNewDevice("");
    rtcSetDeviceErrorFunction(device, embree_error, nullptr);
    rtcSetDeviceMemoryMonitorFunction(device, embree_memory_monitor, nullptr);
  }
  return device;
}

// Initialize Embree BVH
static void build_embree_bvh(bvh_shape& shape, const bvh_params& params) {
  auto embree_device = get_embree_device();
  auto embree_scene  = rtcNewScene(embree_device);
  if (params.embree_compact) {
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_COMPACT);
  }
  if (params.high_quality) {
    rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
  }
  shape.embree_bvh = embree_scene;
  auto embree_geom = (RTCGeometry) nullptr;
  if (!shape.points.empty()) {
    throw std::runtime_error("embree does not support points");
  } else if (!shape.lines.empty()) {
    auto elines     = vector<int>{};
    auto epositions = vector<vec4f>{};
    auto last_index = -1;
    for (auto& l : shape.lines) {
      if (last_index == l.x) {
        elines.push_back((int)epositions.size() - 1);
        epositions.push_back({shape.positions[l.y], shape.radius[l.y]});
      } else {
        elines.push_back((int)epositions.size());
        epositions.push_back({shape.positions[l.x], shape.radius[l.x]});
        epositions.push_back({shape.positions[l.y], shape.radius[l.y]});
      }
      last_index = l.y;
    }
    embree_geom = rtcNewGeometry(
        get_embree_device(), RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE);
    rtcSetGeometryVertexAttributeCount(embree_geom, 1);
    auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
        RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * 4, epositions.size());
    auto embree_lines     = rtcSetNewGeometryBuffer(embree_geom,
        RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT, 4, elines.size());
    memcpy(embree_positions, epositions.data(), epositions.size() * 16);
    memcpy(embree_lines, elines.data(), elines.size() * 4);
  } else if (!shape.triangles.empty()) {
    embree_geom = rtcNewGeometry(
        get_embree_device(), RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetGeometryVertexAttributeCount(embree_geom, 1);
    if (params.embree_compact) {
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_VERTEX, 0,
          RTC_FORMAT_FLOAT3, shape.positions.data(), 0, 3 * 4,
          shape.positions.size());
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_INDEX, 0,
          RTC_FORMAT_UINT3, shape.triangles.data(), 0, 3 * 4,
          shape.triangles.size());
    } else {
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          shape.positions.size());
      auto embree_triangles = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * 4,
          shape.triangles.size());
      memcpy(embree_positions, shape.positions.data(),
          shape.positions.size() * 12);
      memcpy(embree_triangles, shape.triangles.data(),
          shape.triangles.size() * 12);
    }
  } else if (!shape.quads.empty()) {
    embree_geom = rtcNewGeometry(get_embree_device(), RTC_GEOMETRY_TYPE_QUAD);
    rtcSetGeometryVertexAttributeCount(embree_geom, 1);
    if (params.embree_compact) {
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_VERTEX, 0,
          RTC_FORMAT_FLOAT3, shape.positions.data(), 0, 3 * 4,
          shape.positions.size());
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_INDEX, 0,
          RTC_FORMAT_UINT4, shape.quads.data(), 0, 4 * 4, shape.quads.size());
    } else {
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          shape.positions.size());
      auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
          shape.quads.size());
      memcpy(embree_positions, shape.positions.data(),
          shape.positions.size() * 12);
      memcpy(embree_quads, shape.quads.data(), shape.quads.size() * 16);
    }
  } else if (!shape.quadspos.empty()) {
    embree_geom = rtcNewGeometry(get_embree_device(), RTC_GEOMETRY_TYPE_QUAD);
    rtcSetGeometryVertexAttributeCount(embree_geom, 1);
    if (params.embree_compact) {
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_VERTEX, 0,
          RTC_FORMAT_FLOAT3, shape.positions.data(), 0, 3 * 4,
          shape.positions.size());
      rtcSetSharedGeometryBuffer(embree_geom, RTC_BUFFER_TYPE_INDEX, 0,
          RTC_FORMAT_UINT4, shape.quadspos.data(), 0, 4 * 4,
          shape.quadspos.size());
    } else {
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          shape.positions.size());
      auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
          shape.quadspos.size());
      memcpy(embree_positions, shape.positions.data(),
          shape.positions.size() * 12);
      memcpy(embree_quads, shape.quadspos.data(), shape.quadspos.size() * 16);
    }
  }
  rtcCommitGeometry(embree_geom);
  rtcAttachGeometryByID(embree_scene, embree_geom, 0);
  rtcCommitScene(embree_scene);
  shape.embree_flattened = false;
}
// Build a BVH using Embree.
static void build_embree_bvh(bvh_scene& scene, const bvh_params& params) {
  // scene bvh
  auto embree_device = get_embree_device();
  auto embree_scene  = rtcNewScene(embree_device);
  if (params.embree_compact) {
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_COMPACT);
  }
  if (params.high_quality) {
    rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
  }
  scene.embree_bvh = embree_scene;
  if (scene.instances.empty()) {
    rtcCommitScene(embree_scene);
    return;
  }
  for (auto instance_id = 0; instance_id < scene.instances.size();
       instance_id++) {
    auto& instance = scene.instances[instance_id];
    if (instance.shape < 0) throw std::runtime_error("empty instance");
    auto& shape = scene.shapes[instance.shape];
    if (!shape.embree_bvh) throw std::runtime_error("bvh not built");
    auto embree_geom = rtcNewGeometry(
        embree_device, RTC_GEOMETRY_TYPE_INSTANCE);
    rtcSetGeometryInstancedScene(embree_geom, (RTCScene)shape.embree_bvh);
    rtcSetGeometryTransform(
        embree_geom, 0, RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR, &instance.frame);
    rtcCommitGeometry(embree_geom);
    rtcAttachGeometryByID(embree_scene, embree_geom, instance_id);
  }
  rtcCommitScene(embree_scene);
  scene.embree_flattened = false;
}

// Initialize Embree BVH
static void build_embree_flattened_bvh(
    bvh_scene& scene, const bvh_params& params) {
  // scene bvh
  auto embree_device = get_embree_device();
  auto embree_scene  = rtcNewScene(embree_device);
  if (params.embree_compact) {
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_COMPACT);
  }
  if (params.high_quality) {
    rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
  }
  scene.embree_bvh = embree_scene;
  if (scene.instances.empty()) {
    rtcCommitScene(embree_scene);
    return;
  }
  for (auto instance_id = 0; instance_id < scene.instances.size();
       instance_id++) {
    auto& instance    = scene.instances[instance_id];
    auto& shape       = scene.shapes[instance.shape];
    auto  embree_geom = (RTCGeometry) nullptr;
    if (shape.positions.empty()) continue;
    auto transformed_positions = vector<vec3f>{
        shape.positions.begin(), shape.positions.end()};
    if (instance.frame != identity3x4f) {
      for (auto& p : transformed_positions)
        p = transform_point(instance.frame, p);
    }
    if (!shape.points.empty()) {
      throw std::runtime_error("embree does not support points");
    } else if (!shape.lines.empty()) {
      auto elines     = vector<int>{};
      auto epositions = vector<vec4f>{};
      auto last_index = -1;
      for (auto& l : shape.lines) {
        if (last_index == l.x) {
          elines.push_back((int)transformed_positions.size() - 1);
          epositions.push_back({transformed_positions[l.y], shape.radius[l.y]});
        } else {
          elines.push_back((int)transformed_positions.size());
          epositions.push_back({transformed_positions[l.x], shape.radius[l.x]});
          epositions.push_back({transformed_positions[l.y], shape.radius[l.y]});
        }
        last_index = l.y;
      }
      embree_geom = rtcNewGeometry(
          get_embree_device(), RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE);
      rtcSetGeometryVertexAttributeCount(embree_geom, 1);
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * 4,
          epositions.size());
      auto embree_lines     = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT, 4, elines.size());
      memcpy(embree_positions, epositions.data(), epositions.size() * 16);
      memcpy(embree_lines, elines.data(), elines.size() * 4);
    } else if (!shape.triangles.empty()) {
      embree_geom = rtcNewGeometry(
          get_embree_device(), RTC_GEOMETRY_TYPE_TRIANGLE);
      rtcSetGeometryVertexAttributeCount(embree_geom, 1);
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          transformed_positions.size());
      auto embree_triangles = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * 4,
          shape.triangles.size());
      memcpy(embree_positions, transformed_positions.data(),
          transformed_positions.size() * 12);
      memcpy(embree_triangles, shape.triangles.data(),
          shape.triangles.size() * 12);
    } else if (!shape.quads.empty()) {
      embree_geom = rtcNewGeometry(get_embree_device(), RTC_GEOMETRY_TYPE_QUAD);
      rtcSetGeometryVertexAttributeCount(embree_geom, 1);
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          transformed_positions.size());
      auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
          shape.quads.size());
      memcpy(embree_positions, transformed_positions.data(),
          transformed_positions.size() * 12);
      memcpy(embree_quads, shape.quads.data(), shape.quads.size() * 16);
    } else if (!shape.quadspos.empty()) {
      embree_geom = rtcNewGeometry(get_embree_device(), RTC_GEOMETRY_TYPE_QUAD);
      rtcSetGeometryVertexAttributeCount(embree_geom, 1);
      auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
          transformed_positions.size());
      auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
          RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
          shape.quadspos.size());
      memcpy(embree_positions, transformed_positions.data(),
          transformed_positions.size() * 12);
      memcpy(embree_quads, shape.quadspos.data(), shape.quadspos.size() * 16);
    } else {
      throw std::runtime_error("empty bvh");
    }
    rtcCommitGeometry(embree_geom);
    rtcAttachGeometryByID(embree_scene, embree_geom, instance_id);
  }
  rtcCommitScene(embree_scene);
  scene.embree_flattened = true;
}
// Refit a BVH using Embree. Calls `refit_bvh()` if Embree is not
// available.
static void refit_embree_bvh(bvh_shape& bvh) {
  throw std::runtime_error("not yet implemented");
}
static bool intersect_embree_bvh(const bvh_shape& shape, const ray3f& ray,
    int& element, vec2f& uv, float& distance, bool find_any) {
  RTCRayHit embree_ray;
  embree_ray.ray.org_x     = ray.o.x;
  embree_ray.ray.org_y     = ray.o.y;
  embree_ray.ray.org_z     = ray.o.z;
  embree_ray.ray.dir_x     = ray.d.x;
  embree_ray.ray.dir_y     = ray.d.y;
  embree_ray.ray.dir_z     = ray.d.z;
  embree_ray.ray.tnear     = ray.tmin;
  embree_ray.ray.tfar      = ray.tmax;
  embree_ray.ray.flags     = 0;
  embree_ray.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
  embree_ray.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
  RTCIntersectContext embree_ctx;
  rtcInitIntersectContext(&embree_ctx);
  rtcIntersect1((RTCScene)shape.embree_bvh, &embree_ctx, &embree_ray);
  if (embree_ray.hit.geomID == RTC_INVALID_GEOMETRY_ID) return false;
  element  = (int)embree_ray.hit.primID;
  uv       = {embree_ray.hit.u, embree_ray.hit.v};
  distance = embree_ray.ray.tfar;
  return true;
}
static bool intersect_embree_bvh(const bvh_scene& scene, const ray3f& ray,
    int& instance, int& element, vec2f& uv, float& distance, bool find_any) {
  RTCRayHit embree_ray;
  embree_ray.ray.org_x     = ray.o.x;
  embree_ray.ray.org_y     = ray.o.y;
  embree_ray.ray.org_z     = ray.o.z;
  embree_ray.ray.dir_x     = ray.d.x;
  embree_ray.ray.dir_y     = ray.d.y;
  embree_ray.ray.dir_z     = ray.d.z;
  embree_ray.ray.tnear     = ray.tmin;
  embree_ray.ray.tfar      = ray.tmax;
  embree_ray.ray.flags     = 0;
  embree_ray.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
  embree_ray.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
  RTCIntersectContext embree_ctx;
  rtcInitIntersectContext(&embree_ctx);
  rtcIntersect1((RTCScene)scene.embree_bvh, &embree_ctx, &embree_ray);
  if (embree_ray.hit.geomID == RTC_INVALID_GEOMETRY_ID) return false;
  instance = scene.embree_flattened ? (int)embree_ray.hit.geomID
                                    : (int)embree_ray.hit.instID[0];
  element  = (int)embree_ray.hit.primID;
  uv       = {embree_ray.hit.u, embree_ray.hit.v};
  distance = embree_ray.ray.tfar;
  return true;
}
#endif

// BVH primitive with its bbox, its center and the index to the primitive
struct bvh_prim {
  bbox3f bbox   = invalidb3f;
  vec3f  center = zero3f;
  int    primid = 0;
};

// Splits a BVH node using the SAH heuristic. Returns split position and axis.
static pair<int, int> split_sah(vector<bvh_prim>& prims, int start, int end) {
  // initialize split axis and position
  auto split_axis = 0;
  auto mid        = (start + end) / 2;

  // compute primintive bounds and size
  auto cbbox = invalidb3f;
  for (auto i = start; i < end; i++) cbbox = merge(cbbox, prims[i].center);
  auto csize = cbbox.max - cbbox.min;
  if (csize == zero3f) return {mid, split_axis};

  // consider N bins, compute their cost and keep the minimum
  const int nbins    = 16;
  auto      middle   = 0.0f;
  auto      min_cost = flt_max;
  auto      area     = [](auto& b) {
    auto size = b.max - b.min;
    return 1e-12f + 2 * size.x * size.y + 2 * size.x * size.z +
           2 * size.y * size.z;
  };
  for (auto saxis = 0; saxis < 3; saxis++) {
    for (auto b = 1; b < nbins; b++) {
      auto split     = cbbox.min[saxis] + b * csize[saxis] / nbins;
      auto left_bbox = invalidb3f, right_bbox = invalidb3f;
      auto left_nprims = 0, right_nprims = 0;
      for (auto i = start; i < end; i++) {
        if (prims[i].center[saxis] < split) {
          left_bbox = merge(left_bbox, prims[i].bbox);
          left_nprims += 1;
        } else {
          right_bbox = merge(right_bbox, prims[i].bbox);
          right_nprims += 1;
        }
      }
      auto cost = 1 + left_nprims * area(left_bbox) / area(cbbox) +
                  right_nprims * area(right_bbox) / area(cbbox);
      if (cost < min_cost) {
        min_cost   = cost;
        middle     = split;
        split_axis = saxis;
      }
    }
  }
  // split
  mid = (int)(std::partition(prims.data() + start, prims.data() + end,
                  [split_axis, middle](
                      auto& a) { return a.center[split_axis] < middle; }) -
              prims.data());

  // if we were not able to split, just break the primitives in half
  if (mid == start || mid == end) {
    throw std::runtime_error("bad bvh split");
    split_axis = 0;
    mid        = (start + end) / 2;
  }

  return {mid, split_axis};
}

// Splits a BVH node using the balance heuristic. Returns split position and
// axis.
static pair<int, int> split_balanced(
    vector<bvh_prim>& prims, int start, int end) {
  // initialize split axis and position
  auto axis = 0;
  auto mid  = (start + end) / 2;

  // compute primintive bounds and size
  auto cbbox = invalidb3f;
  for (auto i = start; i < end; i++) cbbox = merge(cbbox, prims[i].center);
  auto csize = cbbox.max - cbbox.min;
  if (csize == zero3f) return {mid, axis};

  // split along largest
  if (csize.x >= csize.y && csize.x >= csize.z) axis = 0;
  if (csize.y >= csize.x && csize.y >= csize.z) axis = 1;
  if (csize.z >= csize.x && csize.z >= csize.y) axis = 2;

  // balanced tree split: find the largest axis of the
  // bounding box and split along this one right in the middle
  mid = (start + end) / 2;
  std::nth_element(prims.data() + start, prims.data() + mid, prims.data() + end,
      [axis](auto& a, auto& b) { return a.center[axis] < b.center[axis]; });

  // if we were not able to split, just break the primitives in half
  if (mid == start || mid == end) {
    throw std::runtime_error("bad bvh split");
    axis = 0;
    mid  = (start + end) / 2;
  }

  return {mid, axis};
}

// Splits a BVH node using the middle heutirtic. Returns split position and
// axis.
static pair<int, int> split_middle(
    vector<bvh_prim>& prims, int start, int end) {
  // initialize split axis and position
  auto axis = 0;
  auto mid  = (start + end) / 2;

  // compute primintive bounds and size
  auto cbbox = invalidb3f;
  for (auto i = start; i < end; i++) cbbox = merge(cbbox, prims[i].center);
  auto csize = cbbox.max - cbbox.min;
  if (csize == zero3f) return {mid, axis};

  // split along largest
  if (csize.x >= csize.y && csize.x >= csize.z) axis = 0;
  if (csize.y >= csize.x && csize.y >= csize.z) axis = 1;
  if (csize.z >= csize.x && csize.z >= csize.y) axis = 2;

  // split the space in the middle along the largest axis
  auto cmiddle = (cbbox.max + cbbox.min) / 2;
  auto middle  = cmiddle[axis];
  mid          = (int)(std::partition(prims.data() + start, prims.data() + end,
                  [axis, middle](auto& a) { return a.center[axis] < middle; }) -
              prims.data());

  // if we were not able to split, just break the primitives in half
  if (mid == start || mid == end) {
    throw std::runtime_error("bad bvh split");
    axis = 0;
    mid  = (start + end) / 2;
  }

  return {mid, axis};
}

// Build BVH nodes
static void build_bvh_serial(vector<bvh_node>& nodes, vector<bvh_prim>& prims,
    const bvh_params& params) {
  // prepare to build nodes
  nodes.clear();
  nodes.reserve(prims.size() * 2);

  // queue up first node
  auto queue = std::deque<vec3i>{{0, 0, (int)prims.size()}};
  nodes.emplace_back();

  // create nodes until the queue is empty
  while (!queue.empty()) {
    // exit if needed
    if (params.cancel && *params.cancel) return;

    // grab node to work on
    auto next = queue.front();
    queue.pop_front();
    auto nodeid = next.x, start = next.y, end = next.z;

    // grab node
    auto& node = nodes[nodeid];

    // compute bounds
    node.bbox = invalidb3f;
    for (auto i = start; i < end; i++)
      node.bbox = merge(node.bbox, prims[i].bbox);

    // split into two children
    if (end - start > bvh_max_prims) {
      // get split
      auto [mid, axis] = (params.high_quality)
                             ? split_sah(prims, start, end)
                             : split_balanced(prims, start, end);

      // make an internal node
      node.internal = true;
      node.axis     = axis;
      node.num      = 2;
      node.prims[0] = (int)nodes.size() + 0;
      node.prims[1] = (int)nodes.size() + 1;
      nodes.emplace_back();
      nodes.emplace_back();
      queue.push_back({node.prims[0], start, mid});
      queue.push_back({node.prims[1], mid, end});
    } else {
      // Make a leaf node
      node.internal = false;
      node.num      = end - start;
      for (auto i = 0; i < node.num; i++)
        node.prims[i] = prims[start + i].primid;
    }
  }

  // cleanup
  nodes.shrink_to_fit();
}

// Build BVH nodes
static void build_bvh_parallel(vector<bvh_node>& nodes, vector<bvh_prim>& prims,
    const bvh_params& params) {
  // prepare to build nodes
  nodes.clear();
  nodes.reserve(prims.size() * 2);

  // queue up first node
  auto queue = std::deque<vec3i>{{0, 0, (int)prims.size()}};
  nodes.emplace_back();

  // synchronization
  std::atomic<int>          num_processed_prims(0);
  std::mutex                queue_mutex;
  vector<std::future<void>> futures;
  auto                      nthreads = std::thread::hardware_concurrency();

  // create nodes until the queue is empty
  for (auto thread_id = 0; thread_id < nthreads; thread_id++) {
    futures.emplace_back(std::async(std::launch::async,
        [&nodes, &prims, &params, &num_processed_prims, &queue_mutex, &queue] {
          while (true) {
            // exit if needed
            if (num_processed_prims >= prims.size()) return;
            if (params.cancel && *params.cancel) return;

            // grab node to work on
            auto next = zero3i;
            {
              std::lock_guard<std::mutex> lock{queue_mutex};
              if (!queue.empty()) {
                next = queue.front();
                queue.pop_front();
              }
            }

            // wait a bit if needed
            if (next == zero3i) {
              std::this_thread::sleep_for(std::chrono::microseconds(10));
              continue;
            }

            // grab node
            auto  nodeid = next.x, start = next.y, end = next.z;
            auto& node = nodes[nodeid];

            // compute bounds
            node.bbox = invalidb3f;
            for (auto i = start; i < end; i++)
              node.bbox = merge(node.bbox, prims[i].bbox);

            // split into two children
            if (end - start > bvh_max_prims) {
              // get split
              auto [mid, axis] = (params.high_quality)
                                     ? split_sah(prims, start, end)
                                     : split_balanced(prims, start, end);

              // make an internal node
              {
                std::lock_guard<std::mutex> lock{queue_mutex};
                node.internal = true;
                node.axis     = axis;
                node.num      = 2;
                node.prims[0] = (int)nodes.size() + 0;
                node.prims[1] = (int)nodes.size() + 1;
                nodes.emplace_back();
                nodes.emplace_back();
                queue.push_back({node.prims[0], start, mid});
                queue.push_back({node.prims[1], mid, end});
              }
            } else {
              // Make a leaf node
              node.internal = false;
              node.num      = end - start;
              for (auto i = 0; i < node.num; i++)
                node.prims[i] = prims[start + i].primid;
              num_processed_prims += node.num;
            }
          }
        }));
  }
  for (auto& f : futures) f.get();

  // cleanup
  nodes.shrink_to_fit();
}

void build_bvh(bvh_shape& shape, const bvh_params& params) {
#if YOCTO_EMBREE
  // call Embree if needed
  if (params.use_embree) {
    return build_embree_bvh(shape, params);
  }
#endif

  // build primitives
  auto prims = vector<bvh_prim>{};
  if (!shape.points.empty()) {
    prims = vector<bvh_prim>(shape.points.size());
    for (auto idx = 0; idx < prims.size(); idx++) {
      auto& p    = shape.points[idx];
      auto  bbox = point_bounds(shape.positions[p], shape.radius[p]);
      prims[idx] = {bbox, center(bbox), idx};
    }
  } else if (!shape.lines.empty()) {
    prims = vector<bvh_prim>(shape.lines.size());
    for (auto idx = 0; idx < prims.size(); idx++) {
      auto& l    = shape.lines[idx];
      auto  bbox = line_bounds(shape.positions[l.x], shape.positions[l.y],
          shape.radius[l.x], shape.radius[l.y]);
      prims[idx] = {bbox, center(bbox), idx};
    }
  } else if (!shape.triangles.empty()) {
    prims = vector<bvh_prim>(shape.triangles.size());
    for (auto idx = 0; idx < prims.size(); idx++) {
      auto& t    = shape.triangles[idx];
      auto  bbox = triangle_bounds(
          shape.positions[t.x], shape.positions[t.y], shape.positions[t.z]);
      prims[idx] = {bbox, center(bbox), idx};
    }
  } else if (!shape.quads.empty()) {
    prims = vector<bvh_prim>(shape.quads.size());
    for (auto idx = 0; idx < prims.size(); idx++) {
      auto& q    = shape.quads[idx];
      auto  bbox = quad_bounds(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w]);
      prims[idx] = {bbox, center(bbox), idx};
    }
  } else if (!shape.quadspos.empty()) {
    prims = vector<bvh_prim>(shape.quadspos.size());
    for (auto idx = 0; idx < prims.size(); idx++) {
      auto& q    = shape.quadspos[idx];
      auto  bbox = quad_bounds(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w]);
      prims[idx] = {bbox, center(bbox), idx};
    }
  } else {
  }

  // build nodes
  if (params.noparallel) {
    build_bvh_serial(shape.nodes, prims, params);
  } else {
    build_bvh_parallel(shape.nodes, prims, params);
  }
}
void build_bvh(bvh_scene& scene, const bvh_params& params) {
  for (auto idx = 0; idx < scene.shapes.size(); idx++) {
    build_bvh(scene.shapes[idx], params);
  }

  // embree
#if YOCTO_EMBREE
  if (params.use_embree) {
    if (params.embree_flatten) {
      return build_embree_flattened_bvh(scene, params);
    } else {
      return build_embree_bvh(scene, params);
    }
  }
#endif

  // build primitives
  auto prims = vector<bvh_prim>(scene.instances.size());
  for (auto idx = 0; idx < prims.size(); idx++) {
    auto& instance = scene.instances[idx];
    auto& sbvh     = scene.shapes[instance.shape];
    auto  bbox     = sbvh.nodes.empty()
                    ? invalidb3f
                    : transform_bbox(instance.frame, sbvh.nodes[0].bbox);
    prims[idx] = {bbox, center(bbox), idx};
  }

  // build nodes
  if (params.noparallel) {
    build_bvh_serial(scene.nodes, prims, params);
  } else {
    build_bvh_parallel(scene.nodes, prims, params);
  }
}

void refit_bvh(bvh_shape& shape, const bvh_params& params) {
#if YOCTO_EMBREE
  if (shape.embree_bvh) throw std::runtime_error("Embree reftting disabled");
#endif

  // refit
  for (auto nodeid = (int)shape.nodes.size() - 1; nodeid >= 0; nodeid--) {
    auto& node = shape.nodes[nodeid];
    node.bbox  = invalidb3f;
    if (node.internal) {
      for (auto i = 0; i < 2; i++) {
        node.bbox = merge(node.bbox, shape.nodes[node.prims[i]].bbox);
      }
    } else if (!shape.points.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& p   = shape.points[node.prims[idx]];
        node.bbox = merge(
            node.bbox, point_bounds(shape.positions[p], shape.radius[p]));
      }
    } else if (!shape.lines.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& l   = shape.lines[node.prims[idx]];
        node.bbox = merge(
            node.bbox, line_bounds(shape.positions[l.x], shape.positions[l.y],
                           shape.radius[l.x], shape.radius[l.y]));
      }
    } else if (!shape.triangles.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& t   = shape.triangles[node.prims[idx]];
        node.bbox = merge(
            node.bbox, triangle_bounds(shape.positions[t.x],
                           shape.positions[t.y], shape.positions[t.z]));
      }
    } else if (!shape.quads.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q   = shape.quads[node.prims[idx]];
        node.bbox = merge(
            node.bbox, quad_bounds(shape.positions[q.x], shape.positions[q.y],
                           shape.positions[q.z], shape.positions[q.w]));
      }
    } else if (!shape.quadspos.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q   = shape.quadspos[node.prims[idx]];
        node.bbox = merge(
            node.bbox, quad_bounds(shape.positions[q.x], shape.positions[q.y],
                           shape.positions[q.z], shape.positions[q.w]));
      }
    }
  }
}

void refit_bvh(bvh_scene& scene, const vector<int>& updated_shapes,
    const bvh_params& params) {
  // update shapes
  for (auto shape : updated_shapes) refit_bvh(scene.shapes[shape], params);

#if YOCTO_EMBREE
  if (scene.embree_bvh) throw std::runtime_error("Embree reftting disabled");
#endif

  // refit
  for (auto nodeid = (int)scene.nodes.size() - 1; nodeid >= 0; nodeid--) {
    auto& node = scene.nodes[nodeid];
    node.bbox  = invalidb3f;
    if (node.internal) {
      for (auto i = 0; i < 2; i++) {
        node.bbox = merge(node.bbox, scene.nodes[node.prims[i]].bbox);
      }
    } else {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& instance = scene.instances[idx];
        auto& sbvh     = scene.shapes[instance.shape];
        auto  bbox     = sbvh.nodes.empty()
                        ? invalidb3f
                        : transform_bbox(instance.frame, sbvh.nodes[0].bbox);
        node.bbox = merge(node.bbox, bbox);
      }
    }
  }
}

// Intersect ray with a bvh.
bool intersect_bvh(const bvh_shape& shape, const ray3f& ray_, int& element,
    vec2f& uv, float& distance, bool find_any) {
#if YOCTO_EMBREE
  // call Embree if needed
  if (shape.embree_bvh) {
    return intersect_embree_bvh(shape, ray_, element, uv, distance, find_any);
  }
#endif

  // check empty
  if (shape.nodes.empty()) return false;

  // node stack
  int  node_stack[128];
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // shared variables
  auto hit = false;

  // copy ray to modify it
  auto ray = ray_;

  // prepare ray for fast queries
  auto ray_dinv  = vec3f{1 / ray.d.x, 1 / ray.d.y, 1 / ray.d.z};
  auto ray_dsign = vec3i{(ray_dinv.x < 0) ? 1 : 0, (ray_dinv.y < 0) ? 1 : 0,
      (ray_dinv.z < 0) ? 1 : 0};

  // walking stack
  while (node_cur) {
    // grab node
    auto& node = shape.nodes[node_stack[--node_cur]];

    // intersect bbox
    // if (!intersect_bbox(ray, ray_dinv, ray_dsign, node.bbox)) continue;
    if (!intersect_bbox(ray, ray_dinv, node.bbox)) continue;

    // intersect node, switching based on node type
    // for each type, iterate over the the primitive list
    if (node.internal) {
      // for internal nodes, attempts to proceed along the
      // split axis from smallest to largest nodes
      if (ray_dsign[node.axis]) {
        node_stack[node_cur++] = node.prims[0];
        node_stack[node_cur++] = node.prims[1];
      } else {
        node_stack[node_cur++] = node.prims[1];
        node_stack[node_cur++] = node.prims[0];
      }
    } else if (!shape.points.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& p = shape.points[node.prims[idx]];
        if (intersect_point(
                ray, shape.positions[p], shape.radius[p], uv, distance)) {
          hit      = true;
          element  = node.prims[idx];
          ray.tmax = distance;
        }
      }
    } else if (!shape.lines.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& l = shape.lines[node.prims[idx]];
        if (intersect_line(ray, shape.positions[l.x], shape.positions[l.y],
                shape.radius[l.x], shape.radius[l.y], uv, distance)) {
          hit      = true;
          element  = node.prims[idx];
          ray.tmax = distance;
        }
      }
    } else if (!shape.triangles.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& t = shape.triangles[node.prims[idx]];
        if (intersect_triangle(ray, shape.positions[t.x], shape.positions[t.y],
                shape.positions[t.z], uv, distance)) {
          hit      = true;
          element  = node.prims[idx];
          ray.tmax = distance;
        }
      }
    } else if (!shape.quads.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q = shape.quads[node.prims[idx]];
        if (intersect_quad(ray, shape.positions[q.x], shape.positions[q.y],
                shape.positions[q.z], shape.positions[q.w], uv, distance)) {
          hit      = true;
          element  = node.prims[idx];
          ray.tmax = distance;
        }
      }
    } else if (!shape.quadspos.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q = shape.quadspos[node.prims[idx]];
        if (intersect_quad(ray, shape.positions[q.x], shape.positions[q.y],
                shape.positions[q.z], shape.positions[q.w], uv, distance)) {
          hit      = true;
          element  = node.prims[idx];
          ray.tmax = distance;
        }
      }
    }

    // check for early exit
    if (find_any && hit) return hit;
  }

  return hit;
}

// Intersect ray with a bvh.
bool intersect_bvh(const bvh_scene& scene, const ray3f& ray_, int& instance,
    int& element, vec2f& uv, float& distance, bool find_any,
    bool non_rigid_frames) {
#if YOCTO_EMBREE
  // call Embree if needed
  if (scene.embree_bvh) {
    return intersect_embree_bvh(
        scene, ray_, instance, element, uv, distance, find_any);
  }
#endif

  // check empty
  if (scene.nodes.empty()) return false;

  // node stack
  int  node_stack[128];
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // shared variables
  auto hit = false;

  // copy ray to modify it
  auto ray = ray_;

  // prepare ray for fast queries
  auto ray_dinv  = vec3f{1 / ray.d.x, 1 / ray.d.y, 1 / ray.d.z};
  auto ray_dsign = vec3i{(ray_dinv.x < 0) ? 1 : 0, (ray_dinv.y < 0) ? 1 : 0,
      (ray_dinv.z < 0) ? 1 : 0};

  // walking stack
  while (node_cur) {
    // grab node
    auto& node = scene.nodes[node_stack[--node_cur]];

    // intersect bbox
    // if (!intersect_bbox(ray, ray_dinv, ray_dsign, node.bbox)) continue;
    if (!intersect_bbox(ray, ray_dinv, node.bbox)) continue;

    // intersect node, switching based on node type
    // for each type, iterate over the the primitive list
    if (node.internal) {
      // for internal nodes, attempts to proceed along the
      // split axis from smallest to largest nodes
      if (ray_dsign[node.axis]) {
        node_stack[node_cur++] = node.prims[0];
        node_stack[node_cur++] = node.prims[1];
      } else {
        node_stack[node_cur++] = node.prims[1];
        node_stack[node_cur++] = node.prims[0];
      }
    } else {
      for (auto i = 0; i < node.num; i++) {
        auto& instance_ = scene.instances[node.prims[i]];
        auto  inv_ray   = transform_ray(
            inverse(instance_.frame, non_rigid_frames), ray);
        if (intersect_bvh(scene.shapes[instance_.shape], inv_ray, element, uv,
                distance, find_any)) {
          hit      = true;
          instance = node.prims[i];
          ray.tmax = distance;
        }
      }
    }

    // check for early exit
    if (find_any && hit) return hit;
  }

  return hit;
}
// Intersect ray with a bvh.
bool intersect_bvh(const bvh_scene& scene, int instance, const ray3f& ray,
    int& element, vec2f& uv, float& distance, bool find_any,
    bool non_rigid_frames) {
  auto& instance_ = scene.instances[instance];
  auto inv_ray = transform_ray(inverse(instance_.frame, non_rigid_frames), ray);
  return intersect_bvh(
      scene.shapes[instance_.shape], inv_ray, element, uv, distance, find_any);
}

// Intersect ray with a bvh.
bool overlap_bvh(const bvh_shape& shape, const vec3f& pos, float max_distance,
    int& element, vec2f& uv, float& distance, bool find_any) {
  // check if empty
  if (shape.nodes.empty()) return false;

  // node stack
  int  node_stack[64];
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // hit
  auto hit = false;

  // walking stack
  while (node_cur) {
    // grab node
    auto& node = shape.nodes[node_stack[--node_cur]];

    // intersect bbox
    if (!distance_check_bbox(pos, max_distance, node.bbox)) continue;

    // intersect node, switching based on node type
    // for each type, iterate over the the primitive list
    if (node.internal) {
      // internal node
      node_stack[node_cur++] = node.prims[0];
      node_stack[node_cur++] = node.prims[1];
    } else if (!shape.points.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& p = shape.points[node.prims[idx]];
        if (overlap_point(pos, max_distance, shape.positions[p],
                shape.radius[p], uv, distance)) {
          hit          = true;
          element      = node.prims[idx];
          max_distance = distance;
        }
      }
    } else if (!shape.lines.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& l = shape.lines[node.prims[idx]];
        if (overlap_line(pos, max_distance, shape.positions[l.x],
                shape.positions[l.y], shape.radius[l.x], shape.radius[l.y], uv,
                distance)) {
          hit          = true;
          element      = node.prims[idx];
          max_distance = distance;
        }
      }
    } else if (!shape.triangles.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& t = shape.triangles[node.prims[idx]];
        if (overlap_triangle(pos, max_distance, shape.positions[t.x],
                shape.positions[t.y], shape.positions[t.z], shape.radius[t.x],
                shape.radius[t.y], shape.radius[t.z], uv, distance)) {
          hit          = true;
          element      = node.prims[idx];
          max_distance = distance;
        }
      }
    } else if (!shape.quads.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q = shape.quads[node.prims[idx]];
        if (overlap_quad(pos, max_distance, shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], shape.radius[q.x], shape.radius[q.y],
                shape.radius[q.z], shape.radius[q.w], uv, distance)) {
          hit          = true;
          element      = node.prims[idx];
          max_distance = distance;
        }
      }
    } else if (!shape.quadspos.empty()) {
      for (auto idx = 0; idx < node.num; idx++) {
        auto& q = shape.quadspos[node.prims[idx]];
        if (overlap_quad(pos, max_distance, shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], shape.radius[q.x], shape.radius[q.y],
                shape.radius[q.z], shape.radius[q.w], uv, distance)) {
          hit          = true;
          element      = node.prims[idx];
          max_distance = distance;
        }
      }
    }

    // check for early exit
    if (find_any && hit) return hit;
  }

  return hit;
}

// Intersect ray with a bvh.
bool overlap_bvh(const bvh_scene& scene, const vec3f& pos, float max_distance,
    int& instance, int& element, vec2f& uv, float& distance, bool find_any,
    bool non_rigid_frames) {
  // check if empty
  if (scene.nodes.empty()) return false;

  // node stack
  int  node_stack[64];
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // hit
  auto hit = false;

  // walking stack
  while (node_cur) {
    // grab node
    auto& node = scene.nodes[node_stack[--node_cur]];

    // intersect bbox
    if (!distance_check_bbox(pos, max_distance, node.bbox)) continue;

    // intersect node, switching based on node type
    // for each type, iterate over the the primitive list
    if (node.internal) {
      // internal node
      node_stack[node_cur++] = node.prims[0];
      node_stack[node_cur++] = node.prims[1];
    } else {
      for (auto i = 0; i < node.num; i++) {
        auto instance_ = scene.instances[node.prims[i]];
        auto inv_pos   = transform_point(
            inverse(instance_.frame, non_rigid_frames), pos);
        if (overlap_bvh(scene.shapes[instance_.shape], inv_pos, max_distance,
                element, uv, distance, find_any)) {
          hit          = true;
          instance     = node.prims[i];
          max_distance = distance;
        }
      }
    }

    // check for early exit
    if (find_any && hit) return hit;
  }

  return hit;
}

#if 0
    // Finds the overlap between BVH leaf nodes.
    template <typename OverlapElem>
    void overlap_bvh_elems(const bvh_scene_data& bvh1, const bvh_scene_data& bvh2,
                           bool skip_duplicates, bool skip_self, vector<vec2i>& overlaps,
                           const OverlapElem& overlap_elems) {
        // node stack
        vec2i node_stack[128];
        auto node_cur = 0;
        node_stack[node_cur++] = {0, 0};

        // walking stack
        while (node_cur) {
            // grab node
            auto node_idx = node_stack[--node_cur];
            const auto node1 = bvh1->nodes[node_idx.x];
            const auto node2 = bvh2->nodes[node_idx.y];

            // intersect bbox
            if (!overlap_bbox(node1.bbox, node2.bbox)) continue;

            // check for leaves
            if (node1.isleaf && node2.isleaf) {
                // collide primitives
                for (auto i1 = node1.start; i1 < node1.start + node1.count; i1++) {
                    for (auto i2 = node2.start; i2 < node2.start + node2.count;
                         i2++) {
                        auto idx1 = bvh1->sorted_prim[i1];
                        auto idx2 = bvh2->sorted_prim[i2];
                        if (skip_duplicates && idx1 > idx2) continue;
                        if (skip_self && idx1 == idx2) continue;
                        if (overlap_elems(idx1, idx2))
                            overlaps.push_back({idx1, idx2});
                    }
                }
            } else {
                // descend
                if (node1.isleaf) {
                    for (auto idx2 = node2.start; idx2 < node2.start + node2.count;
                         idx2++) {
                        node_stack[node_cur++] = {node_idx.x, (int)idx2};
                    }
                } else if (node2.isleaf) {
                    for (auto idx1 = node1.start; idx1 < node1.start + node1.count;
                         idx1++) {
                        node_stack[node_cur++] = {(int)idx1, node_idx.y};
                    }
                } else {
                    for (auto idx2 = node2.start; idx2 < node2.start + node2.count;
                         idx2++) {
                        for (auto idx1 = node1.start;
                             idx1 < node1.start + node1.count; idx1++) {
                            node_stack[node_cur++] = {(int)idx1, (int)idx2};
                        }
                    }
                }
            }
        }
    }
#endif

bvh_intersection intersect_bvh(
    const bvh_shape& shape, const ray3f& ray, bool find_any) {
  auto intersection = bvh_intersection{};
  intersection.hit  = intersect_bvh(shape, ray, intersection.element,
      intersection.uv, intersection.distance, find_any);
  return intersection;
}
bvh_intersection intersect_bvh(const bvh_scene& scene, const ray3f& ray,
    bool find_any, bool non_rigid_frames) {
  auto intersection = bvh_intersection{};
  intersection.hit  = intersect_bvh(scene, ray, intersection.instance,
      intersection.element, intersection.uv, intersection.distance, find_any);
  return intersection;
}
bvh_intersection intersect_bvh(const bvh_scene& scene, int instance,
    const ray3f& ray, bool find_any, bool non_rigid_frames) {
  auto intersection = bvh_intersection{};
  intersection.hit  = intersect_bvh(scene, instance, ray, intersection.element,
      intersection.uv, intersection.distance, find_any);
  intersection.instance = instance;
  return intersection;
}

bvh_intersection overlap_bvh(const bvh_shape& shape, const vec3f& pos,
    float max_distance, bool find_any) {
  auto intersection = bvh_intersection{};
  intersection.hit = overlap_bvh(shape, pos, max_distance, intersection.element,
      intersection.uv, intersection.distance, find_any);
  return intersection;
}
bvh_intersection overlap_bvh(const bvh_scene& scene, const vec3f& pos,
    float max_distance, bool find_any, bool non_rigid_frames) {
  auto intersection = bvh_intersection{};
  intersection.hit  = overlap_bvh(scene, pos, max_distance,
      intersection.instance, intersection.element, intersection.uv,
      intersection.distance, find_any);
  return intersection;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF BVH UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Print bvh statistics.
string format_stats(const bvh_shape& bvh) {
  // TODO
  auto str = ""s;
  return str;
}
string format_stats(const bvh_scene& bvh) {
#if 0
    auto num_shapes    = (size_t)0;
    auto num_instances = (size_t)0;

    auto elem_points    = (size_t)0;
    auto elem_lines     = (size_t)0;
    auto elem_triangles = (size_t)0;
    auto elem_quads     = (size_t)0;

    auto vert_pos    = (size_t)0;
    auto vert_radius = (size_t)0;

    auto shape_nodes = (size_t)0;
    auto scene_nodes = (size_t)0;

    auto stored_elem_points    = (size_t)0;
    auto stored_elem_lines     = (size_t)0;
    auto stored_elem_triangles = (size_t)0;
    auto stored_elem_quads     = (size_t)0;

    auto stored_vert_pos    = (size_t)0;
    auto stored_vert_radius = (size_t)0;

    auto memory_elems = (size_t)0;
    auto memory_verts = (size_t)0;

    auto memory_ists = (size_t)0;

    auto memory_shape_nodes = (size_t)0;
    auto memory_scene_nodes = (size_t)0;

    for (auto& sbvh : bvh.shapes) {
        shape_nodes += sbvh.nodes.size();
    }

    num_shapes    = bvh.shapes.size();
    num_instances = bvh.instances.size();
    scene_nodes   = bvh.bvh_.nodes.size();

    memory_elems = stored_elem_points * sizeof(int) +
                   stored_elem_lines * sizeof(vec2i) +
                   stored_elem_triangles * sizeof(vec3i) +
                   stored_elem_quads * sizeof(vec4i);
    memory_verts = stored_vert_pos * sizeof(vec3f) +
                   stored_vert_radius * sizeof(float);

    memory_ists = num_instances * sizeof(bvh_instance);

    memory_shape_nodes = shape_nodes * sizeof(bvh_node);
    memory_scene_nodes = scene_nodes * sizeof(bvh_node);

    auto str = ""s;

    str += "num_shapes: " + std::to_string(num_shapes) + "\n";
    str += "num_instances: " + std::to_string(num_instances) + "\n";

    str += "elem_points: " + std::to_string(elem_points) + "\n";
    str += "elem_lines: " + std::to_string(elem_lines) + "\n";
    str += "elem_triangles: " + std::to_string(elem_triangles) + "\n";
    str += "elem_quads: " + std::to_string(elem_quads) + "\n";
    str += "vert_pos: " + std::to_string(vert_pos) + "\n";
    str += "vert_radius: " + std::to_string(vert_radius) + "\n";

    str += "shape_nodes: " + std::to_string(shape_nodes) + "\n";
    str += "scene_nodes: " + std::to_string(scene_nodes) + "\n";

    str += "memory_elems: " + std::to_string(memory_elems) + "\n";
    str += "memory_verts: " + std::to_string(memory_verts) + "\n";
    str += "memory_ists: " + std::to_string(memory_ists) + "\n";
    str += "memory_shape_nodes: " + std::to_string(memory_shape_nodes) + "\n";
    str += "memory_scene_nodes: " + std::to_string(memory_scene_nodes) + "\n";

#if YOCTO_EMBREE
    str += "memory_embree: " + std::to_string(embree_memory) + "\n";
#endif
#endif
  // TODO
  auto str = ""s;
  return str;
}

}  // namespace yocto
