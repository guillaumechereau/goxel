//
// Implementation for Yocto/BVH.
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

#include "yocto_bvh.h"
#include "yocto_utils.h"

#if YOCTO_EMBREE
#include <embree3/rtcore.h>
#endif

// -----------------------------------------------------------------------------
// IMPLEMENRTATION OF RAY-PRIMITIVE INTERSECTION FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Intersect a ray with a point (approximate)
bvh_element_intersection intersect_point(
    const ray3f& ray, const vec3f& p, float r) {
    // find parameter for line-point minimum distance
    auto w = p - ray.o;
    auto t = dot(w, ray.d) / dot(ray.d, ray.d);

    // exit if not within bounds
    if (t < ray.tmin || t > ray.tmax) return {};

    // test for line-point distance vs point radius
    auto rp  = ray.o + ray.d * t;
    auto prp = p - rp;
    if (dot(prp, prp) > r * r) return {};

    // intersection occurred: set options and exit
    return {{0, 0}, t, true};
}

// Intersect a ray with a line
bvh_element_intersection intersect_line(
    const ray3f& ray, const vec3f& p0, const vec3f& p1, float r0, float r1) {
    // setup intersection options
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
    if (det == 0) return {};

    // compute Parameters on both ray and segment
    auto t = (b * e - c * d) / det;
    auto s = (a * e - b * d) / det;

    // exit if not within bounds
    if (t < ray.tmin || t > ray.tmax) return {};

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

    // intersection occurred: set options and exit
    return {{s, sqrt(d2) / r}, t, true};
}

// Intersect a ray with a triangle
bvh_element_intersection intersect_triangle(
    const ray3f& ray, const vec3f& p0, const vec3f& p1, const vec3f& p2) {
    // compute triangle edges
    auto edge1 = p1 - p0;
    auto edge2 = p2 - p0;

    // compute determinant to solve a linear system
    auto pvec = cross(ray.d, edge2);
    auto det  = dot(edge1, pvec);

    // check determinant and exit if triangle and ray are parallel
    // (could use EPSILONS if desired)
    if (det == 0) return {};
    auto inv_det = 1.0f / det;

    // compute and check first bricentric coordinated
    auto tvec = ray.o - p0;
    auto u    = dot(tvec, pvec) * inv_det;
    if (u < 0 || u > 1) return {};

    // compute and check second bricentric coordinated
    auto qvec = cross(tvec, edge1);
    auto v    = dot(ray.d, qvec) * inv_det;
    if (v < 0 || u + v > 1) return {};

    // compute and check ray parameter
    auto t = dot(edge2, qvec) * inv_det;
    if (t < ray.tmin || t > ray.tmax) return {};

    // intersection occurred: set options and exit
    return {{u, v}, t, true};
}

// Intersect a ray with a quad.
bvh_element_intersection intersect_quad(const ray3f& ray, const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec3f& p3) {
    auto intersection  = bvh_element_intersection{};
    auto tray          = ray;
    auto intersection1 = intersect_triangle(tray, p0, p1, p3);
    if (intersection1.hit) {
        intersection = intersection1;
        tray.tmax    = intersection.distance;
    }
    auto intersection2 = intersect_triangle(tray, p2, p3, p1);
    if (intersection2.hit) {
        intersection            = intersection2;
        intersection.element_uv = {
            1 - intersection.element_uv.x, 1 - intersection.element_uv.y};
        tray.tmax = intersection.distance;
    }
    return intersection;
}

// Min/max used in BVH traversal. Copied here since the traversal code
// relies on the specific behaviour wrt NaNs.
static inline const float& _safemin(const float& a, const float& b) {
    return (a < b) ? a : b;
}
// Min/max used in BVH traversal. Copied here since the traversal code
// relies on the specific behaviour wrt NaNs.
static inline const float& _safemax(const float& a, const float& b) {
    return (a > b) ? a : b;
}

// Intersect a ray with a axis-aligned bounding box
bool intersect_bbox(const ray3f& ray, const bbox3f& bbox) {
    // determine intersection ranges
    auto invd = 1.0f / ray.d;
    auto t0   = (bbox.min - ray.o) * invd;
    auto t1   = (bbox.max - ray.o) * invd;
    // flip based on range directions
    if (invd.x < 0.0f) swap(t0.x, t1.x);
    if (invd.y < 0.0f) swap(t0.y, t1.y);
    if (invd.z < 0.0f) swap(t0.z, t1.z);
    auto tmin = _safemax(t0.z, _safemax(t0.y, _safemax(t0.x, ray.tmin)));
    auto tmax = _safemin(t1.z, _safemin(t1.y, _safemin(t1.x, ray.tmax)));
    tmax *= 1.00000024f;  // for double: 1.0000000000000004
    return tmin <= tmax;
}

// Intersect a ray with a axis-aligned bounding box
bool intersect_bbox(const ray3f& ray, const vec3f& ray_dinv,
    const vec3i& ray_dsign, const bbox3f& bbox_) {
    auto bbox  = &bbox_.min;
    auto txmin = (bbox[ray_dsign.x].x - ray.o.x) * ray_dinv.x;
    auto txmax = (bbox[1 - ray_dsign.x].x - ray.o.x) * ray_dinv.x;
    auto tymin = (bbox[ray_dsign.y].y - ray.o.y) * ray_dinv.y;
    auto tymax = (bbox[1 - ray_dsign.y].y - ray.o.y) * ray_dinv.y;
    auto tzmin = (bbox[ray_dsign.z].z - ray.o.z) * ray_dinv.z;
    auto tzmax = (bbox[1 - ray_dsign.z].z - ray.o.z) * ray_dinv.z;
    auto tmin  = _safemax(tzmin, _safemax(tymin, _safemax(txmin, ray.tmin)));
    auto tmax  = _safemin(tzmax, _safemin(tymax, _safemin(txmax, ray.tmax)));
    tmax *= 1.00000024f;  // for double: 1.0000000000000004
    return tmin <= tmax;
}

// Intersect a ray with a axis-aligned bounding box
bool intersect_bbox(
    const ray3f& ray, const vec3f& ray_dinv, const bbox3f& bbox) {
    auto it_min = (bbox.min - ray.o) * ray_dinv;
    auto it_max = (bbox.max - ray.o) * ray_dinv;
    auto tmin   = vec3f{_safemin(it_min.x, it_max.x),
        _safemin(it_min.y, it_max.y), _safemin(it_min.z, it_max.z)};
    auto tmax   = vec3f{_safemax(it_min.x, it_max.x),
        _safemax(it_min.y, it_max.y), _safemax(it_min.z, it_max.z)};
    auto t0 = _safemax(tmin.x, _safemax(tmin.y, _safemax(tmin.z, ray.tmin)));
    auto t1 = _safemin(tmax.x, _safemin(tmax.y, _safemin(tmax.z, ray.tmax)));
    t1 *= 1.00000024f;  // for double: 1.0000000000000004
    return t0 <= t1;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENRTATION OF POINT-PRIMITIVE DISTANCE FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// TODO: documentation
bvh_element_intersection overlap_point(
    const vec3f& pos, float dist_max, const vec3f& p, float r) {
    auto d2 = dot(pos - p, pos - p);
    if (d2 > (dist_max + r) * (dist_max + r)) return {};
    return {{0, 0}, sqrt(d2), true};
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
bvh_element_intersection overlap_line(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, float r0, float r1) {
    auto u = closestuv_line(pos, p0, p1);
    // Compute projected position from the clamped t d = a + t * ab;
    auto p  = p0 + (p1 - p0) * u;
    auto r  = r0 + (r1 - r0) * u;
    auto d2 = dot(pos - p, pos - p);
    // check distance
    if (d2 > (dist_max + r) * (dist_max + r)) return {};
    // done
    return {{u, 0}, sqrt(d2), true};
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
bvh_element_intersection overlap_triangle(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, const vec3f& p2, float r0, float r1,
    float r2) {
    auto uv = closestuv_triangle(pos, p0, p1, p2);
    auto p  = p0 * (1 - uv.x - uv.y) + p1 * uv.x + p2 * uv.y;
    auto r  = r0 * (1 - uv.x - uv.y) + r1 * uv.x + r2 * uv.y;
    auto dd = dot(p - pos, p - pos);
    if (dd > (dist_max + r) * (dist_max + r)) return {};
    return {uv, sqrt(dd), true};
}

// TODO: documentation
bvh_element_intersection overlap_quad(const vec3f& pos, float dist_max,
    const vec3f& p0, const vec3f& p1, const vec3f& p2, const vec3f& p3,
    float r0, float r1, float r2, float r3) {
    auto intersection  = bvh_element_intersection{};
    auto intersection1 = overlap_triangle(
        pos, dist_max, p0, p1, p3, r0, r1, r2);
    if (intersection1.hit) {
        intersection = intersection1;
        dist_max     = intersection.distance;
    }
    auto intersection2 = overlap_triangle(
        pos, dist_max, p2, p3, p1, r2, r3, r1);
    if (intersection2.hit) {
        intersection            = intersection2;
        intersection.element_uv = {
            1 - intersection.element_uv.x, 1 - intersection.element_uv.y};
        dist_max = intersection.distance;
    }
    return intersection;
}

// TODO: documentation
bool distance_check_bbox(const vec3f& pos, float dist_max, const bbox3f& bbox) {
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
bool overlap_bbox(const bbox3f& bbox1, const bbox3f& bbox2) {
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

// Cleanup
void clear_shape_bvh_embree(bvh_shape& bvh) {
#if YOCTO_EMBREE
    if (bvh.embree_bvh) {
        rtcReleaseScene((RTCScene)bvh.embree_bvh);
    }
#endif
}
void clear_scene_bvh_embree(bvh_scene& bvh) {
#if YOCTO_EMBREE
    if (bvh.embree_bvh) {
        for (auto i = 0; i < max(1, (int)bvh.instances.size()); i++) {
            auto geom = rtcGetGeometry((RTCScene)bvh.embree_bvh, i);
            rtcDetachGeometry((RTCScene)bvh.embree_bvh, i);
            rtcReleaseGeometry(geom);
        }
        rtcReleaseScene((RTCScene)bvh.embree_bvh);
    }
#endif
}

#if YOCTO_EMBREE
void embree_error(void* ctx, RTCError code, const char* str) {
    switch (code) {
        case RTC_ERROR_UNKNOWN:
            throw runtime_error("RTC_ERROR_UNKNOWN: "s + str);
            break;
        case RTC_ERROR_INVALID_ARGUMENT:
            throw runtime_error("RTC_ERROR_INVALID_ARGUMENT: "s + str);
            break;
        case RTC_ERROR_INVALID_OPERATION:
            throw runtime_error("RTC_ERROR_INVALID_OPERATION: "s + str);
            break;
        case RTC_ERROR_OUT_OF_MEMORY:
            throw runtime_error("RTC_ERROR_OUT_OF_MEMORY: "s + str);
            break;
        case RTC_ERROR_UNSUPPORTED_CPU:
            throw runtime_error("RTC_ERROR_UNSUPPORTED_CPU: "s + str);
            break;
        case RTC_ERROR_CANCELLED:
            throw runtime_error("RTC_ERROR_CANCELLED: "s + str);
            break;
        default: throw runtime_error("invalid error code"); break;
    }
}

// Get Embree device
RTCDevice get_embree_device() {
    static RTCDevice device = nullptr;
    if (!device) {
        device = rtcNewDevice("");
        rtcSetDeviceErrorFunction(device, embree_error, nullptr);
    }
    return device;
}

// unused Embree code kept in case we want to reactivate in the future
// void test_embree_shape_filter(const RTCFilterFunctionNArguments* args) {
//     auto& bvh = *(bvh_shape*)args->geometryUserPtr;
//     if(!bvh.intersection_filter) return;
//     auto element_uv = vec2f{RTCHitN_u(args->hit, args->N, 0),
//     RTCHitN_v(args->hit, args->N, 0)}; auto element_id =
//     (int)RTCHitN_primID(args->hit, args->N, 0); auto hit =
//     bvh.intersection_filter(element_id, element_uv); if(hit) args->valid[0] =
//     0;
// }

// Build a BVH using Embree.
void build_shape_embree_bvh(bvh_shape& bvh, const build_bvh_options& options) {
    auto embree_device = get_embree_device();
    auto embree_scene  = rtcNewScene(embree_device);
    // rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
    bvh.embree_bvh = embree_scene;
    if (!bvh.points.empty()) {
        throw runtime_error("embree does not support points");
    } else if (!bvh.lines.empty()) {
        throw runtime_error("not yet implemented");
    } else if (!bvh.triangles.empty()) {
        auto embree_geom = rtcNewGeometry(
            embree_device, RTC_GEOMETRY_TYPE_TRIANGLE);
        rtcSetGeometryVertexAttributeCount(embree_geom, 1);
        auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
            RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
            bvh.positions.size());
        auto embree_triangles = rtcSetNewGeometryBuffer(embree_geom,
            RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * 4,
            bvh.triangles.size());
        memcpy(
            embree_positions, bvh.positions.data(), bvh.positions.size() * 12);
        memcpy(
            embree_triangles, bvh.triangles.data(), bvh.triangles.size() * 12);
        rtcCommitGeometry(embree_geom);
        rtcAttachGeometryByID(embree_scene, embree_geom, 0);
    } else if (!bvh.quads.empty()) {
        auto embree_geom = rtcNewGeometry(
            embree_device, RTC_GEOMETRY_TYPE_QUAD);
        rtcSetGeometryVertexAttributeCount(embree_geom, 1);
        auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
            RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
            bvh.positions.size());
        auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
            RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
            bvh.quads.size());
        memcpy(
            embree_positions, bvh.positions.data(), bvh.positions.size() * 12);
        memcpy(embree_quads, bvh.quads.data(), bvh.quads.size() * 16);
        rtcCommitGeometry(embree_geom);
        rtcAttachGeometryByID(embree_scene, embree_geom, 0);
    } else {
        throw runtime_error("empty bvh");
    }
    rtcCommitScene(embree_scene);
}
void build_scene_embree_instanced_bvh(
    bvh_scene& bvh, const build_bvh_options& options) {
    // build shape and surface bvhs
    for (auto& shape_bvh : bvh.shape_bvhs)
        build_shape_embree_bvh(shape_bvh, options);

    // scene bvh
    auto embree_device = get_embree_device();
    auto embree_scene  = rtcNewScene(embree_device);
    // rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
    bvh.embree_bvh = embree_scene;
    if (bvh.instances.empty()) {
        rtcCommitScene(embree_scene);
        return;
    }
    for (auto instance_id = 0; instance_id < bvh.instances.size();
         instance_id++) {
        auto& instance = bvh.instances[instance_id];
        if (instance.shape_id < 0) continue;
        if (bvh.shape_bvhs[instance.shape_id].positions.empty()) continue;
        auto embree_geom = rtcNewGeometry(
            embree_device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embree_geom,
            (RTCScene)bvh.shape_bvhs[instance.shape_id].embree_bvh);
        rtcSetGeometryTransform(
            embree_geom, 0, RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR, &instance.frame);
        rtcCommitGeometry(embree_geom);
        rtcAttachGeometryByID(embree_scene, embree_geom, instance_id);
    }
    rtcCommitScene(embree_scene);
    bvh.embree_flattened = false;
}
void build_scene_embree_flattened_bvh(
    bvh_scene& bvh, const build_bvh_options& options) {
    // build shape and surface bvhs
    for (auto& shape_bvh : bvh.shape_bvhs)
        build_shape_embree_bvh(shape_bvh, options);

    // scene bvh
    auto embree_device = get_embree_device();
    auto embree_scene  = rtcNewScene(embree_device);
    rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
    bvh.embree_bvh = embree_scene;
    if (bvh.instances.empty()) {
        rtcCommitScene(embree_scene);
        return;
    }
    for (auto instance_id = 0; instance_id < bvh.instances.size();
         instance_id++) {
        auto& instance = bvh.instances[instance_id];
        if (bvh.shape_bvhs[instance.shape_id].positions.empty()) continue;
        auto& shape_bvh             = bvh.shape_bvhs.at(instance.shape_id);
        auto  transformed_positions = shape_bvh.positions;
        if (instance.frame != identity_frame3f) {
            for (auto& p : transformed_positions)
                p = transform_point(instance.frame, p);
        }
        if (!shape_bvh.points.empty()) {
            throw runtime_error("embree does not support points");
        } else if (!shape_bvh.lines.empty()) {
            throw runtime_error("not yet implemented");
        } else if (!shape_bvh.triangles.empty()) {
            auto embree_geom = rtcNewGeometry(
                embree_device, RTC_GEOMETRY_TYPE_TRIANGLE);
            rtcSetGeometryVertexAttributeCount(embree_geom, 1);
            auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
                RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
                shape_bvh.positions.size());
            auto embree_triangles = rtcSetNewGeometryBuffer(embree_geom,
                RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * 4,
                shape_bvh.triangles.size());
            memcpy(embree_positions, transformed_positions.data(),
                transformed_positions.size() * 12);
            memcpy(embree_triangles, shape_bvh.triangles.data(),
                shape_bvh.triangles.size() * 12);
            rtcCommitGeometry(embree_geom);
            rtcAttachGeometryByID(embree_scene, embree_geom, instance_id);
        } else if (!shape_bvh.quads.empty()) {
            auto embree_geom = rtcNewGeometry(
                embree_device, RTC_GEOMETRY_TYPE_QUAD);
            rtcSetGeometryVertexAttributeCount(embree_geom, 1);
            auto embree_positions = rtcSetNewGeometryBuffer(embree_geom,
                RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * 4,
                shape_bvh.positions.size());
            auto embree_quads     = rtcSetNewGeometryBuffer(embree_geom,
                RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT4, 4 * 4,
                shape_bvh.quads.size());
            memcpy(embree_positions, transformed_positions.data(),
                transformed_positions.size() * 12);
            memcpy(embree_quads, shape_bvh.quads.data(),
                shape_bvh.quads.size() * 16);
            rtcCommitGeometry(embree_geom);
            rtcAttachGeometryByID(embree_scene, embree_geom, instance_id);
        } else {
            throw runtime_error("empty bvh");
        }
    }
    rtcCommitScene(embree_scene);
    bvh.embree_flattened = true;
}
// Refit a BVH using Embree. Calls `refit_scene_bvh()` if Embree is not
// available.
void refit_embree_bvh(bvh_shape& bvh) {
    throw runtime_error("not yet implemented");
}
void refit_embree_bvh(bvh_scene& bvh) {
    throw runtime_error("not yet implemented");
}
bvh_shape_intersection intersect_embree_bvh(
    const bvh_shape& bvh, const ray3f& ray, bool find_any) {
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
    rtcIntersect1((RTCScene)bvh.embree_bvh, &embree_ctx, &embree_ray);
    if (embree_ray.hit.geomID == RTC_INVALID_GEOMETRY_ID) return {};
    return {(int)embree_ray.hit.primID, {embree_ray.hit.u, embree_ray.hit.v},
        embree_ray.ray.tfar, true};
}
bvh_scene_intersection intersect_embree_bvh(
    const bvh_scene& bvh, const ray3f& ray, bool find_any) {
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
    rtcIntersect1((RTCScene)bvh.embree_bvh, &embree_ctx, &embree_ray);
    if (embree_ray.hit.geomID == RTC_INVALID_GEOMETRY_ID) return {};
    return {(bvh.embree_flattened) ? (int)embree_ray.hit.geomID
                                   : (int)embree_ray.hit.instID[0],
        (int)embree_ray.hit.primID, {embree_ray.hit.u, embree_ray.hit.v},
        embree_ray.ray.tfar, true};
}
#endif

// BVH primitive with its bbox, its center and the index to the primitive
struct bvh_prim {
    bbox3f bbox   = invalid_bbox3f;
    vec3f  center = zero3f;
    int    primid = 0;
};

// Splits a BVH node using the SAH heuristic. Returns split position and axis.
pair<int, int> split_bvh_node_sah(vector<bvh_prim>& prims, int start, int end) {
    // initialize split axis and position
    auto split_axis = 0;
    auto mid        = (start + end) / 2;

    // compute primintive bounds and size
    auto cbbox = invalid_bbox3f;
    for (auto i = start; i < end; i++) cbbox += prims[i].center;
    auto csize = cbbox.max - cbbox.min;
    if (csize == zero3f) return {mid, split_axis};

    // consider N bins, compute their cost and keep the minimum
    const int nbins     = 16;
    auto      middle    = 0.0f;
    auto      min_cost  = float_max;
    auto      bbox_area = [](auto& b) {
        auto size = b.max - b.min;
        return 1e-12f + 2 * size.x * size.y + 2 * size.x * size.z +
               2 * size.y * size.z;
    };
    auto min_left = 0, min_right = 0;
    for (auto axis = 0; axis < 3; axis++) {
        for (auto b = 1; b < nbins; b++) {
            auto split     = (&cbbox.min.x)[axis] + b * csize[axis] / nbins;
            auto left_bbox = invalid_bbox3f, right_bbox = invalid_bbox3f;
            auto left_nprims = 0, right_nprims = 0;
            for (auto i = start; i < end; i++) {
                if ((&prims[i].center.x)[axis] < split) {
                    left_bbox += prims[i].bbox;
                    left_nprims += 1;
                } else {
                    right_bbox += prims[i].bbox;
                    right_nprims += 1;
                }
            }
            auto cost = 1 +
                        left_nprims * bbox_area(left_bbox) / bbox_area(cbbox) +
                        right_nprims * bbox_area(right_bbox) / bbox_area(cbbox);
            if (cost < min_cost) {
                min_cost   = cost;
                middle     = split;
                split_axis = axis;
                min_left   = left_nprims;
                min_right  = right_nprims;
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
        throw runtime_error("bad bvh split");
        split_axis = 0;
        mid        = (start + end) / 2;
    }

    return {mid, split_axis};
}

// Splits a BVH node using the balance heuristic. Returns split position and
// axis.
pair<int, int> split_bvh_node_balanced(
    vector<bvh_prim>& prims, int start, int end) {
    // initialize split axis and position
    auto split_axis = 0;
    auto mid        = (start + end) / 2;

    // compute primintive bounds and size
    auto cbbox = invalid_bbox3f;
    for (auto i = start; i < end; i++) cbbox += prims[i].center;
    auto csize = cbbox.max - cbbox.min;
    if (csize == zero3f) return {mid, split_axis};

    // split along largest
    auto largest_axis = 0;
    if (csize.x >= csize.y && csize.x >= csize.z) largest_axis = 0;
    if (csize.y >= csize.x && csize.y >= csize.z) largest_axis = 1;
    if (csize.z >= csize.x && csize.z >= csize.y) largest_axis = 2;

    // balanced tree split: find the largest axis of the
    // bounding box and split along this one right in the middle
    split_axis = largest_axis;
    mid        = (start + end) / 2;
    std::nth_element(prims.data() + start, prims.data() + mid,
        prims.data() + end, [split_axis](auto& a, auto& b) {
            return a.center[split_axis] < b.center[split_axis];
        });

    // if we were not able to split, just break the primitives in half
    if (mid == start || mid == end) {
        throw runtime_error("bad bvh split");
        split_axis = 0;
        mid        = (start + end) / 2;
    }

    return {mid, split_axis};
}

// Splits a BVH node using the middle heutirtic. Returns split position and
// axis.
pair<int, int> split_bvh_node_middle(
    vector<bvh_prim>& prims, int start, int end) {
    // initialize split axis and position
    auto split_axis = 0;
    auto mid        = (start + end) / 2;

    // compute primintive bounds and size
    auto cbbox = invalid_bbox3f;
    for (auto i = start; i < end; i++) cbbox += prims[i].center;
    auto csize = cbbox.max - cbbox.min;
    if (csize == zero3f) return {mid, split_axis};

    // split along largest
    auto largest_axis = 0;
    if (csize.x >= csize.y && csize.x >= csize.z) largest_axis = 0;
    if (csize.y >= csize.x && csize.y >= csize.z) largest_axis = 1;
    if (csize.z >= csize.x && csize.z >= csize.y) largest_axis = 2;

    // split the space in the middle along the largest axis
    split_axis   = largest_axis;
    auto cmiddle = (cbbox.max + cbbox.min) / 2;
    auto middle  = cmiddle[largest_axis];
    mid = (int)(std::partition(prims.data() + start, prims.data() + end,
                    [split_axis, middle](
                        auto& a) { return a.center[split_axis] < middle; }) -
                prims.data());

    // if we were not able to split, just break the primitives in half
    if (mid == start || mid == end) {
        throw runtime_error("bad bvh split");
        split_axis = 0;
        mid        = (start + end) / 2;
    }

    return {mid, split_axis};
}

// Initializes the BVH node node that contains the primitives sorted_prims
// from start to end, by either splitting it into two other nodes,
// or initializing it as a leaf. When splitting, the heuristic heuristic is
// used and nodes added sequentially in the preallocated nodes array and
// the number of nodes nnodes is updated.
void make_bvh_node(vector<bvh_node>& nodes, vector<bvh_prim>& prims,
    deque<vec3i>& queue, bool high_quality) {
    // grab node to work on
    auto next = queue.front();
    queue.pop_front();
    auto nodeid = next.x, start = next.y, end = next.z;

    // grab node
    auto& node = nodes[nodeid];

    // compute bounds
    node.bbox = invalid_bbox3f;
    for (auto i = start; i < end; i++) node.bbox += prims[i].bbox;

    // split into two children
    if (end - start > bvh_max_prims) {
        // get split
        auto [mid, split_axis] =
            (high_quality) ? split_bvh_node_sah(prims, start, end)
                           : split_bvh_node_balanced(prims, start, end);

        // make an internal node
        node.is_internal      = true;
        node.split_axis       = split_axis;
        node.num_primitives   = 2;
        node.primitive_ids[0] = (int)nodes.size() + 0;
        node.primitive_ids[1] = (int)nodes.size() + 1;
        nodes.emplace_back();
        nodes.emplace_back();
        queue.push_back({node.primitive_ids[0], start, mid});
        queue.push_back({node.primitive_ids[1], mid, end});
    } else {
        // Make a leaf node
        node.is_internal    = false;
        node.num_primitives = end - start;
        for (auto i = 0; i < node.num_primitives; i++)
            node.primitive_ids[i] = prims[start + i].primid;
    }
}

// Build BVH nodes
void build_bvh_nodes_serial(vector<bvh_node>& nodes, vector<bvh_prim>& prims,
    const build_bvh_options& options) {
    // prepare to build nodes
    nodes.clear();
    nodes.reserve(prims.size() * 2);

    // queue up first node
    auto queue = deque<vec3i>{{0, 0, (int)prims.size()}};
    nodes.emplace_back();

    // create nodes until the queue is empty
    while (!queue.empty()) {
        // exit if needed
        if (options.cancel_flag && *options.cancel_flag) return;

        // grab node to work on
        auto next = queue.front();
        queue.pop_front();
        auto nodeid = next.x, start = next.y, end = next.z;

        // grab node
        auto& node = nodes[nodeid];

        // compute bounds
        node.bbox = invalid_bbox3f;
        for (auto i = start; i < end; i++) node.bbox += prims[i].bbox;

        // split into two children
        if (end - start > bvh_max_prims) {
            // get split
            auto [mid, split_axis] = (options.high_quality)
                                         ? split_bvh_node_sah(prims, start, end)
                                         : split_bvh_node_balanced(
                                               prims, start, end);

            // make an internal node
            node.is_internal      = true;
            node.split_axis       = split_axis;
            node.num_primitives   = 2;
            node.primitive_ids[0] = (int)nodes.size() + 0;
            node.primitive_ids[1] = (int)nodes.size() + 1;
            nodes.emplace_back();
            nodes.emplace_back();
            queue.push_back({node.primitive_ids[0], start, mid});
            queue.push_back({node.primitive_ids[1], mid, end});
        } else {
            // Make a leaf node
            node.is_internal    = false;
            node.num_primitives = end - start;
            for (auto i = 0; i < node.num_primitives; i++)
                node.primitive_ids[i] = prims[start + i].primid;
        }
    }

    // cleanup
    nodes.shrink_to_fit();
}

// Build BVH nodes
void build_bvh_nodes_parallel(vector<bvh_node>& nodes, vector<bvh_prim>& prims,
    const build_bvh_options& options) {
    // prepare to build nodes
    nodes.clear();
    nodes.reserve(prims.size() * 2);

    // queue up first node
    auto queue = deque<vec3i>{{0, 0, (int)prims.size()}};
    nodes.emplace_back();

    // synchronization
    atomic<int>          num_processed_prims(0);
    mutex                queue_mutex;
    vector<future<void>> futures;
    auto                 nthreads = thread::hardware_concurrency();

    // create nodes until the queue is empty
    for (auto thread_id = 0; thread_id < nthreads; thread_id++) {
        futures.emplace_back(async([&nodes, &prims, &options,
                                       &num_processed_prims, &queue_mutex,
                                       &queue] {
            while (true) {
                // exit if needed
                if (num_processed_prims >= prims.size()) return;
                if (options.cancel_flag && *options.cancel_flag) return;

                // grab node to work on
                auto next = zero3i;
                {
                    lock_guard<mutex> lock{queue_mutex};
                    if (!queue.empty()) {
                        next = queue.front();
                        queue.pop_front();
                    }
                }

                // wait a bit if needed
                if (next == zero3i) {
                    std::this_thread::sleep_for(10us);
                    continue;
                }

                // grab node
                auto  nodeid = next.x, start = next.y, end = next.z;
                auto& node = nodes[nodeid];

                // compute bounds
                node.bbox = invalid_bbox3f;
                for (auto i = start; i < end; i++) node.bbox += prims[i].bbox;

                // split into two children
                if (end - start > bvh_max_prims) {
                    // get split
                    auto [mid, split_axis] =
                        (options.high_quality)
                            ? split_bvh_node_sah(prims, start, end)
                            : split_bvh_node_balanced(prims, start, end);

                    // make an internal node
                    {
                        lock_guard<mutex> lock{queue_mutex};
                        node.is_internal      = true;
                        node.split_axis       = split_axis;
                        node.num_primitives   = 2;
                        node.primitive_ids[0] = (int)nodes.size() + 0;
                        node.primitive_ids[1] = (int)nodes.size() + 1;
                        nodes.emplace_back();
                        nodes.emplace_back();
                        queue.push_back({node.primitive_ids[0], start, mid});
                        queue.push_back({node.primitive_ids[1], mid, end});
                    }
                } else {
                    // Make a leaf node
                    node.is_internal    = false;
                    node.num_primitives = end - start;
                    for (auto i = 0; i < node.num_primitives; i++)
                        node.primitive_ids[i] = prims[start + i].primid;
                    num_processed_prims += node.num_primitives;
                }
            }
        }));
    }
    for (auto& f : futures) f.get();

    // cleanup
    nodes.shrink_to_fit();
}

// Build a BVH from a set of primitives.
void build_shape_bvh(bvh_shape& bvh, const build_bvh_options& options) {
#if YOCTO_EMBREE
    if (options.use_embree) return build_shape_embree_bvh(bvh, options);
#endif

    // get the number of primitives and the primitive type
    auto prims = vector<bvh_prim>();
    if (!bvh.points.empty()) {
        for (auto& p : bvh.points) {
            prims.push_back({point_bounds(bvh.positions[p], bvh.radius[p])});
        }
    } else if (!bvh.lines.empty()) {
        for (auto& l : bvh.lines) {
            prims.push_back({line_bounds(bvh.positions[l.x], bvh.positions[l.y],
                bvh.radius[l.x], bvh.radius[l.y])});
        }
    } else if (!bvh.triangles.empty()) {
        for (auto& t : bvh.triangles) {
            prims.push_back({triangle_bounds(
                bvh.positions[t.x], bvh.positions[t.y], bvh.positions[t.z])});
        }
    } else if (!bvh.quads.empty()) {
        for (auto& q : bvh.quads) {
            prims.push_back({quad_bounds(bvh.positions[q.x], bvh.positions[q.y],
                bvh.positions[q.z], bvh.positions[q.w])});
        }
    }

    // create an array of primitives to sort
    for (auto i = 0; i < prims.size(); i++) {
        prims[i].center = (prims[i].bbox.min + prims[i].bbox.max) / 2;
        prims[i].primid = i;
    }

    // build nodes
    if (options.run_serially) {
        build_bvh_nodes_serial(bvh.nodes, prims, options);
    } else {
        build_bvh_nodes_parallel(bvh.nodes, prims, options);
    }
}

// Build a BVH from the given set of shape primitives.
void init_shape_bvh(bvh_shape& bvh, const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius) {
    bvh           = {};
    bvh.points    = points;
    bvh.positions = positions;
    bvh.radius    = radius;
}
void init_shape_bvh(bvh_shape& bvh, const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius) {
    bvh           = {};
    bvh.lines     = lines;
    bvh.positions = positions;
    bvh.radius    = radius;
}
void init_shape_bvh(bvh_shape& bvh, const vector<vec3i>& triangles,
    const vector<vec3f>& positions) {
    bvh           = {};
    bvh.triangles = triangles;
    bvh.positions = positions;
}
void init_shape_bvh(bvh_shape& bvh, const vector<vec4i>& quads,
    const vector<vec3f>& positions) {
    bvh           = {};
    bvh.quads     = quads;
    bvh.positions = positions;
}

// Build a BVH from a set of primitives.
void build_scene_bvh(bvh_scene& bvh, const build_bvh_options& options) {
#if YOCTO_EMBREE
    if (options.use_embree) {
        if (options.flatten_embree) {
            return build_scene_embree_flattened_bvh(bvh, options);
        } else {
            return build_scene_embree_instanced_bvh(bvh, options);
        }
    }
#endif

    // build shape and surface bvhs
    for (auto& shape_bvh : bvh.shape_bvhs) build_shape_bvh(shape_bvh, options);

    // get the number of primitives and the primitive type
    auto prims = vector<bvh_prim>();
    if (!bvh.instances.empty()) {
        for (auto& instance : bvh.instances) {
            auto& sbvh = bvh.shape_bvhs[instance.shape_id];
            if (sbvh.nodes.empty()) {
                prims.push_back({invalid_bbox3f});
            } else {
                prims.push_back(
                    {transform_bbox(instance.frame, sbvh.nodes[0].bbox)});
            }
        }
    }

    // create an array of primitives to sort
    for (auto i = 0; i < prims.size(); i++) {
        prims[i].center = (prims[i].bbox.min + prims[i].bbox.max) / 2;
        prims[i].primid = i;
    }

    // build nodes
    if (options.run_serially) {
        build_bvh_nodes_serial(bvh.nodes, prims, options);
    } else {
        build_bvh_nodes_parallel(bvh.nodes, prims, options);
    }
}

// Build a BVH from the given set of instances.
void init_scene_bvh(bvh_scene& bvh, const vector<bvh_instance>& instances,
    const vector<bvh_shape>& shape_bvhs) {
    bvh            = {};
    bvh.instances  = instances;
    bvh.shape_bvhs = shape_bvhs;
}

// Recursively recomputes the node bounds for a shape bvh
void refit_shape_bvh_rec(bvh_shape& bvh, int nodeid) {
    // refit
    auto& node = bvh.nodes[nodeid];
    node.bbox  = invalid_bbox3f;
    if (node.is_internal) {
        for (auto i = 0; i < 2; i++) {
            refit_shape_bvh_rec(bvh, node.primitive_ids[i]);
            node.bbox += bvh.nodes[node.primitive_ids[i]].bbox;
        }
    } else if (!bvh.triangles.empty()) {
        for (auto i = 0; i < node.num_primitives; i++) {
            auto& t = bvh.triangles[node.primitive_ids[i]];
            node.bbox += triangle_bounds(
                bvh.positions[t.x], bvh.positions[t.y], bvh.positions[t.z]);
        }
    } else if (!bvh.quads.empty()) {
        for (auto i = 0; i < node.num_primitives; i++) {
            auto& t = bvh.quads[node.primitive_ids[i]];
            node.bbox += quad_bounds(bvh.positions[t.x], bvh.positions[t.y],
                bvh.positions[t.z], bvh.positions[t.w]);
        }
    } else if (!bvh.lines.empty()) {
        for (auto i = 0; i < node.num_primitives; i++) {
            auto& l = bvh.lines[node.primitive_ids[i]];
            node.bbox += line_bounds(bvh.positions[l.x], bvh.positions[l.y],
                bvh.radius[l.x], bvh.radius[l.y]);
        }
    } else if (!bvh.points.empty()) {
        for (auto i = 0; i < node.num_primitives; i++) {
            auto& p = bvh.points[node.primitive_ids[i]];
            node.bbox += point_bounds(bvh.positions[p], bvh.radius[p]);
        }
    } else {
        printf("empty bvh");
    }
}

// Recursively recomputes the node bounds for a shape bvh
void refit_scene_bvh_rec(bvh_scene& bvh, int nodeid) {
    // refit
    auto& node = bvh.nodes[nodeid];
    node.bbox  = invalid_bbox3f;
    if (node.is_internal) {
        for (auto i = 0; i < 2; i++) {
            refit_scene_bvh_rec(bvh, node.primitive_ids[i]);
            node.bbox += bvh.nodes[node.primitive_ids[i]].bbox;
        }
    } else if (!bvh.instances.empty()) {
        for (auto i = 0; i < node.num_primitives; i++) {
            auto& instance = bvh.instances[node.primitive_ids[i]];
            auto  sbvh     = bvh.shape_bvhs[instance.shape_id];
            node.bbox += transform_bbox(
                instance.frame, sbvh.nodes.front().bbox);
        }
    } else {
        printf("empty bvh");
    }
}

// Recursively recomputes the node bounds for a shape bvh
void refit_shape_bvh(bvh_shape& bvh) { refit_shape_bvh_rec(bvh, 0); }
void refit_scene_bvh(bvh_scene& bvh, const vector<int>& updated_instances,
    const vector<int>& updated_shapes) {
    for (auto shape_id : updated_shapes)
        refit_shape_bvh(bvh.shape_bvhs[shape_id]);
    if (!updated_instances.empty()) refit_scene_bvh_rec(bvh, 0);
}

// Recursively recomputes the node bounds for a shape bvh
void update_shape_bvh(bvh_shape& bvh, const vector<vec3f>& positions) {
    bvh.positions = positions;
}
void update_shape_bvh(bvh_shape& bvh, const vector<vec3f>& positions,
    const vector<float>& radius) {
    bvh.positions = positions;
    bvh.radius    = radius;
}
void update_scene_bvh(bvh_scene& bvh, const vector<bvh_instance>& instances) {
    bvh.instances = instances;
}
bvh_shape& get_shape_bvh(bvh_scene& bvh, int shape_id) {
    return bvh.shape_bvhs[shape_id];
}

// Intersect ray with a bvh.
bvh_shape_intersection intersect_shape_bvh(
    const bvh_shape& bvh, const ray3f& ray_, bool find_any) {
#if YOCTO_EMBREE
    // call Embree if needed
    if (bvh.embree_bvh) return intersect_embree_bvh(bvh, ray_, find_any);
#endif

    // check empty
    if (bvh.nodes.empty()) return {};
    if (bvh.points.empty() && bvh.lines.empty() && bvh.triangles.empty() &&
        bvh.quads.empty())
        return {};

    // node stack
    int  node_stack[128];
    auto node_cur          = 0;
    node_stack[node_cur++] = 0;

    // shared variables
    auto intersection = bvh_shape_intersection{};

    // copy ray to modify it
    auto ray = ray_;

    // prepare ray for fast queries
    auto ray_dinv  = vec3f{1 / ray.d.x, 1 / ray.d.y, 1 / ray.d.z};
    auto ray_dsign = vec3i{(ray_dinv.x < 0) ? 1 : 0, (ray_dinv.y < 0) ? 1 : 0,
        (ray_dinv.z < 0) ? 1 : 0};

    // walking stack
    while (node_cur) {
        // grab node
        auto& node = bvh.nodes[node_stack[--node_cur]];

        // intersect bbox
        // if (!intersect_bbox(ray, ray_dinv, ray_dsign, node.bbox)) continue;
        if (!intersect_bbox(ray, ray_dinv, node.bbox)) continue;

        // intersect node, switching based on node type
        // for each type, iterate over the the primitive list
        if (node.is_internal) {
            // for internal nodes, attempts to proceed along the
            // split axis from smallest to largest nodes
            if (ray_dsign[node.split_axis]) {
                node_stack[node_cur++] = node.primitive_ids[0];
                node_stack[node_cur++] = node.primitive_ids[1];
            } else {
                node_stack[node_cur++] = node.primitive_ids[1];
                node_stack[node_cur++] = node.primitive_ids[0];
            }
        } else {
            for (auto i = 0; i < node.num_primitives; i++) {
                auto element_intersection = bvh_element_intersection{};
                if (!bvh.triangles.empty()) {
                    auto& t              = bvh.triangles[node.primitive_ids[i]];
                    element_intersection = intersect_triangle(ray,
                        bvh.positions[t.x], bvh.positions[t.y],
                        bvh.positions[t.z]);
                } else if (!bvh.quads.empty()) {
                    auto& t              = bvh.quads[node.primitive_ids[i]];
                    element_intersection = intersect_quad(ray,
                        bvh.positions[t.x], bvh.positions[t.y],
                        bvh.positions[t.z], bvh.positions[t.w]);
                } else if (!bvh.lines.empty()) {
                    auto& l              = bvh.lines[node.primitive_ids[i]];
                    element_intersection = intersect_line(ray,
                        bvh.positions[l.x], bvh.positions[l.y], bvh.radius[l.x],
                        bvh.radius[l.y]);
                } else if (!bvh.points.empty()) {
                    auto& p              = bvh.points[node.primitive_ids[i]];
                    element_intersection = intersect_point(
                        ray, bvh.positions[p], bvh.radius[p]);
                } else {
                    continue;
                }
                if (!element_intersection.hit) continue;
                intersection = {node.primitive_ids[i],
                    element_intersection.element_uv,
                    element_intersection.distance, true};
                ray.tmax     = intersection.distance;
            }
        }

        // check for early exit
        if (find_any && intersection.hit) return intersection;
    }

    return intersection;
}

// Intersect ray with a bvh.
bvh_scene_intersection intersect_scene_bvh(
    const bvh_scene& bvh, const ray3f& ray_, bool find_any) {
#if YOCTO_EMBREE
    // call Embree if needed
    if (bvh.embree_bvh) return intersect_embree_bvh(bvh, ray_, find_any);
#endif

    // check empty
    if (bvh.nodes.empty()) return {};
    if (bvh.instances.empty()) return {};

    // node stack
    int  node_stack[128];
    auto node_cur          = 0;
    node_stack[node_cur++] = 0;

    // shared variables
    auto intersection = bvh_scene_intersection{};

    // copy ray to modify it
    auto ray = ray_;

    // prepare ray for fast queries
    auto ray_dinv  = vec3f{1 / ray.d.x, 1 / ray.d.y, 1 / ray.d.z};
    auto ray_dsign = vec3i{(ray_dinv.x < 0) ? 1 : 0, (ray_dinv.y < 0) ? 1 : 0,
        (ray_dinv.z < 0) ? 1 : 0};

    // walking stack
    while (node_cur) {
        // grab node
        auto& node = bvh.nodes[node_stack[--node_cur]];

        // intersect bbox
        // if (!intersect_bbox(ray, ray_dinv, ray_dsign, node.bbox)) continue;
        if (!intersect_bbox(ray, ray_dinv, node.bbox)) continue;

        // intersect node, switching based on node type
        // for each type, iterate over the the primitive list
        if (node.is_internal) {
            // for internal nodes, attempts to proceed along the
            // split axis from smallest to largest nodes
            if (ray_dsign[node.split_axis]) {
                node_stack[node_cur++] = node.primitive_ids[0];
                node_stack[node_cur++] = node.primitive_ids[1];
            } else {
                node_stack[node_cur++] = node.primitive_ids[1];
                node_stack[node_cur++] = node.primitive_ids[0];
            }
        } else {
            for (auto i = 0; i < node.num_primitives; i++) {
                auto& instance           = bvh.instances[node.primitive_ids[i]];
                auto  shape_intersection = intersect_shape_bvh(
                    bvh.shape_bvhs[instance.shape_id],
                    transform_ray(instance.frame_inverse, ray), find_any);
                if (!shape_intersection.hit) continue;
                intersection = {node.primitive_ids[i],
                    shape_intersection.element_id,
                    shape_intersection.element_uv, shape_intersection.distance,
                    true};
                ray.tmax     = intersection.distance;
            }
        }

        // check for early exit
        if (find_any && intersection.hit) return intersection;
    }

    return intersection;
}

// Finds the closest element with a bvh.
bvh_shape_intersection overlap_shape_bvh(
    const bvh_shape& bvh, const vec3f& pos, float max_distance, bool find_any) {
    // node stack
    int  node_stack[64];
    auto node_cur          = 0;
    node_stack[node_cur++] = 0;

    // hit
    auto intersection = bvh_shape_intersection{};

    // walking stack
    while (node_cur) {
        // grab node
        auto node = bvh.nodes[node_stack[--node_cur]];

        // intersect bbox
        if (!distance_check_bbox(pos, max_distance, node.bbox)) continue;

        // intersect node, switching based on node type
        // for each type, iterate over the the primitive list
        if (node.is_internal) {
            // internal node
            node_stack[node_cur++] = node.primitive_ids[0];
            node_stack[node_cur++] = node.primitive_ids[1];
        } else {
            for (auto i = 0; i < node.num_primitives; i++) {
                auto element_intersection = bvh_element_intersection{};
                if (!bvh.triangles.empty()) {
                    auto& t              = bvh.triangles[node.primitive_ids[i]];
                    element_intersection = overlap_triangle(pos, max_distance,
                        bvh.positions[t.x], bvh.positions[t.y],
                        bvh.positions[t.z], bvh.radius[t.x], bvh.radius[t.y],
                        bvh.radius[t.z]);
                } else if (!bvh.quads.empty()) {
                    auto& t              = bvh.quads[node.primitive_ids[i]];
                    element_intersection = overlap_quad(pos, max_distance,
                        bvh.positions[t.x], bvh.positions[t.y],
                        bvh.positions[t.z], bvh.positions[t.w], bvh.radius[t.x],
                        bvh.radius[t.y], bvh.radius[t.z], bvh.radius[t.w]);
                } else if (!bvh.lines.empty()) {
                    auto& l              = bvh.lines[node.primitive_ids[i]];
                    element_intersection = overlap_line(pos, max_distance,
                        bvh.positions[l.x], bvh.positions[l.y], bvh.radius[l.x],
                        bvh.radius[l.y]);
                } else if (!bvh.points.empty()) {
                    auto& p              = bvh.points[node.primitive_ids[i]];
                    element_intersection = overlap_point(
                        pos, max_distance, bvh.positions[p], bvh.radius[p]);
                } else {
                    continue;
                }
                if (!element_intersection.hit) continue;
                intersection = {node.primitive_ids[i],
                    element_intersection.element_uv,
                    element_intersection.distance, true};
                max_distance = intersection.distance;
            }
        }

        // check for early exit
        if (find_any && intersection.hit) return intersection;
    }

    return intersection;
}

bvh_scene_intersection intersect_instance_bvh(
    const bvh_scene& bvh, int instance_id, const ray3f& ray, bool find_any) {
    auto& instance           = bvh.instances[instance_id];
    auto  tray               = transform_ray_inverse(instance.frame, ray);
    auto  shape_intersection = bvh_shape_intersection{};
    shape_intersection       = intersect_shape_bvh(
        bvh.shape_bvhs[instance.shape_id], tray, find_any);
    if (!shape_intersection.hit) return {};
    return {instance_id, shape_intersection.element_id,
        shape_intersection.element_uv, shape_intersection.distance, true};
}

// Finds the closest element with a bvh.
bvh_scene_intersection overlap_scene_bvh(
    const bvh_scene& bvh, const vec3f& pos, float max_distance, bool find_any) {
    // node stack
    int  node_stack[64];
    auto node_cur          = 0;
    node_stack[node_cur++] = 0;

    // hit
    auto intersection = bvh_scene_intersection{};

    // walking stack
    while (node_cur) {
        // grab node
        auto node = bvh.nodes[node_stack[--node_cur]];

        // intersect bbox
        if (!distance_check_bbox(pos, max_distance, node.bbox)) continue;

        // intersect node, switching based on node type
        // for each type, iterate over the the primitive list
        if (node.is_internal) {
            // internal node
            node_stack[node_cur++] = node.primitive_ids[0];
            node_stack[node_cur++] = node.primitive_ids[1];
        } else if (!bvh.instances.empty()) {
            for (auto i = 0; i < node.num_primitives; i++) {
                auto& instance           = bvh.instances[node.primitive_ids[i]];
                auto  shape_intersection = overlap_shape_bvh(
                    bvh.shape_bvhs[instance.shape_id],
                    transform_point(instance.frame_inverse, pos), max_distance,
                    find_any);
                if (!shape_intersection.hit) continue;
                intersection = {node.primitive_ids[i],
                    shape_intersection.element_id,
                    shape_intersection.element_uv, shape_intersection.distance,
                    true};
                max_distance = intersection.distance;
            }
        } else {
            throw runtime_error("empty bvh");
        }

        // check for early exit
        if (find_any && intersection.hit) return intersection;
    }

    return intersection;
}

#if 0
    // Finds the overlap between BVH leaf nodes.
    template <typename OverlapElem>
    void overlap_bvh_elems(const bvh_scene& bvh1, const bvh_scene& bvh2,
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

}  // namespace yocto
