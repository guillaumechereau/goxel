//
// # Yocto/Shape: Shape utilities
//
// Yocto/Shape is a collection of utilities for manipulating shapes in 3D
// graphics, with a focus on triangle and quad meshes.
// Yocto/Shape is implemented in `yocto_shape.h` and `yocto_shape.cpp`.
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

#ifndef _YOCTO_SHAPE_H_
#define _YOCTO_SHAPE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "yocto_geometry.h"
#include "yocto_math.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::array;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE DATA AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Shape data represented as indexed meshes of elements.
// May contain either points, lines, triangles and quads.
struct shape_data {
  // element data
  vector<int>   points    = {};
  vector<vec2i> lines     = {};
  vector<vec3i> triangles = {};
  vector<vec4i> quads     = {};

  // vertex data
  vector<vec3f> positions = {};
  vector<vec3f> normals   = {};
  vector<vec2f> texcoords = {};
  vector<vec4f> colors    = {};
  vector<float> radius    = {};
  vector<vec4f> tangents  = {};
};

// Interpolate vertex data
vec3f eval_position(const shape_data& shape, int element, const vec2f& uv);
vec3f eval_normal(const shape_data& shape, int element, const vec2f& uv);
vec3f eval_tangent(const shape_data& shape, int element, const vec2f& uv);
vec2f eval_texcoord(const shape_data& shape, int element, const vec2f& uv);
vec4f eval_color(const shape_data& shape, int element, const vec2f& uv);
float eval_radius(const shape_data& shape, int element, const vec2f& uv);

// Evaluate element normals
vec3f eval_element_normal(const shape_data& shape, int element);

// Compute per-vertex normals/tangents for lines/triangles/quads.
vector<vec3f> compute_normals(const shape_data& shape);
void          compute_normals(vector<vec3f>& normals, const shape_data& shape);

// An unevaluated location on a shape
struct shape_point {
  int   element = 0;
  vec2f uv      = {0, 0};
};

// Shape sampling
vector<float> sample_shape_cdf(const shape_data& shape);
void          sample_shape_cdf(vector<float>& cdf, const shape_data& shape);
shape_point   sample_shape(const shape_data& shape, const vector<float>& cdf,
      float rn, const vec2f& ruv);
vector<shape_point> sample_shape(
    const shape_data& shape, int num_samples, uint64_t seed = 98729387);

// Conversions
shape_data quads_to_triangles(const shape_data& shape);
void       quads_to_triangles_inplace(shape_data& shape);

// Subdivision
shape_data subdivide_shape(
    const shape_data& shape, int subdivisions, bool catmullclark);

// Shape statistics
vector<string> shape_stats(const shape_data& shape, bool verbose = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// FACE-VARYING SHAPE DATA AND UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Shape data stored as a face-varying mesh
struct fvshape_data {
  // element data
  vector<vec4i> quadspos      = {};
  vector<vec4i> quadsnorm     = {};
  vector<vec4i> quadstexcoord = {};

  // vertex data
  vector<vec3f> positions = {};
  vector<vec3f> normals   = {};
  vector<vec2f> texcoords = {};
};

// Interpolate vertex data
vec3f eval_position(const fvshape_data& shape, int element, const vec2f& uv);
vec3f eval_normal(const fvshape_data& shape, int element, const vec2f& uv);
vec2f eval_texcoord(const shape_data& shape, int element, const vec2f& uv);

// Evaluate element normals
vec3f eval_element_normal(const fvshape_data& shape, int element);

// Compute per-vertex normals/tangents for lines/triangles/quads.
vector<vec3f> compute_normals(const fvshape_data& shape);
void compute_normals(vector<vec3f>& normals, const fvshape_data& shape);

// Conversions
shape_data fvshape_to_shape(
    const fvshape_data& shape, bool as_triangles = false);
fvshape_data shape_to_fvshape(const shape_data& shape);

// Subdivision
fvshape_data subdivide_fvshape(
    const fvshape_data& shape, int subdivisions, bool catmullclark);

// Shape statistics
vector<string> fvshape_stats(const fvshape_data& shape, bool verbose = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE SHAPES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a plane.
shape_data make_rect(const vec2i& steps = {1, 1}, const vec2f& scale = {1, 1},
    const vec2f& uvscale = {1, 1});
shape_data make_bulged_rect(const vec2i& steps = {1, 1},
    const vec2f& scale = {1, 1}, const vec2f& uvscale = {1, 1},
    float radius = 0.3f);
// Make a plane in the xz plane.
shape_data make_recty(const vec2i& steps = {1, 1}, const vec2f& scale = {1, 1},
    const vec2f& uvscale = {1, 1});
shape_data make_bulged_recty(const vec2i& steps = {1, 1},
    const vec2f& scale = {1, 1}, const vec2f& uvscale = {1, 1},
    float radius = 0.3f);
// Make a box.
shape_data make_box(const vec3i& steps = {1, 1, 1},
    const vec3f& scale = {1, 1, 1}, const vec3f& uvscale = {1, 1, 1});
shape_data make_rounded_box(const vec3i& steps = {1, 1, 1},
    const vec3f& scale = {1, 1, 1}, const vec3f& uvscale = {1, 1, 1},
    float radius = 0.3f);
// Make a quad stack
shape_data make_rect_stack(const vec3i& steps = {1, 1, 1},
    const vec3f& scale = {1, 1, 1}, const vec2f& uvscale = {1, 1});
// Make a floor.
shape_data make_floor(const vec2i& steps = {1, 1},
    const vec2f& scale = {10, 10}, const vec2f& uvscale = {10, 10});
shape_data make_bent_floor(const vec2i& steps = {1, 1},
    const vec2f& scale = {10, 10}, const vec2f& uvscale = {10, 10},
    float bent = 0.5f);
// Make a sphere.
shape_data make_sphere(int steps = 32, float scale = 1, float uvscale = 1);
// Make a sphere.
shape_data make_uvsphere(const vec2i& steps = {32, 32}, float scale = 1,
    const vec2f& uvscale = {1, 1});
shape_data make_uvspherey(const vec2i& steps = {32, 32}, float scale = 1,
    const vec2f& uvscale = {1, 1});
// Make a sphere with slipped caps.
shape_data make_capped_uvsphere(const vec2i& steps = {32, 32}, float scale = 1,
    const vec2f& uvscale = {1, 1}, float height = 0.3f);
shape_data make_capped_uvspherey(const vec2i& steps = {32, 32}, float scale = 1,
    const vec2f& uvscale = {1, 1}, float height = 0.3f);
// Make a disk
shape_data make_disk(int steps = 32, float scale = 1, float uvscale = 1);
// Make a bulged disk
shape_data make_bulged_disk(
    int steps = 32, float scale = 1, float uvscale = 1, float height = 0.3f);
// Make a uv disk
shape_data make_uvdisk(const vec2i& steps = {32, 32}, float scale = 1,
    const vec2f& uvscale = {1, 1});
// Make a uv cylinder
shape_data make_uvcylinder(const vec3i& steps = {32, 32, 32},
    const vec2f& scale = {1, 1}, const vec3f& uvscale = {1, 1, 1});
// Make a rounded uv cylinder
shape_data make_rounded_uvcylinder(const vec3i& steps = {32, 32, 32},
    const vec2f& scale = {1, 1}, const vec3f& uvscale = {1, 1, 1},
    float radius = 0.3f);

// Make a facevarying rect
fvshape_data make_fvrect(const vec2i& steps = {1, 1},
    const vec2f& scale = {1, 1}, const vec2f& uvscale = {1, 1});
// Make a facevarying box
fvshape_data make_fvbox(const vec3i& steps = {1, 1, 1},
    const vec3f& scale = {1, 1, 1}, const vec3f& uvscale = {1, 1, 1});
// Make a facevarying sphere
fvshape_data make_fvsphere(int steps = 32, float scale = 1, float uvscale = 1);

// Generate lines set along a quad. Returns lines, pos, norm, texcoord, radius.
shape_data make_lines(const vec2i& steps = {4, 65536},
    const vec2f& scale = {1, 1}, const vec2f& uvscale = {1, 1},
    const vec2f& radius = {0.001f, 0.001f});

// Make a point primitive. Returns points, pos, norm, texcoord, radius.
shape_data make_point(float radius = 0.001f);
// Make a point set on a grid. Returns points, pos, norm, texcoord, radius.
shape_data make_points(
    int num = 65536, float uvscale = 1, float radius = 0.001f);
shape_data make_points(const vec2i& steps = {256, 256},
    const vec2f& size = {1, 1}, const vec2f& uvscale = {1, 1},
    const vec2f& radius = {0.001f, 0.001f});
// Make random points in a cube. Returns points, pos, norm, texcoord, radius.
shape_data make_random_points(int num = 65536, const vec3f& size = {1, 1, 1},
    float uvscale = 1, float radius = 0.001f, uint64_t seed = 17);

// Predefined meshes
shape_data   make_monkey(float scale = 1, int subdivisions = 0);
shape_data   make_quad(float scale = 1, int subdivisions = 0);
shape_data   make_quady(float scale = 1, int subdivisions = 0);
shape_data   make_cube(float scale = 1, int subdivisions = 0);
fvshape_data make_fvcube(float scale = 1, int subdivisions = 0);
shape_data   make_geosphere(float scale = 1, int subdivisions = 0);

// Make a hair ball around a shape.
// length: minimum and maximum length
// rad: minimum and maximum radius from base to tip
// noise: noise added to hair (strength/scale)
// clump: clump added to hair (strength/number)
// rotation: rotation added to hair (angle/strength)
shape_data make_hair(const shape_data& shape, const vec2i& steps = {8, 65536},
    const vec2f& length = {0.1f, 0.1f}, const vec2f& radius = {0.001f, 0.001f},
    const vec2f& noise = {0, 10}, const vec2f& clump = {0, 128},
    const vec2f& rotation = {0, 0}, int seed = 7);

// Grow hairs around a shape
shape_data make_hair2(const shape_data& shape, const vec2i& steps = {8, 65536},
    const vec2f& length = {0.1f, 0.1f}, const vec2f& radius = {0.001f, 0.001f},
    float noise = 0, float gravity = 0.001f, int seed = 7);

// Convert points to small spheres and lines to small cylinders. This is
// intended for making very small primitives for display in interactive
// applications, so the spheres are low res.
shape_data points_to_spheres(
    const vector<vec3f>& vertices, int steps = 2, float scale = 0.01f);
shape_data polyline_to_cylinders(
    const vector<vec3f>& vertices, int steps = 4, float scale = 0.01f);
shape_data lines_to_cylinders(
    const vector<vec3f>& vertices, int steps = 4, float scale = 0.01f);
shape_data lines_to_cylinders(const vector<vec2i>& lines,
    const vector<vec3f>& positions, int steps = 4, float scale = 0.01f);

// Make a heightfield mesh.
shape_data make_heightfield(const vec2i& size, const vector<float>& height);
shape_data make_heightfield(const vec2i& size, const vector<vec4f>& color);

}  // namespace yocto

// -----------------------------------------------------------------------------
// COMPUTATION OF PER_VERTEX PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Compute per-vertex normals/tangents for lines/triangles/quads.
vector<vec3f> lines_tangents(
    const vector<vec2i>& lines, const vector<vec3f>& positions);
vector<vec3f> triangles_normals(
    const vector<vec3i>& triangles, const vector<vec3f>& positions);
vector<vec3f> quads_normals(
    const vector<vec4i>& quads, const vector<vec3f>& positions);
// Update normals and tangents
void lines_tangents(vector<vec3f>& tangents, const vector<vec2i>& lines,
    const vector<vec3f>& positions);
void triangles_normals(vector<vec3f>& normals, const vector<vec3i>& triangles,
    const vector<vec3f>& positions);
void quads_normals(vector<vec3f>& normals, const vector<vec4i>& quads,
    const vector<vec3f>& positions);

// Compute per-vertex tangent space for triangle meshes.
// Tangent space is defined by a four component vector.
// The first three components are the tangent with respect to the u texcoord.
// The fourth component is the sign of the tangent wrt the v texcoord.
// Tangent frame is useful in normal mapping.
vector<vec4f> triangle_tangent_spaces(const vector<vec3i>& triangles,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<vec2f>& texcoords);

// Apply skinning to vertex position and normals.
pair<vector<vec3f>, vector<vec3f>> skin_vertices(const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms);
// Apply skinning as specified in Khronos glTF.
pair<vector<vec3f>, vector<vec3f>> skin_matrices(const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms);
// Update skinning
void skin_vertices(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms);
void skin_matrices(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms);

}  // namespace yocto

// -----------------------------------------------------------------------------
// COMPUTATION OF VERTEX PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Flip vertex normals
vector<vec3f> flip_normals(const vector<vec3f>& normals);
// Flip face orientation
vector<vec3i> flip_triangles(const vector<vec3i>& triangles);
vector<vec4i> flip_quads(const vector<vec4i>& quads);
// Align vertex positions. Alignment is 0: none, 1: min, 2: max, 3: center.
vector<vec3f> align_vertices(
    const vector<vec3f>& positions, const vec3i& alignment);

}  // namespace yocto

// -----------------------------------------------------------------------------
// VECTOR HASHING
// -----------------------------------------------------------------------------
namespace std {

// Hash functor for vector for use with hash_map
template <>
struct hash<yocto::vec2i> {
  size_t operator()(const yocto::vec2i& v) const {
    static const auto hasher = std::hash<int>();
    auto              h      = (size_t)0;
    h ^= hasher(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};
template <>
struct hash<yocto::vec3i> {
  size_t operator()(const yocto::vec3i& v) const {
    static const auto hasher = std::hash<int>();
    auto              h      = (size_t)0;
    h ^= hasher(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};
template <>
struct hash<yocto::vec4i> {
  size_t operator()(const yocto::vec4i& v) const {
    static const auto hasher = std::hash<int>();
    auto              h      = (size_t)0;
    h ^= hasher(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(v.w) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

}  // namespace std

// -----------------------------------------------------------------------------
// EDGES AND ADJACENCIES
// -----------------------------------------------------------------------------
namespace yocto {

// Dictionary to store edge information. `index` is the index to the edge
// array, `edges` the array of edges and `nfaces` the number of adjacent faces.
// We store only bidirectional edges to keep the dictionary small. Use the
// functions below to access this data.
struct edge_map {
  struct edge_data {
    int index;
    int nfaces;
  };
  unordered_map<vec2i, edge_data> edges = {};
};

// Initialize an edge map with elements.
edge_map make_edge_map(const vector<vec3i>& triangles);
edge_map make_edge_map(const vector<vec4i>& quads);
void     insert_edges(edge_map& emap, const vector<vec3i>& triangles);
void     insert_edges(edge_map& emap, const vector<vec4i>& quads);
// Insert an edge and return its index
int insert_edge(edge_map& emap, const vec2i& edge);
// Get the edge index
int edge_index(const edge_map& emap, const vec2i& edge);
// Get edges and boundaries
int           num_edges(const edge_map& emap);
vector<vec2i> get_edges(const edge_map& emap);
vector<vec2i> get_boundary(const edge_map& emap);
vector<vec2i> get_edges(const vector<vec3i>& triangles);
vector<vec2i> get_edges(const vector<vec4i>& quads);
vector<vec2i> get_edges(
    const vector<vec3i>& triangles, const vector<vec4i>& quads);

// Build adjacencies between faces (sorted counter-clockwise)
vector<vec3i> face_adjacencies(const vector<vec3i>& triangles);

// Build adjacencies between vertices (sorted counter-clockwise)
vector<vector<int>> vertex_adjacencies(
    const vector<vec3i>& triangles, const vector<vec3i>& adjacencies);

// Compute boundaries as a list of loops (sorted counter-clockwise)
vector<vector<int>> ordered_boundaries(const vector<vec3i>& triangles,
    const vector<vec3i>& adjacency, int num_vertices);

// Build adjacencies between each vertex and its adjacent faces.
// Adjacencies are sorted counter-clockwise and have same starting points as
// vertex_adjacencies()
vector<vector<int>> vertex_to_faces_adjacencies(
    const vector<vec3i>& triangles, const vector<vec3i>& adjacencies);

}  // namespace yocto

// -----------------------------------------------------------------------------
// BVH, RAY INTERSECTION AND OVERLAP QUERIES
// -----------------------------------------------------------------------------
namespace yocto {

// BVH tree node containing its bounds, indices to the BVH arrays of either
// primitives or internal nodes, the node element type,
// and the split axis. Leaf and internal nodes are identical, except that
// indices refer to primitives for leaf nodes or other nodes for internal nodes.
struct bvh_node {
  bbox3f  bbox     = invalidb3f;
  int32_t start    = 0;
  int16_t num      = 0;
  int8_t  axis     = 0;
  bool    internal = false;
};

// BVH tree stored as a node array with the tree structure is encoded using
// array indices. BVH nodes indices refer to either the node array,
// for internal nodes, or the primitive arrays, for leaf nodes.
// Application data is not stored explicitly.
struct bvh_tree {
  vector<bvh_node> nodes      = {};
  vector<int>      primitives = {};
};

// Results of intersect_xxx and overlap_xxx functions that include hit flag,
// instance id, shape element id, shape element uv and intersection distance.
// The values are all set for scene intersection. Shape intersection does not
// set the instance id and element intersections do not set shape element id
// and the instance id. Results values are set only if hit is true.
struct shape_intersection {
  int   element  = -1;
  vec2f uv       = {0, 0};
  float distance = 0;
  bool  hit      = false;
};

// Make shape bvh
bvh_tree make_points_bvh(const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius);
bvh_tree make_lines_bvh(const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius);
bvh_tree make_triangles_bvh(const vector<vec3i>& triangles,
    const vector<vec3f>& positions, const vector<float>& radius);
bvh_tree make_quads_bvh(const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<float>& radius);

// Updates shape bvh for changes in positions and radia
void update_points_bvh(bvh_tree& bvh, const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius);
void update_lines_bvh(bvh_tree& bvh, const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius);
void update_triangles_bvh(bvh_tree& bvh, const vector<vec3i>& triangles,
    const vector<vec3f>& positions);
void update_quads_bvh(
    bvh_tree& bvh, const vector<vec4i>& quads, const vector<vec3f>& positions);

// Find a shape element or scene instances that intersects a ray,
// returning either the closest or any overlap depending on `find_any`.
// Returns the point distance, the instance id, the shape element index and
// the element barycentric coordinates.
shape_intersection intersect_points_bvh(const bvh_tree& bvh,
    const vector<int>& points, const vector<vec3f>& positions,
    const vector<float>& radius, const ray3f& ray, bool find_any = false);
shape_intersection intersect_lines_bvh(const bvh_tree& bvh,
    const vector<vec2i>& lines, const vector<vec3f>& positions,
    const vector<float>& radius, const ray3f& ray, bool find_any = false);
shape_intersection intersect_triangles_bvh(const bvh_tree& bvh,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const ray3f& ray, bool find_any = false);
shape_intersection intersect_quads_bvh(const bvh_tree& bvh,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const ray3f& ray, bool find_any = true);

// Find a shape element that overlaps a point within a given distance
// max distance, returning either the closest or any overlap depending on
// `find_any`. Returns the point distance, the instance id, the shape element
// index and the element barycentric coordinates.
shape_intersection overlap_points_bvh(const bvh_tree& bvh,
    const vector<int>& points, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any = false);
shape_intersection overlap_lines_bvh(const bvh_tree& bvh,
    const vector<vec2i>& lines, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any = false);
shape_intersection overlap_triangles_bvh(const bvh_tree& bvh,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any = false);
shape_intersection overlap_quads_bvh(const bvh_tree& bvh,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// HASH GRID AND NEAREST NEIGHBORS
// -----------------------------------------------------------------------------
namespace yocto {

// A sparse grid of cells, containing list of points. Cells are stored in
// a dictionary to get sparsity. Helpful for nearest neighboor lookups.
struct hash_grid {
  float                             cell_size     = 0;
  float                             cell_inv_size = 0;
  vector<vec3f>                     positions     = {};
  unordered_map<vec3i, vector<int>> cells         = {};
};

// Create a hash_grid
hash_grid make_hash_grid(float cell_size);
hash_grid make_hash_grid(const vector<vec3f>& positions, float cell_size);
// Inserts a point into the grid
int insert_vertex(hash_grid& grid, const vec3f& position);
// Finds the nearest neighbors within a given radius
void find_neighbors(const hash_grid& grid, vector<int>& neighbors,
    const vec3f& position, float max_radius);
void find_neighbors(const hash_grid& grid, vector<int>& neighbors, int vertex,
    float max_radius);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE ELEMENT CONVERSION AND GROUPING
// -----------------------------------------------------------------------------
namespace yocto {

// Convert quads to triangles
vector<vec3i> quads_to_triangles(const vector<vec4i>& quads);
// Convert triangles to quads by creating degenerate quads
vector<vec4i> triangles_to_quads(const vector<vec3i>& triangles);
// Convert beziers to lines using 3 lines for each bezier.
vector<vec4i> bezier_to_lines(vector<vec2i>& lines);

// Convert face-varying data to single primitives. Returns the quads indices
// and filled vectors for pos, norm and texcoord.
void split_facevarying(vector<vec4i>& split_quads,
    vector<vec3f>& split_positions, vector<vec3f>& split_normals,
    vector<vec2f>& split_texcoords, const vector<vec4i>& quadspos,
    const vector<vec4i>& quadsnorm, const vector<vec4i>& quadstexcoord,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<vec2f>& texcoords);

// Weld vertices within a threshold.
pair<vector<vec3f>, vector<int>> weld_vertices(
    const vector<vec3f>& positions, float threshold);
pair<vector<vec3i>, vector<vec3f>> weld_triangles(
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    float threshold);
pair<vector<vec4i>, vector<vec3f>> weld_quads(const vector<vec4i>& quads,
    const vector<vec3f>& positions, float threshold);

// Merge shape elements
void merge_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& tangents, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec2i>& merge_lines, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_tangents,
    const vector<vec2f>& merge_texturecoords,
    const vector<float>& merge_radius);
void merge_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec2i>& merge_triangles, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords);
void merge_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec4i>& merge_quads, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE SUBDIVISION
// -----------------------------------------------------------------------------
namespace yocto {

// Subdivide lines by splitting each line in half.
pair<vector<vec2i>, vector<float>> subdivide_lines(
    const vector<vec2i>& lines, const vector<float>& vertex);
pair<vector<vec2i>, vector<vec2f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec2f>& vertex);
pair<vector<vec2i>, vector<vec3f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec3f>& vertex);
pair<vector<vec2i>, vector<vec4f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec4f>& vertex);
// Subdivide triangle by splitting each triangle in four, creating new
// vertices for each edge.
pair<vector<vec3i>, vector<float>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<float>& vertex);
pair<vector<vec3i>, vector<vec2f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec2f>& vertex);
pair<vector<vec3i>, vector<vec3f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec3f>& vertex);
pair<vector<vec3i>, vector<vec4f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec4f>& vertex);
// Subdivide quads by splitting each quads in four, creating new
// vertices for each edge and for each face.
pair<vector<vec4i>, vector<float>> subdivide_quads(
    const vector<vec4i>& quads, const vector<float>& vertex);
pair<vector<vec4i>, vector<vec2f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec2f>& vertex);
pair<vector<vec4i>, vector<vec3f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec3f>& vertex);
pair<vector<vec4i>, vector<vec4f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec4f>& vertex);
// Subdivide beziers by splitting each segment in two.
pair<vector<vec4i>, vector<float>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<float>& vertex);
pair<vector<vec4i>, vector<vec2f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec2f>& vertex);
pair<vector<vec4i>, vector<vec3f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec3f>& vertex);
pair<vector<vec4i>, vector<vec4f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec4f>& vertex);
// Subdivide quads using Carmull-Clark subdivision rules.
pair<vector<vec4i>, vector<float>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<float>& vertex,
    bool lock_boundary = false);
pair<vector<vec4i>, vector<vec2f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec2f>& vertex,
    bool lock_boundary = false);
pair<vector<vec4i>, vector<vec3f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec3f>& vertex,
    bool lock_boundary = false);
pair<vector<vec4i>, vector<vec4f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec4f>& vertex,
    bool lock_boundary = false);

// Subdivide lines by splitting each line in half.
template <typename T>
inline pair<vector<vec2i>, vector<T>> subdivide_lines(
    const vector<vec2i>& lines, const vector<T>& vertices, int level);
// Subdivide triangle by splitting each triangle in four, creating new
// vertices for each edge.
template <typename T>
inline pair<vector<vec3i>, vector<T>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<T>& vertices, int level);
// Subdivide quads by splitting each quads in four, creating new
// vertices for each edge and for each face.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_quads(
    const vector<vec4i>& quads, const vector<T>& vertices, int level);
// Subdivide beziers by splitting each segment in two.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<T>& vertices, int level);
// Subdivide quads using Carmull-Clark subdivision rules.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<T>& vertices, int level,
    bool lock_boundary = false);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE SAMPLING
// -----------------------------------------------------------------------------
namespace yocto {

// Pick a point in a point set uniformly.
int           sample_points(int npoints, float re);
int           sample_points(const vector<float>& cdf, float re);
vector<float> sample_points_cdf(int npoints);
void          sample_points_cdf(vector<float>& cdf, int npoints);

// Pick a point on lines uniformly.
pair<int, float> sample_lines(const vector<float>& cdf, float re, float ru);
vector<float>    sample_lines_cdf(
       const vector<vec2i>& lines, const vector<vec3f>& positions);
void sample_lines_cdf(vector<float>& cdf, const vector<vec2i>& lines,
    const vector<vec3f>& positions);

// Pick a point on a triangle mesh uniformly.
pair<int, vec2f> sample_triangles(
    const vector<float>& cdf, float re, const vec2f& ruv);
vector<float> sample_triangles_cdf(
    const vector<vec3i>& triangles, const vector<vec3f>& positions);
void sample_triangles_cdf(vector<float>& cdf, const vector<vec3i>& triangles,
    const vector<vec3f>& positions);

// Pick a point on a quad mesh uniformly.
pair<int, vec2f> sample_quads(
    const vector<float>& cdf, float re, const vec2f& ruv);
vector<float> sample_quads_cdf(
    const vector<vec4i>& quads, const vector<vec3f>& positions);
void sample_quads_cdf(vector<float>& cdf, const vector<vec4i>& quads,
    const vector<vec3f>& positions);

// Samples a set of points over a triangle/quad mesh uniformly. Returns pos,
// norm and texcoord of the sampled points.
void sample_triangles(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texcoords,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texcoords, int npoints,
    int seed = 7);
void sample_quads(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texcoords,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texcoords, int npoints,
    int seed = 7);

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a quad.
void make_rect(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale);
void make_bulged_rect(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float height);
// Make a quad.
void make_recty(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale);
void make_bulged_recty(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float height);
// Make a cube.
void make_box(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec3f& uvscale);
void make_rounded_box(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec3f& uvscale, float radius);
void make_rect_stack(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec2f& uvscale);
void make_floor(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale);
void make_bent_floor(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float radius);
// Generate a sphere
void make_sphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale);
// Generate a uvsphere
void make_uvsphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale);
void make_capped_uvsphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale, float cap);
void make_uvspherey(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale);
void make_capped_uvspherey(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale, float cap);
// Generate a disk
void make_disk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale);
void make_bulged_disk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale, float height);
// Generate a uvdisk
void make_uvdisk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale);
// Generate a uvcylinder
void make_uvcylinder(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec2f& scale, const vec3f& uvscale);
// Generate a uvcylinder
void make_rounded_uvcylinder(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec2f& scale, const vec3f& uvscale, float radius);

// Generate lines set along a quad.
void make_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvscale,
    const vec2f& rad);

// Generate a point at the origin.
void make_point(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    float point_radius);

// Generate a point set with points placed at the origin with texcoords
// varying along u.
void make_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    int num, float uvscale, float point_radius);

// Generate a point set along a quad.
void make_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvscale,
    const vec2f& rad);

// Generate a point set.
void make_random_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    int num, const vec3f& size, float uvscale, float point_radius,
    uint64_t seed);

// Make a bezier circle. Returns bezier, pos.
void make_bezier_circle(
    vector<vec4i>& beziers, vector<vec3f>& positions, float size);

// Make fvquad
void make_fvrect(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvscale);
void make_fvbox(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvscale);
void make_fvsphere(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float size,
    float uvscale);

// Predefined meshes
void make_monkey(vector<vec4i>& quads, vector<vec3f>& positions, float scale,
    int subdivisions);
void make_quad(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions);
void make_quady(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions);
void make_cube(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions);
void make_fvcube(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions);
void make_geosphere(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, float scale, int subdivisions);

// Convert points to small spheres and lines to small cylinders. This is
// intended for making very small primitives for display in interactive
// applications, so the spheres are low res and without texcoords and normals.
void points_to_spheres(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps = 2, float scale = 0.01f);
void polyline_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps = 4, float scale = 0.01f);
void lines_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps = 4, float scale = 0.01f);
void lines_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec2i>& lines, const vector<vec3f>& vertices, int steps = 4,
    float scale = 0.01f);

// Make a hair ball around a shape
void make_hair(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec3i>& striangles, const vector<vec4i>& squads,
    const vector<vec3f>& spos, const vector<vec3f>& snorm,
    const vector<vec2f>& stexcoord, const vec2i& steps, const vec2f& len,
    const vec2f& rad, const vec2f& noise, const vec2f& clump,
    const vec2f& rotation, int seed);

// Grow hairs around a shape
void make_hair2(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec3i>& striangles, const vector<vec4i>& squads,
    const vector<vec3f>& spos, const vector<vec3f>& snorm,
    const vector<vec2f>& stexcoord, const vec2i& steps, const vec2f& len,
    const vec2f& rad, float noise, float gravity, int seed);

// Thickens a shape by copy9ing the shape content, rescaling it and flipping
// its normals. Note that this is very much not robust and only useful for
// trivial cases.
void make_shell(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float thickness);

// Make a heightfield mesh.
void make_heightfield(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& size,
    const vector<float>& height);
void make_heightfield(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& size,
    const vector<vec4f>& color);

}  // namespace yocto

// -----------------------------------------------------------------------------
//
//
// IMPLEMENTATION
//
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// SHAPE SUBDIVISION
// -----------------------------------------------------------------------------
namespace yocto {

// Subdivide lines by splitting each line in half.
template <typename T>
inline pair<vector<vec2i>, vector<T>> subdivide_lines(
    const vector<vec2i>& lines, const vector<T>& vertices, int level) {
  if (level < 1) return {lines, vertices};
  auto tess = pair{lines, vertices};
  for (auto idx : range(level)) tess = subdivide_lines(tess.first, tess.second);
  return tess;
}
// Subdivide triangle by splitting each triangle in four, creating new
// vertices for each edge.
template <typename T>
inline pair<vector<vec3i>, vector<T>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<T>& vertices, int level) {
  if (level < 1) return {triangles, vertices};
  auto tess = pair{triangles, vertices};
  for (auto idx : range(level))
    tess = subdivide_triangles(tess.first, tess.second);
  return tess;
}
// Subdivide quads by splitting each quads in four, creating new
// vertices for each edge and for each face.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_quads(
    const vector<vec4i>& quads, const vector<T>& vertices, int level) {
  if (level < 1) return {quads, vertices};
  auto tess = pair{quads, vertices};
  for (auto idx : range(level)) tess = subdivide_quads(tess.first, tess.second);
  return tess;
}
// Subdivide beziers by splitting each segment in two.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<T>& vertices, int level) {
  if (level < 1) return {beziers, vertices};
  auto tess = pair{beziers, vertices};
  for (auto idx : range(level))
    tess = subdivide_beziers(tess.first, tess.second);
  return tess;
}
// Subdivide quads using Carmull-Clark subdivision rules.
template <typename T>
inline pair<vector<vec4i>, vector<T>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<T>& vertices, int level,
    bool lock_boundary) {
  if (level < 1) return {quads, vertices};
  auto tess = pair{quads, vertices};
  for (auto idx : range(level))
    tess = subdivide_catmullclark(tess.first, tess.second, lock_boundary);
  return tess;
}

}  // namespace yocto

#endif
