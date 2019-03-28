//
// # Yocto/Shape: Tiny Library for shape operations for graphics
//
//
// Yocto/Shape is a collection of utilities for manipulating shapes in 3D
// graphics, with a focus on triangle and quad meshes. We support both low-level
// geometry operation and whole shape operations.
//
//
// ## Geometry functions
//
// The library supports basic geomtry functions such as computing
// line/triangle/quad normals and areas, picking points on triangles
// and the like. In these functions triangles are parameterized with us written
// w.r.t the (p1-p0) and (p2-p0) axis respectively. Quads are internally handled
// as pairs of two triangles p0,p1,p3 and p2,p3,p1, with the u/v coordinates
// of the second triangle corrected as 1-u and 1-v to produce a quad
// parametrization where u and v go from 0 to 1. Degenerate quads with p2==p3
// represent triangles correctly, an this convention is used throught the
// library. This is equivalent to Intel's Embree.
//
//
// ## Shape functions
//
// We provide a small number of utilities for shape manipulation for index
// triangle and quad meshes, indexed line and point sets and indexed beziers.
// The utliities collected here are written to support a global illumination
// rendering and not for generic geometry processing. We support operation for
// shape smoothing, shape subdivision (including Catmull-Clark subdivs), and
// example shape creation.
//
// 1. compute line tangents, and triangle and quad areas and normals with
//    `line_tangent()`, `triamgle_normal()`, `quad_normal()` and
//    `line_length()`, `triangle_area()` and `quad_normal()`
// 2. interpolate values over primitives with `interpolate_line()`,
//    `interpolate_triangle()` and `interpolate_quad()`
// 3. evaluate Bezier curves and derivatives with `interpolate_bezier()` and
//    `interpolate_bezier_derivative()`
// 4. compute smooth normals and tangents with `compute_vertex_normals()`
//   `compute_vertex_tangents()`
// 5. compute tangent frames from texture coordinates with
//    `compute_tangent_spaces()`
// 6. compute skinning with `compute_skinning()` and
//    `compute_matrix_skinning()`
// 6. create shapes with `make_cube_shape()`, `make_uvsphere_shape()`,
// `make_quad_shape()`,
//    `make_cube_fvshape()`, `make_hair_shape()`, `make_suzanne_shape()`,
//    `make_lines_shape()`, `make_points_shape()`, `make_sphere_shape()`,
//    `make_cube_rounded_shape()`, `make_uvsphere_flipcap_shape()`,
//    `make_uvcylinder_shape()`, `make_uvcylinder_rounded_shape()`,
//    `make_uvdisk_shape()`, `make_cylinder_side_shape()`,
//    `make_disk_shape()`
// 7. merge element with `marge_lines()`, `marge_triangles()`, `marge_quads()`
// 8. shape sampling with `sample_points_element()`, `sample_lines_element()`,
//    `sample_triangles_element()`; initialize the sampling CDFs with
//    `sample_points_element_cdf()`, `sample_lines_element_cdf()`,
//    `sample_triangles_element_cdf()`
// 9.  sample a could of point over a surface with
// `sample_triangles_element_points()`
// 10. get edges and boundaries with `get_edges()`
// 11. convert quads to triangles with `convert_quads_to_triangles()`
// 12. convert face varying to vertex shared representations with
//     `convert_face_varying()`
// 13. subdivide elements by edge splits with `subdivide_lines()`,
//     `subdivide_triangles()`, `subdivide_quads()`, `subdivide_beziers()`
// 14. Catmull-Clark subdivision surface with `subdivide_catmullclark()`
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

#ifndef _YOCTO_SHAPE_H_
#define _YOCTO_SHAPE_H_

#ifndef YOCTO_QUADS_AS_TRIANGLES
#define YOCTO_QUADS_AS_TRIANGLES 1
#endif

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"
#include "yocto_utils.h"

#include <unordered_map>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

using std::unordered_map;

}

// -----------------------------------------------------------------------------
// GEOMETRY UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Line properties.
inline vec3f line_tangent(const vec3f& p0, const vec3f& p1) {
    return normalize(p1 - p0);
}
inline float line_length(const vec3f& p0, const vec3f& p1) {
    return length(p1 - p0);
}

// Triangle properties.
inline vec3f triangle_normal(
    const vec3f& p0, const vec3f& p1, const vec3f& p2) {
    return normalize(cross(p1 - p0, p2 - p0));
}
inline float triangle_area(const vec3f& p0, const vec3f& p1, const vec3f& p2) {
    return length(cross(p1 - p0, p2 - p0)) / 2;
}

// Quad propeties.
inline vec3f quad_normal(
    const vec3f& p0, const vec3f& p1, const vec3f& p2, const vec3f& p3) {
    return normalize(triangle_normal(p0, p1, p3) + triangle_normal(p2, p3, p1));
}
inline float quad_area(
    const vec3f& p0, const vec3f& p1, const vec3f& p2, const vec3f& p3) {
    return triangle_area(p0, p1, p3) + triangle_area(p2, p3, p1);
}

// Triangle tangent and bitangent from uv
inline pair<vec3f, vec3f> triangle_tangents_fromuv(const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec2f& uv0, const vec2f& uv1,
    const vec2f& uv2);

// Quad tangent and bitangent from uv. Note that we pass a current_uv since
// internally we may want to split the quad in two and we need to known where
// to do it. If not interested in the split, just pass zero2f here.
inline pair<vec3f, vec3f> quad_tangents_fromuv(const vec3f& p0, const vec3f& p1,
    const vec3f& p2, const vec3f& p3, const vec2f& uv0, const vec2f& uv1,
    const vec2f& uv2, const vec2f& uv3, const vec2f& current_uv);

// Interpolates values over a line parameterized from a to b by u. Same as lerp.
template <typename T>
inline T interpolate_line(const T& p0, const T& p1, float u) {
    return p0 * (1 - u) + p1 * u;
}
// Interpolates values over a triangle parameterized by u and v along the
// (p1-p0) and (p2-p0) directions. Same as barycentric interpolation.
template <typename T>
inline T interpolate_triangle(
    const T& p0, const T& p1, const T& p2, const vec2f& uv) {
    return p0 * (1 - uv.x - uv.y) + p1 * uv.x + p2 * uv.y;
}
// Interpolates values over a quad parameterized by u and v along the
// (p1-p0) and (p2-p1) directions. Same as bilinear interpolation.
template <typename T>
inline T interpolate_quad(
    const T& p0, const T& p1, const T& p2, const T& p3, const vec2f& uv) {
#if YOCTO_QUADS_AS_TRIANGLES
    if (uv.x + uv.y <= 1) {
        return interpolate_triangle(p0, p1, p3, uv);
    } else {
        return interpolate_triangle(p2, p3, p1, 1 - uv);
    }
#else
    return p0 * (1 - uv.x) * (1 - uv.y) + p1 * uv.x * (1 - uv.y) +
           p2 * uv.x * uv.y + p3 * (1 - uv.x) * uv.y;
#endif
}

// Interpolates values along a cubic Bezier segment parametrized by u.
template <typename T>
inline T interpolate_bezier(
    const T& p0, const T& p1, const T& p2, const T& p3, float u) {
    return p0 * (1 - u) * (1 - u) * (1 - u) + p1 * 3 * u * (1 - u) * (1 - u) +
           p2 * 3 * u * u * (1 - u) + p3 * u * u * u;
}
// Computes the derivative of a cubic Bezier segment parametrized by u.
template <typename T>
inline T interpolate_bezier_derivative(
    const T& p0, const T& p1, const T& p2, const T& p3, float u) {
    return (p1 - p0) * 3 * (1 - u) * (1 - u) + (p2 - p1) * 6 * u * (1 - u) +
           (p3 - p2) * 3 * u * u;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// COMPUTATION OF PER_VERTEX PROPETIES
// -----------------------------------------------------------------------------
namespace yocto {

// Compute per-vertex normals/tangents for lines/triangles/quads.
void compute_vertex_tangents(vector<vec3f>& tangents,
    const vector<vec2i>& lines, const vector<vec3f>& positions);
void compute_vertex_normals(vector<vec3f>& normals,
    const vector<vec3i>& triangles, const vector<vec3f>& positions);
void compute_vertex_normals(vector<vec3f>& normals, const vector<vec4i>& quads,
    const vector<vec3f>& positions);

// Compute per-vertex tangent space for triangle meshes.
// Tangent space is defined by a four component vector.
// The first three components are the tangent with respect to the u texcoord.
// The fourth component is the sign of the tangent wrt the v texcoord.
// Tangent frame is useful in normal mapping.
void compute_tangent_spaces(vector<vec4f>& tangent_spaces,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords);

// Apply skinning to vertex position and normals.
void compute_skinning(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms);
// Apply skinning as specified in Khronos glTF.
void compute_matrix_skinning(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EDGE AND GRID DATA STRUCTURES
// -----------------------------------------------------------------------------
namespace yocto {

// Dictionary to store edge information. edge_index is the index to the edge
// array, edges the array of edges and adj_faces the adjacent faces. We store
// only bidirectional edges to keep the dictionary small. Use the functions
// below to access this data.
struct edge_map {
    unordered_map<vec2i, int> edge_index = {};
    vector<vec2i>             edges      = {};
    vector<int>               num_faces  = {};
};

// Initialize an edge map with elements.
void insert_edges(edge_map& emap, const vector<vec3i>& triangles);
void insert_edges(edge_map& emap, const vector<vec4i>& quads);
// Insert an edge and return its index
int insert_edge(edge_map& emap, const vec2i& edge);
// Get the edge index / insertion count
int get_edge_index(const edge_map& emap, const vec2i& edge);
// Get list of edges / boundary edges
int  get_num_edges(const edge_map& emap);
void get_edges(const edge_map& emap, vector<vec2i>& edges);
void get_boundary(const edge_map& emap, vector<vec2i>& edges);
void get_edges(const vector<vec3i>& triangles, vector<vec2i>& edges);
void get_edges(const vector<vec4i>& quads, vector<vec2i>& edges);

// A sparse grid of cells, containing list of points. Cells are stored in
// a dictionary to get sparsing. Helpful for nearest neighboor lookups.
struct hash_grid {
    float                             cell_size     = 0;
    float                             cell_inv_size = 0;
    vector<vec3f>                     positions     = {};
    unordered_map<vec3i, vector<int>> cells         = {};
};

// Create a hash_grid
void init_hash_grid(hash_grid& grid, float cell_size);
void init_hash_grid(
    hash_grid& grid, const vector<vec3f>& positions, float cell_size);
// Inserts a point into the grid
int insert_vertex(hash_grid& grid, const vec3f& position);
// Finds the nearest neighboors within a given radius
void find_nearest_neightbors(const hash_grid& grid, vector<int>& neighboors,
    const vec3f& position, float max_radius);
void find_nearest_neightbors(const hash_grid& grid, vector<int>& neighboors,
    int vertex_id, float max_radius);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE ELEMENT CONVERSION AND GROUPING
// -----------------------------------------------------------------------------
namespace yocto {

// Convert quads to triangles
void convert_quads_to_triangles(
    vector<vec3i>& triangles, const vector<vec4i>& quads);
// Convert quads to triangles with a diamond-like topology.
// Quads have to be consecutive one row after another.
void convert_quads_to_triangles(
    vector<vec3i>& triangles, const vector<vec4i>& quads, int row_length);
// Convert triangles to quads by creating degenerate quads
void convert_triangles_to_quads(
    vector<vec4i>& quads, const vector<vec3i>& triangles);

// Convert beziers to lines using 3 lines for each bezier.
void convert_bezier_to_lines(
    vector<vec2i>& lines, const vector<vec4i>& beziers);

// Convert face-varying data to single primitives. Returns the quads indices
// and face ids and filled vectors for pos, norm, texcoord and colors.
void convert_facevarying(vector<vec4i>& split_quads,
    vector<vec3f>& split_positions, vector<vec3f>& split_normals,
    vector<vec2f>& split_texturecoords, const vector<vec4i>& quads_positions,
    const vector<vec4i>& quads_normals,
    const vector<vec4i>& quads_texturecoords, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords);

// Split primitives per id
void ungroup_lines(vector<vector<vec2i>>& split_lines,
    const vector<vec2i>& lines, const vector<int>& ids);
void ungroup_triangles(vector<vector<vec3i>>& split_triangles,
    const vector<vec3i>& triangles, const vector<int>& ids);
void ungroup_quads(vector<vector<vec4i>>& split_quads,
    const vector<vec4i>& quads, const vector<int>& ids);

// Weld vertices within a threshold.
void weld_vertices(
    vector<vec3f>& positions, vector<int>& indices, float threshold);
void weld_triangles(
    vector<vec3i>& triangles, vector<vec3f>& positions, float threshold);
void weld_quads(
    vector<vec4i>& quads, vector<vec3f>& positions, float threshold);

// Merge shape elements
void merge_lines(
    vector<vec2i>& lines, const vector<vec2i>& merge_lines, int num_verts);
void merge_triangles(vector<vec3i>& triangles,
    const vector<vec2i>& merge_triangles, int num_verts);
void merge_quads(
    vector<vec4i>& quads, const vector<vec4i>& merge_quads, int num_verts);
void merge_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& tangents, vector<vec2f>& texturecoords,
    vector<float>& radius, const vector<vec2i>& merge_lines,
    const vector<vec3f>& merge_positions, const vector<vec3f>& merge_tangents,
    const vector<vec2f>& merge_texturecoords,
    const vector<float>& merge_radius);
void merge_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
    const vector<vec2i>& merge_triangles, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords);
void merge_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
    const vector<vec4i>& merge_quads, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE SUBDIVISION
// -----------------------------------------------------------------------------
namespace yocto {

// Subdivide lines by splitting each line in half.
void subdivide_lines(vector<vec2i>& lines, vector<float>& vert);
void subdivide_lines(vector<vec2i>& lines, vector<vec2f>& vert);
void subdivide_lines(vector<vec2i>& lines, vector<vec3f>& vert);
void subdivide_lines(vector<vec2i>& lines, vector<vec4f>& vert);
void subdivide_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius);
// Subdivide triangle by splitting each triangle in four, creating new
// vertices for each edge.
void subdivide_triangles(vector<vec3i>& triangles, vector<float>& vert);
void subdivide_triangles(vector<vec3i>& triangles, vector<vec2f>& vert);
void subdivide_triangles(vector<vec3i>& triangles, vector<vec3f>& vert);
void subdivide_triangles(vector<vec3i>& triangles, vector<vec4f>& vert);
void subdivide_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius);
// Subdivide quads by splitting each quads in four, creating new
// vertices for each edge and for each face.
void subdivide_quads(vector<vec4i>& quads, vector<float>& vert);
void subdivide_quads(vector<vec4i>& quads, vector<vec2f>& vert);
void subdivide_quads(vector<vec4i>& quads, vector<vec3f>& vert);
void subdivide_quads(vector<vec4i>& quads, vector<vec4f>& vert);
void subdivide_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius);
// Subdivide beziers by splitting each segment in two.
void subdivide_beziers(vector<vec4i>& beziers, vector<float>& vert);
void subdivide_beziers(vector<vec4i>& beziers, vector<vec2f>& vert);
void subdivide_beziers(vector<vec4i>& beziers, vector<vec3f>& vert);
void subdivide_beziers(vector<vec4i>& beziers, vector<vec4f>& vert);
void subdivide_beziers(vector<vec4i>& beziers, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius);
// Subdivide quads using Carmull-Clark subdivision rules.
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<float>& vert, bool lock_boundary = false);
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec2f>& vert, bool lock_boundary = false);
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec3f>& vert, bool lock_boundary = false);
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec4f>& vert, bool lock_boundary = false);
void subdivide_catmullclark(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE SAMPLING
// -----------------------------------------------------------------------------
namespace yocto {

// Pick a point in a point set uniformly.
int  sample_points_element(int npoints, float re);
void sample_points_element_cdf(vector<float>& cdf, int npoints);
int  sample_points_element(const vector<float>& cdf, float re);

// Pick a point on lines uniformly.
void sample_lines_element_cdf(vector<float>& cdf, const vector<vec2i>& lines,
    const vector<vec3f>& positions);
pair<int, float> sample_lines_element(
    const vector<float>& cdf, float re, float ru);

// Pick a point on a triangle mesh uniformly.
void             sample_triangles_element_cdf(vector<float>& cdf,
                const vector<vec3i>& triangles, const vector<vec3f>& positions);
pair<int, vec2f> sample_triangles_element(
    const vector<float>& cdf, float re, const vec2f& ruv);

// Pick a point on a quad mesh uniformly.
void sample_quads_element_cdf(vector<float>& cdf, const vector<vec4i>& quads,
    const vector<vec3f>& positions);
pair<int, vec2f> sample_quads_element(
    const vector<float>& cdf, float re, const vec2f& ruv);
pair<int, vec2f> sample_quads_element(const vector<vec4i>& quads,
    const vector<float>& cdf, float re, const vec2f& ruv);

// Samples a set of points over a triangle/quad mesh uniformly. Returns pos,
// norm and texcoord of the sampled points.
void sample_triangles_points(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texturecoords,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    int npoints, int seed = 7);
void sample_quads_points(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texturecoords,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    int npoints, int seed = 7);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE GEODESICS
// -----------------------------------------------------------------------------
namespace yocto {

// Data structure used for geodesic computation
struct geodesic_solver {
    struct arc_ {
        int   node   = 0;
        float length = 0;
    };
    struct index_ {
        int node  = 0;
        int index = 0;
    };
    vector<vector<arc_>>   graph      = {};
    vector<vector<index_>> edge_index = {};
    vector<vec3f>          positions  = {};
    vector<vec2i>          edges      = {};
};

// Construct an edge graph
void init_geodesic_solver(geodesic_solver& solver,
    const vector<vec3i>& triangles, const vector<vec3f>& positions);
void compute_geodesic_distances(geodesic_solver& solver,
    vector<float>& distances, const vector<int>& sources);
void convert_distance_to_color(
    vector<vec4f>& colors, const vector<float>& distances);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Make examples shapes that are not watertight (besides quads).
// Return (triangles, quads, pos, norm, texcoord)
void make_quad_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize);
void make_quad_stack_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec2f& uvsize);
void make_floor_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize);
void make_floor_bent_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize, float radius);
void make_cube_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvsize);
void make_cube_rounded_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvsize, float radius);
void make_uvsphere_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize);
void make_sphere_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize);
void make_uvsphere_flipcap_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize, const vec2f& zflip);
void make_uvdisk_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize);
void make_disk_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize);
void make_disk_bulged_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize, float height);
void make_quad_bulged_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize, float height);
void make_uvcylinder_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec2f& size, const vec3f& uvsize);
void make_uvcylinder_rounded_shape(vector<vec4i>& quads,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, const vec3i& steps, const vec2f& size,
    const vec3f& uvsize, float radius);
void make_geodesic_sphere_shape(vector<vec3i>& triangles,
    vector<vec3f>& positions, vector<vec3f>& normals, int tesselation,
    float size);

// Make examples shapes with are watertight (good for subdivs).
void make_suzanne_shape(
    vector<vec4i>& quads, vector<vec3f>& positions, float size);
void make_cube_shape(
    vector<vec4i>& quads, vector<vec3f>& positions, const vec3f& size);

// Make facevarying example shapes that are watertight (good for subdivs).
void make_cube_fvshape(vector<vec4i>& quads_positions,
    vector<vec4i>& quads_normals, vector<vec4i>& quads_texturecoords,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, const vec3i& steps, const vec3f& size,
    const vec3f& uvsize);
void make_sphere_fvshape(vector<vec4i>& quads_positions,
    vector<vec4i>& quads_normals, vector<vec4i>& quads_texturecoords,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, int steps, float size, float uvsize);

// Generate lines set along a quad. Returns lines, pos, norm, texcoord, radius.
void make_lines_shape(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvsize,
    const vec2f& line_radius = {0.001f, 0.001f});

// Make point primitives. Returns points, pos, norm, texcoord, radius.
void make_point_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    float point_radius = 0.001f);
void make_points_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    int num, float uvsize, float point_radius = 0.001f);
void make_random_points_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    int num, const vec3f& size, float uvsize, float point_radius = 0.001f,
    uint64_t seed = 0);

// Make a bezier circle. Returns bezier, pos.
void make_bezier_circle_shape(
    vector<vec4i>& beziers, vector<vec3f>& positions, float size);

// Make a hair ball around a shape.  Returns lines, pos, norm, texcoord, radius.
// length: minimum and maximum length
// rad: minimum and maximum radius from base to tip
// noise: noise added to hair (strength/scale)
// clump: clump added to hair (number/strength)
// rotation: rotation added to hair (angle/strength)
void make_hair_shape(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    const vec2i& steps, const vector<vec3i>& striangles,
    const vector<vec4i>& squads, const vector<vec3f>& spos,
    const vector<vec3f>& snorm, const vector<vec2f>& stexcoord,
    const vec2f& length = {0.1f, 0.1f}, const vec2f& rad = {0.001f, 0.001f},
    const vec2f& noise = zero2f, const vec2f& clump = zero2f,
    const vec2f& rotation = zero2f, int seed = 7);

// Thickens a shape by copy9ing the shape content, rescaling it and flipping its
// normals. Note that this is very much not robust and only useful for trivial
// cases.
void make_shell_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, float thickness);

}  // namespace yocto

// ---------------------------------------------------------------------------//
//                                                                            //
//                             IMPLEMENTATION                                 //
//                                                                            //
// ---------------------------------------------------------------------------//

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF GEOMETRY UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Triangle tangent and bitangent from uv
inline pair<vec3f, vec3f> triangle_tangents_fromuv(const vec3f& p0,
    const vec3f& p1, const vec3f& p2, const vec2f& uv0, const vec2f& uv1,
    const vec2f& uv2) {
    // Follows the definition in http://www.terathon.com/code/tangent.html and
    // https://gist.github.com/aras-p/2843984
    // normal points up from texture space
    auto p   = p1 - p0;
    auto q   = p2 - p0;
    auto s   = vec2f{uv1.x - uv0.x, uv2.x - uv0.x};
    auto t   = vec2f{uv1.y - uv0.y, uv2.y - uv0.y};
    auto div = s.x * t.y - s.y * t.x;

    if (div != 0) {
        auto tu = vec3f{t.y * p.x - t.x * q.x, t.y * p.y - t.x * q.y,
                      t.y * p.z - t.x * q.z} /
                  div;
        auto tv = vec3f{s.x * q.x - s.y * p.x, s.x * q.y - s.y * p.y,
                      s.x * q.z - s.y * p.z} /
                  div;
        return {tu, tv};
    } else {
        return {{1, 0, 0}, {0, 1, 0}};
    }
}

// Quad tangent and bitangent from uv.
inline pair<vec3f, vec3f> quad_tangents_fromuv(const vec3f& p0, const vec3f& p1,
    const vec3f& p2, const vec3f& p3, const vec2f& uv0, const vec2f& uv1,
    const vec2f& uv2, const vec2f& uv3, const vec2f& current_uv) {
#if YOCTO_QUADS_AS_TRIANGLES
    if (current_uv.x + current_uv.y <= 1) {
        return triangle_tangents_fromuv(p0, p1, p3, uv0, uv1, uv3);
    } else {
        return triangle_tangents_fromuv(p2, p3, p1, uv2, uv3, uv1);
    }
#else
    return triangle_tangents_fromuv(p0, p1, p3, uv0, uv1, uv3);
#endif
}

}  // namespace yocto

#endif
