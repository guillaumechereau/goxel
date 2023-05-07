//
// Implementation for Yocto/Shape
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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_shape.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>

#include "yocto_geometry.h"
#include "yocto_modelio.h"
#include "yocto_noise.h"
#include "yocto_sampling.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::deque;
using namespace std::string_literals;

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FO SHAPE PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Interpolate vertex data
vec3f eval_position(const shape_data& shape, int element, const vec2f& uv) {
  if (!shape.points.empty()) {
    auto& point = shape.points[element];
    return shape.positions[point];
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return interpolate_line(
        shape.positions[line.x], shape.positions[line.y], uv.x);
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return interpolate_triangle(shape.positions[triangle.x],
        shape.positions[triangle.y], shape.positions[triangle.z], uv);
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return interpolate_quad(shape.positions[quad.x], shape.positions[quad.y],
        shape.positions[quad.z], shape.positions[quad.w], uv);
  } else {
    return {0, 0, 0};
  }
}

vec3f eval_normal(const shape_data& shape, int element, const vec2f& uv) {
  if (shape.normals.empty()) return eval_element_normal(shape, element);
  if (!shape.points.empty()) {
    auto& point = shape.points[element];
    return normalize(shape.normals[point]);
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return normalize(
        interpolate_line(shape.normals[line.x], shape.normals[line.y], uv.x));
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return normalize(interpolate_triangle(shape.normals[triangle.x],
        shape.normals[triangle.y], shape.normals[triangle.z], uv));
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return normalize(
        interpolate_quad(shape.normals[quad.x], shape.normals[quad.y],
            shape.normals[quad.z], shape.normals[quad.w], uv));
  } else {
    return {0, 0, 1};
  }
}

vec3f eval_tangent(const shape_data& shape, int element, const vec2f& uv) {
  return eval_normal(shape, element, uv);
}

vec2f eval_texcoord(const shape_data& shape, int element, const vec2f& uv) {
  if (shape.texcoords.empty()) return uv;
  if (!shape.points.empty()) {
    auto& point = shape.points[element];
    return shape.texcoords[point];
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return interpolate_line(
        shape.texcoords[line.x], shape.texcoords[line.y], uv.x);
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return interpolate_triangle(shape.texcoords[triangle.x],
        shape.texcoords[triangle.y], shape.texcoords[triangle.z], uv);
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return interpolate_quad(shape.texcoords[quad.x], shape.texcoords[quad.y],
        shape.texcoords[quad.z], shape.texcoords[quad.w], uv);
  } else {
    return uv;
  }
}

vec4f eval_color(const shape_data& shape, int element, const vec2f& uv) {
  if (shape.colors.empty()) return {1, 1, 1, 1};
  if (!shape.points.empty()) {
    auto& point = shape.points[element];
    return shape.colors[point];
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return interpolate_line(shape.colors[line.x], shape.colors[line.y], uv.x);
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return interpolate_triangle(shape.colors[triangle.x],
        shape.colors[triangle.y], shape.colors[triangle.z], uv);
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return interpolate_quad(shape.colors[quad.x], shape.colors[quad.y],
        shape.colors[quad.z], shape.colors[quad.w], uv);
  } else {
    return {0, 0};
  }
}

float eval_radius(const shape_data& shape, int element, const vec2f& uv) {
  if (shape.radius.empty()) return 0;
  if (!shape.points.empty()) {
    auto& point = shape.points[element];
    return shape.radius[point];
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return interpolate_line(shape.radius[line.x], shape.radius[line.y], uv.x);
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return interpolate_triangle(shape.radius[triangle.x],
        shape.radius[triangle.y], shape.radius[triangle.z], uv);
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return interpolate_quad(shape.radius[quad.x], shape.radius[quad.y],
        shape.radius[quad.z], shape.radius[quad.w], uv);
  } else {
    return 0;
  }
}

// Evaluate element normals
vec3f eval_element_normal(const shape_data& shape, int element) {
  if (!shape.points.empty()) {
    return {0, 0, 1};
  } else if (!shape.lines.empty()) {
    auto& line = shape.lines[element];
    return line_tangent(shape.positions[line.x], shape.positions[line.y]);
  } else if (!shape.triangles.empty()) {
    auto& triangle = shape.triangles[element];
    return triangle_normal(shape.positions[triangle.x],
        shape.positions[triangle.y], shape.positions[triangle.z]);
  } else if (!shape.quads.empty()) {
    auto& quad = shape.quads[element];
    return quad_normal(shape.positions[quad.x], shape.positions[quad.y],
        shape.positions[quad.z], shape.positions[quad.w]);
  } else {
    return {0, 0, 0};
  }
}

// Compute per-vertex normals/tangents for lines/triangles/quads.
vector<vec3f> compute_normals(const shape_data& shape) {
  if (!shape.points.empty()) {
    return vector<vec3f>(shape.positions.size(), {0, 0, 1});
  } else if (!shape.lines.empty()) {
    return lines_tangents(shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    return triangles_normals(shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    return quads_normals(shape.quads, shape.positions);
  } else {
    return vector<vec3f>(shape.positions.size(), {0, 0, 1});
  }
}
void compute_normals(vector<vec3f>& normals, const shape_data& shape) {
  if (!shape.points.empty()) {
    normals.assign(shape.positions.size(), {0, 0, 1});
  } else if (!shape.lines.empty()) {
    lines_tangents(normals, shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    triangles_normals(normals, shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    quads_normals(normals, shape.quads, shape.positions);
  } else {
    normals.assign(shape.positions.size(), {0, 0, 1});
  }
}

// Shape sampling
vector<float> sample_shape_cdf(const shape_data& shape) {
  if (!shape.points.empty()) {
    return sample_points_cdf((int)shape.points.size());
  } else if (!shape.lines.empty()) {
    return sample_lines_cdf(shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    return sample_triangles_cdf(shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    return sample_quads_cdf(shape.quads, shape.positions);
  } else {
    return sample_points_cdf((int)shape.positions.size());
  }
}

void sample_shape_cdf(vector<float>& cdf, const shape_data& shape) {
  if (!shape.points.empty()) {
    sample_points_cdf(cdf, (int)shape.points.size());
  } else if (!shape.lines.empty()) {
    sample_lines_cdf(cdf, shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    sample_triangles_cdf(cdf, shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    sample_quads_cdf(cdf, shape.quads, shape.positions);
  } else {
    sample_points_cdf(cdf, (int)shape.positions.size());
  }
}

shape_point sample_shape(const shape_data& shape, const vector<float>& cdf,
    float rn, const vec2f& ruv) {
  if (!shape.points.empty()) {
    auto element = sample_points(cdf, rn);
    return {element, {0, 0}};
  } else if (!shape.lines.empty()) {
    auto [element, u] = sample_lines(cdf, rn, ruv.x);
    return {element, {u, 0}};
  } else if (!shape.triangles.empty()) {
    auto [element, uv] = sample_triangles(cdf, rn, ruv);
    return {element, uv};
  } else if (!shape.quads.empty()) {
    auto [element, uv] = sample_quads(cdf, rn, ruv);
    return {element, uv};
  } else {
    auto element = sample_points(cdf, rn);
    return {element, {0, 0}};
  }
}

vector<shape_point> sample_shape(
    const shape_data& shape, int num_samples, uint64_t seed) {
  auto cdf    = sample_shape_cdf(shape);
  auto points = vector<shape_point>(num_samples);
  auto rng    = make_rng(seed);
  for (auto& point : points) {
    point = sample_shape(shape, cdf, rand1f(rng), rand2f(rng));
  }
  return points;
}

// Conversions
shape_data quads_to_triangles(const shape_data& shape) {
  auto result = shape;
  if (!shape.quads.empty()) {
    result.triangles = quads_to_triangles(shape.quads);
    result.quads     = {};
  }
  return result;
}
void quads_to_triangles_inplace(shape_data& shape) {
  if (shape.quads.empty()) return;
  shape.triangles = quads_to_triangles(shape.quads);
  shape.quads     = {};
}

// Subdivision
shape_data subdivide_shape(
    const shape_data& shape, int subdivisions, bool catmullclark) {
  // This should probably be re-implemented in a faster fashion,
  // but how it is not obvious
  if (subdivisions == 0) return shape;
  auto subdivided = shape_data{};
  if (!subdivided.points.empty()) {
    subdivided = shape;
  } else if (!subdivided.lines.empty()) {
    std::tie(std::ignore, subdivided.normals) = subdivide_lines(
        shape.lines, shape.normals, subdivisions);
    std::tie(std::ignore, subdivided.texcoords) = subdivide_lines(
        shape.lines, shape.texcoords, subdivisions);
    std::tie(std::ignore, subdivided.colors) = subdivide_lines(
        shape.lines, shape.colors, subdivisions);
    std::tie(std::ignore, subdivided.radius) = subdivide_lines(
        subdivided.lines, shape.radius, subdivisions);
    std::tie(subdivided.lines, subdivided.positions) = subdivide_lines(
        shape.lines, shape.positions, subdivisions);
  } else if (!subdivided.triangles.empty()) {
    std::tie(std::ignore, subdivided.normals) = subdivide_triangles(
        shape.triangles, shape.normals, subdivisions);
    std::tie(std::ignore, subdivided.texcoords) = subdivide_triangles(
        shape.triangles, shape.texcoords, subdivisions);
    std::tie(std::ignore, subdivided.colors) = subdivide_triangles(
        shape.triangles, shape.colors, subdivisions);
    std::tie(std::ignore, subdivided.radius) = subdivide_triangles(
        shape.triangles, shape.radius, subdivisions);
    std::tie(subdivided.triangles, subdivided.positions) = subdivide_triangles(
        shape.triangles, shape.positions, subdivisions);
  } else if (!subdivided.quads.empty() && !catmullclark) {
    std::tie(std::ignore, subdivided.normals) = subdivide_quads(
        shape.quads, shape.normals, subdivisions);
    std::tie(std::ignore, subdivided.texcoords) = subdivide_quads(
        shape.quads, shape.texcoords, subdivisions);
    std::tie(std::ignore, subdivided.colors) = subdivide_quads(
        shape.quads, shape.colors, subdivisions);
    std::tie(std::ignore, subdivided.radius) = subdivide_quads(
        shape.quads, shape.radius, subdivisions);
    std::tie(subdivided.quads, subdivided.positions) = subdivide_quads(
        shape.quads, shape.positions, subdivisions);
  } else if (!subdivided.quads.empty() && catmullclark) {
    std::tie(std::ignore, subdivided.normals) = subdivide_catmullclark(
        shape.quads, shape.normals, subdivisions);
    std::tie(std::ignore, subdivided.texcoords) = subdivide_catmullclark(
        shape.quads, shape.texcoords, subdivisions);
    std::tie(std::ignore, subdivided.colors) = subdivide_catmullclark(
        shape.quads, shape.colors, subdivisions);
    std::tie(std::ignore, subdivided.radius) = subdivide_catmullclark(
        shape.quads, shape.radius, subdivisions);
    std::tie(subdivided.quads, subdivided.positions) = subdivide_catmullclark(
        shape.quads, shape.positions, subdivisions);
  } else {
    // empty shape
  }
  return subdivided;
}

vector<string> shape_stats(const shape_data& shape, bool verbose) {
  auto format = [](auto num) {
    auto str = std::to_string(num);
    while (str.size() < 13) str = " " + str;
    return str;
  };
  auto format3 = [](auto num) {
    auto str = std::to_string(num.x) + " " + std::to_string(num.y) + " " +
               std::to_string(num.z);
    while (str.size() < 13) str = " " + str;
    return str;
  };

  auto bbox = invalidb3f;
  for (auto& pos : shape.positions) bbox = merge(bbox, pos);

  auto stats = vector<string>{};
  stats.push_back("points:       " + format(shape.points.size()));
  stats.push_back("lines:        " + format(shape.lines.size()));
  stats.push_back("triangles:    " + format(shape.triangles.size()));
  stats.push_back("quads:        " + format(shape.quads.size()));
  stats.push_back("positions:    " + format(shape.positions.size()));
  stats.push_back("normals:      " + format(shape.normals.size()));
  stats.push_back("texcoords:    " + format(shape.texcoords.size()));
  stats.push_back("colors:       " + format(shape.colors.size()));
  stats.push_back("radius:       " + format(shape.radius.size()));
  stats.push_back("center:       " + format3(center(bbox)));
  stats.push_back("size:         " + format3(size(bbox)));
  stats.push_back("min:          " + format3(bbox.min));
  stats.push_back("max:          " + format3(bbox.max));

  return stats;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR FVSHAPE PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Interpolate vertex data
vec3f eval_position(const fvshape_data& shape, int element, const vec2f& uv) {
  if (!shape.quadspos.empty()) {
    auto& quad = shape.quadspos[element];
    return interpolate_quad(shape.positions[quad.x], shape.positions[quad.y],
        shape.positions[quad.z], shape.positions[quad.w], uv);
  } else {
    return {0, 0, 0};
  }
}

vec3f eval_normal(const fvshape_data& shape, int element, const vec2f& uv) {
  if (shape.normals.empty()) return eval_element_normal(shape, element);
  if (!shape.quadspos.empty()) {
    auto& quad = shape.quadsnorm[element];
    return normalize(
        interpolate_quad(shape.normals[quad.x], shape.normals[quad.y],
            shape.normals[quad.z], shape.normals[quad.w], uv));
  } else {
    return {0, 0, 1};
  }
}

vec2f eval_texcoord(const fvshape_data& shape, int element, const vec2f& uv) {
  if (shape.texcoords.empty()) return uv;
  if (!shape.quadspos.empty()) {
    auto& quad = shape.quadstexcoord[element];
    return interpolate_quad(shape.texcoords[quad.x], shape.texcoords[quad.y],
        shape.texcoords[quad.z], shape.texcoords[quad.w], uv);
  } else {
    return uv;
  }
}

// Evaluate element normals
vec3f eval_element_normal(const fvshape_data& shape, int element) {
  if (!shape.quadspos.empty()) {
    auto& quad = shape.quadspos[element];
    return quad_normal(shape.positions[quad.x], shape.positions[quad.y],
        shape.positions[quad.z], shape.positions[quad.w]);
  } else {
    return {0, 0, 0};
  }
}

// Compute per-vertex normals/tangents for lines/triangles/quads.
vector<vec3f> compute_normals(const fvshape_data& shape) {
  if (!shape.quadspos.empty()) {
    return quads_normals(shape.quadspos, shape.positions);
  } else {
    return vector<vec3f>(shape.positions.size(), {0, 0, 1});
  }
}
void compute_normals(vector<vec3f>& normals, const fvshape_data& shape) {
  if (!shape.quadspos.empty()) {
    quads_normals(normals, shape.quadspos, shape.positions);
  } else {
    normals.assign(shape.positions.size(), {0, 0, 1});
  }
}

// Conversions
shape_data fvshape_to_shape(const fvshape_data& fvshape, bool as_triangles) {
  auto shape = shape_data{};
  split_facevarying(shape.quads, shape.positions, shape.normals,
      shape.texcoords, fvshape.quadspos, fvshape.quadsnorm,
      fvshape.quadstexcoord, fvshape.positions, fvshape.normals,
      fvshape.texcoords);
  return shape;
}
fvshape_data shape_to_fvshape(const shape_data& shape) {
  if (!shape.points.empty() || !shape.lines.empty())
    throw std::invalid_argument{"cannor convert shape"};
  auto fvshape          = fvshape_data{};
  fvshape.positions     = shape.positions;
  fvshape.normals       = shape.normals;
  fvshape.texcoords     = shape.texcoords;
  fvshape.quadspos      = !shape.quads.empty() ? shape.quads
                                               : triangles_to_quads(shape.triangles);
  fvshape.quadsnorm     = !shape.normals.empty() ? fvshape.quadspos
                                                 : vector<vec4i>{};
  fvshape.quadstexcoord = !shape.texcoords.empty() ? fvshape.quadspos
                                                   : vector<vec4i>{};
  return fvshape;
}

// Subdivision
fvshape_data subdivide_fvshape(
    const fvshape_data& shape, int subdivisions, bool catmullclark) {
  // This should be probably re-implemeneted in a faster fashion.
  if (subdivisions == 0) return shape;
  auto subdivided = fvshape_data{};
  if (!catmullclark) {
    std::tie(subdivided.quadspos, subdivided.positions) = subdivide_quads(
        shape.quadspos, shape.positions, subdivisions);
    std::tie(subdivided.quadsnorm, subdivided.normals) = subdivide_quads(
        shape.quadsnorm, shape.normals, subdivisions);
    std::tie(subdivided.quadstexcoord, subdivided.texcoords) = subdivide_quads(
        shape.quadstexcoord, shape.texcoords, subdivisions);
  } else {
    std::tie(subdivided.quadspos, subdivided.positions) =
        subdivide_catmullclark(shape.quadspos, shape.positions, subdivisions);
    std::tie(subdivided.quadsnorm, subdivided.normals) = subdivide_catmullclark(
        shape.quadsnorm, shape.normals, subdivisions);
    std::tie(subdivided.quadstexcoord, subdivided.texcoords) =
        subdivide_catmullclark(
            shape.quadstexcoord, shape.texcoords, subdivisions, true);
  }
  return subdivided;
}

vector<string> fvshape_stats(const fvshape_data& shape, bool verbose) {
  auto format = [](auto num) {
    auto str = std::to_string(num);
    while (str.size() < 13) str = " " + str;
    return str;
  };
  auto format3 = [](auto num) {
    auto str = std::to_string(num.x) + " " + std::to_string(num.y) + " " +
               std::to_string(num.z);
    while (str.size() < 13) str = " " + str;
    return str;
  };

  auto bbox = invalidb3f;
  for (auto& pos : shape.positions) bbox = merge(bbox, pos);

  auto stats = vector<string>{};
  stats.push_back("fvquads:      " + format(shape.quadspos.size()));
  stats.push_back("positions:    " + format(shape.positions.size()));
  stats.push_back("normals:      " + format(shape.normals.size()));
  stats.push_back("texcoords:    " + format(shape.texcoords.size()));
  stats.push_back("center:       " + format3(center(bbox)));
  stats.push_back("size:         " + format3(size(bbox)));
  stats.push_back("min:          " + format3(bbox.min));
  stats.push_back("max:          " + format3(bbox.max));

  return stats;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR SHAPE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a tesselated rectangle. Useful in other subdivisions.
static shape_data make_quads(
    const vec2i& steps, const vec2f& scale, const vec2f& uvscale) {
  auto shape = shape_data{};

  shape.positions.resize((steps.x + 1) * (steps.y + 1));
  shape.normals.resize((steps.x + 1) * (steps.y + 1));
  shape.texcoords.resize((steps.x + 1) * (steps.y + 1));
  for (auto j : range(steps.y + 1)) {
    for (auto i : range(steps.x + 1)) {
      auto uv = vec2f{i / (float)steps.x, j / (float)steps.y};
      shape.positions[j * (steps.x + 1) + i] = {
          (2 * uv.x - 1) * scale.x, (2 * uv.y - 1) * scale.y, 0};
      shape.normals[j * (steps.x + 1) + i]   = {0, 0, 1};
      shape.texcoords[j * (steps.x + 1) + i] = vec2f{uv.x, 1 - uv.y} * uvscale;
    }
  }

  shape.quads.resize(steps.x * steps.y);
  for (auto j : range(steps.y)) {
    for (auto i : range(steps.x)) {
      shape.quads[j * steps.x + i] = {j * (steps.x + 1) + i,
          j * (steps.x + 1) + i + 1, (j + 1) * (steps.x + 1) + i + 1,
          (j + 1) * (steps.x + 1) + i};
    }
  }

  return shape;
}

// Merge shape elements
void merge_shape_inplace(shape_data& shape, const shape_data& merge) {
  auto offset = (int)shape.positions.size();
  for (auto& p : merge.points) shape.points.push_back(p + offset);
  for (auto& l : merge.lines)
    shape.lines.push_back({l.x + offset, l.y + offset});
  for (auto& t : merge.triangles)
    shape.triangles.push_back({t.x + offset, t.y + offset, t.z + offset});
  for (auto& q : merge.quads)
    shape.quads.push_back(
        {q.x + offset, q.y + offset, q.z + offset, q.w + offset});
  shape.positions.insert(
      shape.positions.end(), merge.positions.begin(), merge.positions.end());
  shape.tangents.insert(
      shape.tangents.end(), merge.tangents.begin(), merge.tangents.end());
  shape.texcoords.insert(
      shape.texcoords.end(), merge.texcoords.begin(), merge.texcoords.end());
  shape.colors.insert(
      shape.colors.end(), merge.colors.begin(), merge.colors.end());
  shape.radius.insert(
      shape.radius.end(), merge.radius.begin(), merge.radius.end());
}

// Make a plane.
shape_data make_rect(
    const vec2i& steps, const vec2f& scale, const vec2f& uvscale) {
  return make_quads(steps, scale, uvscale);
}
shape_data make_bulged_rect(const vec2i& steps, const vec2f& scale,
    const vec2f& uvscale, float height) {
  auto shape = make_rect(steps, scale, uvscale);
  if (height != 0) {
    height      = min(height, min(scale));
    auto radius = (1 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i : range(shape.positions.size())) {
      auto pn            = normalize(shape.positions[i] - center);
      shape.positions[i] = center + pn * radius;
      shape.normals[i]   = pn;
    }
  }
  return shape;
}

// Make a plane in the xz plane.
shape_data make_recty(
    const vec2i& steps, const vec2f& scale, const vec2f& uvscale) {
  auto shape = make_rect(steps, scale, uvscale);
  for (auto& position : shape.positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : shape.normals) normal = {normal.x, normal.z, normal.y};
  return shape;
}
shape_data make_bulged_recty(const vec2i& steps, const vec2f& scale,
    const vec2f& uvscale, float height) {
  auto shape = make_bulged_rect(steps, scale, uvscale, height);
  for (auto& position : shape.positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : shape.normals) normal = {normal.x, normal.z, normal.y};
  return shape;
}

// Make a box.
shape_data make_box(
    const vec3i& steps, const vec3f& scale, const vec3f& uvscale) {
  auto shape  = shape_data{};
  auto qshape = shape_data{};
  // + z
  qshape = make_rect(
      {steps.x, steps.y}, {scale.x, scale.y}, {uvscale.x, uvscale.y});
  for (auto& p : qshape.positions) p = {p.x, p.y, scale.z};
  for (auto& n : qshape.normals) n = {0, 0, 1};
  merge_shape_inplace(shape, qshape);
  // - z
  qshape = make_rect(
      {steps.x, steps.y}, {scale.x, scale.y}, {uvscale.x, uvscale.y});
  for (auto& p : qshape.positions) p = {-p.x, p.y, -scale.z};
  for (auto& n : qshape.normals) n = {0, 0, -1};
  merge_shape_inplace(shape, qshape);
  // + x
  qshape = make_rect(
      {steps.z, steps.y}, {scale.z, scale.y}, {uvscale.z, uvscale.y});
  for (auto& p : qshape.positions) p = {scale.x, p.y, -p.x};
  for (auto& n : qshape.normals) n = {1, 0, 0};
  merge_shape_inplace(shape, qshape);
  // - x
  qshape = make_rect(
      {steps.z, steps.y}, {scale.z, scale.y}, {uvscale.z, uvscale.y});
  for (auto& p : qshape.positions) p = {-scale.x, p.y, p.x};
  for (auto& n : qshape.normals) n = {-1, 0, 0};
  merge_shape_inplace(shape, qshape);
  // + y
  qshape = make_rect(
      {steps.x, steps.z}, {scale.x, scale.z}, {uvscale.x, uvscale.z});
  for (auto i : range(qshape.positions.size())) {
    qshape.positions[i] = {
        qshape.positions[i].x, scale.y, -qshape.positions[i].y};
    qshape.normals[i] = {0, 1, 0};
  }
  merge_shape_inplace(shape, qshape);
  // - y
  qshape = make_rect(
      {steps.x, steps.z}, {scale.x, scale.z}, {uvscale.x, uvscale.z});
  for (auto i : range(qshape.positions.size())) {
    qshape.positions[i] = {
        qshape.positions[i].x, -scale.y, qshape.positions[i].y};
    qshape.normals[i] = {0, -1, 0};
  }
  merge_shape_inplace(shape, qshape);
  return shape;
}
shape_data make_rounded_box(const vec3i& steps, const vec3f& scale,
    const vec3f& uvscale, float radius) {
  auto shape = make_box(steps, scale, uvscale);
  if (radius != 0) {
    radius = min(radius, min(scale));
    auto c = scale - radius;
    for (auto i : range(shape.positions.size())) {
      auto pc = vec3f{abs(shape.positions[i].x), abs(shape.positions[i].y),
          abs(shape.positions[i].z)};
      auto ps = vec3f{shape.positions[i].x < 0 ? -1.0f : 1.0f,
          shape.positions[i].y < 0 ? -1.0f : 1.0f,
          shape.positions[i].z < 0 ? -1.0f : 1.0f};
      if (pc.x >= c.x && pc.y >= c.y && pc.z >= c.z) {
        auto pn            = normalize(pc - c);
        shape.positions[i] = c + radius * pn;
        shape.normals[i]   = pn;
      } else if (pc.x >= c.x && pc.y >= c.y) {
        auto pn            = normalize((pc - c) * vec3f{1, 1, 0});
        shape.positions[i] = {c.x + radius * pn.x, c.y + radius * pn.y, pc.z};
        shape.normals[i]   = pn;
      } else if (pc.x >= c.x && pc.z >= c.z) {
        auto pn            = normalize((pc - c) * vec3f{1, 0, 1});
        shape.positions[i] = {c.x + radius * pn.x, pc.y, c.z + radius * pn.z};
        shape.normals[i]   = pn;
      } else if (pc.y >= c.y && pc.z >= c.z) {
        auto pn            = normalize((pc - c) * vec3f{0, 1, 1});
        shape.positions[i] = {pc.x, c.y + radius * pn.y, c.z + radius * pn.z};
        shape.normals[i]   = pn;
      } else {
        continue;
      }
      shape.positions[i] *= ps;
      shape.normals[i] *= ps;
    }
  }
  return shape;
}

// Make a quad stack
shape_data make_rect_stack(
    const vec3i& steps, const vec3f& scale, const vec2f& uvscale) {
  auto shape  = shape_data{};
  auto qshape = shape_data{};
  for (auto i : range(steps.z + 1)) {
    qshape = make_rect({steps.x, steps.y}, {scale.x, scale.y}, uvscale);
    for (auto& p : qshape.positions)
      p.z = (-1 + 2 * (float)i / steps.z) * scale.z;
    merge_shape_inplace(shape, qshape);
  }
  return shape;
}

// Make a floor.
shape_data make_floor(
    const vec2i& steps, const vec2f& scale, const vec2f& uvscale) {
  auto shape = make_rect(steps, scale, uvscale);
  for (auto& position : shape.positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : shape.normals) normal = {normal.x, normal.z, normal.y};
  return shape;
}
shape_data make_bent_floor(const vec2i& steps, const vec2f& scale,
    const vec2f& uvscale, float radius) {
  auto shape = make_floor(steps, scale, uvscale);
  if (radius != 0) {
    radius     = min(radius, scale.y);
    auto start = (scale.y - radius) / 2;
    auto end   = start + radius;
    for (auto i : range(shape.positions.size())) {
      if (shape.positions[i].z < -end) {
        shape.positions[i] = {
            shape.positions[i].x, -shape.positions[i].z - end + radius, -end};
        shape.normals[i] = {0, 0, 1};
      } else if (shape.positions[i].z < -start &&
                 shape.positions[i].z >= -end) {
        auto phi = (pif / 2) * (-shape.positions[i].z - start) / radius;
        shape.positions[i] = {shape.positions[i].x, -cos(phi) * radius + radius,
            -sin(phi) * radius - start};
        shape.normals[i]   = {0, cos(phi), sin(phi)};
      } else {
      }
    }
  }
  return shape;
}

// Make a sphere.
shape_data make_sphere(int steps, float scale, float uvscale) {
  auto shape = make_box({steps, steps, steps}, {scale, scale, scale},
      {uvscale, uvscale, uvscale});
  for (auto& p : shape.positions) p = normalize(p) * scale;
  shape.normals = shape.positions;
  for (auto& n : shape.normals) n = normalize(n);
  return shape;
}

// Make a sphere.
shape_data make_uvsphere(
    const vec2i& steps, float scale, const vec2f& uvscale) {
  auto shape = make_rect(steps, {1, 1});
  for (auto i : range(shape.positions.size())) {
    auto uv = shape.texcoords[i];
    auto a  = vec2f{2 * pif * uv.x, pif * (1 - uv.y)};
    shape.positions[i] =
        vec3f{cos(a.x) * sin(a.y), sin(a.x) * sin(a.y), cos(a.y)} * scale;
    shape.normals[i]   = normalize(shape.positions[i]);
    shape.texcoords[i] = uv * uvscale;
  }
  return shape;
}

// Make a sphere.
shape_data make_uvspherey(
    const vec2i& steps, float scale, const vec2f& uvscale) {
  auto shape = make_uvsphere(steps, scale, uvscale);
  for (auto& position : shape.positions)
    position = {position.x, position.z, position.y};
  for (auto& normal : shape.normals) normal = {normal.x, normal.z, normal.y};
  for (auto& texcoord : shape.texcoords)
    texcoord = {texcoord.x, 1 - texcoord.y};
  for (auto& quad : shape.quads) quad = {quad.x, quad.w, quad.z, quad.y};
  return shape;
}

// Make a sphere with slipped caps.
shape_data make_capped_uvsphere(
    const vec2i& steps, float scale, const vec2f& uvscale, float cap) {
  auto shape = make_uvsphere(steps, scale, uvscale);
  if (cap != 0) {
    cap        = min(cap, scale / 2);
    auto zflip = (scale - cap);
    for (auto i : range(shape.positions.size())) {
      if (shape.positions[i].z > zflip) {
        shape.positions[i].z = 2 * zflip - shape.positions[i].z;
        shape.normals[i].x   = -shape.normals[i].x;
        shape.normals[i].y   = -shape.normals[i].y;
      } else if (shape.positions[i].z < -zflip) {
        shape.positions[i].z = 2 * (-zflip) - shape.positions[i].z;
        shape.normals[i].x   = -shape.normals[i].x;
        shape.normals[i].y   = -shape.normals[i].y;
      }
    }
  }
  return shape;
}

// Make a sphere with slipped caps.
shape_data make_capped_uvspherey(
    const vec2i& steps, float scale, const vec2f& uvscale, float cap) {
  auto shape = make_capped_uvsphere(steps, scale, uvscale, cap);
  for (auto& position : shape.positions)
    position = {position.x, position.z, position.y};
  for (auto& normal : shape.normals) normal = {normal.x, normal.z, normal.y};
  for (auto& texcoord : shape.texcoords)
    texcoord = {texcoord.x, 1 - texcoord.y};
  for (auto& quad : shape.quads) quad = {quad.x, quad.w, quad.z, quad.y};
  return shape;
}

// Make a disk
shape_data make_disk(int steps, float scale, float uvscale) {
  auto shape = make_rect({steps, steps}, {1, 1}, {uvscale, uvscale});
  for (auto& position : shape.positions) {
    // Analytical Methods for Squaring the Disc, by C. Fong
    // https://arxiv.org/abs/1509.06344
    auto xy = vec2f{position.x, position.y};
    auto uv = vec2f{
        xy.x * sqrt(1 - xy.y * xy.y / 2), xy.y * sqrt(1 - xy.x * xy.x / 2)};
    position = vec3f{uv.x, uv.y, 0} * scale;
  }
  return shape;
}

// Make a bulged disk
shape_data make_bulged_disk(
    int steps, float scale, float uvscale, float height) {
  auto shape = make_disk(steps, scale, uvscale);
  if (height != 0) {
    height      = min(height, scale);
    auto radius = (1 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i : range(shape.positions.size())) {
      auto pn            = normalize(shape.positions[i] - center);
      shape.positions[i] = center + pn * radius;
      shape.normals[i]   = pn;
    }
  }
  return shape;
}

// Make a uv disk
shape_data make_uvdisk(const vec2i& steps, float scale, const vec2f& uvscale) {
  auto shape = make_rect(steps, {1, 1}, {1, 1});
  for (auto i : range(shape.positions.size())) {
    auto uv            = shape.texcoords[i];
    auto phi           = 2 * pif * uv.x;
    shape.positions[i] = vec3f{cos(phi) * uv.y, sin(phi) * uv.y, 0} * scale;
    shape.normals[i]   = {0, 0, 1};
    shape.texcoords[i] = uv * uvscale;
  }
  return shape;
}

// Make a uv cylinder
shape_data make_uvcylinder(
    const vec3i& steps, const vec2f& scale, const vec3f& uvscale) {
  auto shape  = shape_data{};
  auto qshape = shape_data{};
  // side
  qshape = make_rect({steps.x, steps.y}, {1, 1}, {1, 1});
  for (auto i : range(qshape.positions.size())) {
    auto uv             = qshape.texcoords[i];
    auto phi            = 2 * pif * uv.x;
    qshape.positions[i] = {
        cos(phi) * scale.x, sin(phi) * scale.x, (2 * uv.y - 1) * scale.y};
    qshape.normals[i]   = {cos(phi), sin(phi), 0};
    qshape.texcoords[i] = uv * vec2f{uvscale.x, uvscale.y};
  }
  for (auto& quad : qshape.quads) quad = {quad.x, quad.w, quad.z, quad.y};
  merge_shape_inplace(shape, qshape);
  // top
  qshape = make_rect({steps.x, steps.z}, {1, 1}, {1, 1});
  for (auto i : range(qshape.positions.size())) {
    auto uv             = qshape.texcoords[i];
    auto phi            = 2 * pif * uv.x;
    qshape.positions[i] = {
        cos(phi) * uv.y * scale.x, sin(phi) * uv.y * scale.x, 0};
    qshape.normals[i]     = {0, 0, 1};
    qshape.texcoords[i]   = uv * vec2f{uvscale.x, uvscale.z};
    qshape.positions[i].z = scale.y;
  }
  merge_shape_inplace(shape, qshape);
  // bottom
  qshape = make_rect({steps.x, steps.z}, {1, 1}, {1, 1});
  for (auto i : range(qshape.positions.size())) {
    auto uv             = qshape.texcoords[i];
    auto phi            = 2 * pif * uv.x;
    qshape.positions[i] = {
        cos(phi) * uv.y * scale.x, sin(phi) * uv.y * scale.x, 0};
    qshape.normals[i]     = {0, 0, 1};
    qshape.texcoords[i]   = uv * vec2f{uvscale.x, uvscale.z};
    qshape.positions[i].z = -scale.y;
    qshape.normals[i]     = -qshape.normals[i];
  }
  for (auto& qquad : qshape.quads) swap(qquad.x, qquad.z);
  merge_shape_inplace(shape, qshape);
  return shape;
}

// Make a rounded uv cylinder
shape_data make_rounded_uvcylinder(const vec3i& steps, const vec2f& scale,
    const vec3f& uvscale, float radius) {
  auto shape = make_uvcylinder(steps, scale, uvscale);
  if (radius != 0) {
    radius = min(radius, min(scale));
    auto c = scale - radius;
    for (auto i : range(shape.positions.size())) {
      auto phi = atan2(shape.positions[i].y, shape.positions[i].x);
      auto r   = length(vec2f{shape.positions[i].x, shape.positions[i].y});
      auto z   = shape.positions[i].z;
      auto pc  = vec2f{r, abs(z)};
      auto ps  = (z < 0) ? -1.0f : 1.0f;
      if (pc.x >= c.x && pc.y >= c.y) {
        auto pn            = normalize(pc - c);
        shape.positions[i] = {cos(phi) * (c.x + radius * pn.x),
            sin(phi) * (c.x + radius * pn.x), ps * (c.y + radius * pn.y)};
        shape.normals[i]   = {cos(phi) * pn.x, sin(phi) * pn.x, ps * pn.y};
      } else {
        continue;
      }
    }
  }
  return shape;
}

// Generate lines set along a quad. Returns lines, pos, norm, texcoord, radius.
shape_data make_lines(const vec2i& steps, const vec2f& scale,
    const vec2f& uvscale, const vec2f& rad) {
  auto shape = shape_data{};
  shape.positions.resize((steps.x + 1) * steps.y);
  shape.normals.resize((steps.x + 1) * steps.y);
  shape.texcoords.resize((steps.x + 1) * steps.y);
  shape.radius.resize((steps.x + 1) * steps.y);
  if (steps.y > 1) {
    for (auto j : range(steps.y)) {
      for (auto i : range(steps.x + 1)) {
        auto uv = vec2f{i / (float)steps.x, j / (float)(steps.y - 1)};
        shape.positions[j * (steps.x + 1) + i] = {
            (uv.x - 0.5f) * scale.x, (uv.y - 0.5f) * scale.y, 0};
        shape.normals[j * (steps.x + 1) + i]   = {1, 0, 0};
        shape.texcoords[j * (steps.x + 1) + i] = uv * uvscale;
        shape.radius[j * (steps.x + 1) + i]    = lerp(rad.x, rad.y, uv.x);
      }
    }
  } else {
    for (auto i : range(steps.x + 1)) {
      auto uv            = vec2f{i / (float)steps.x, 0};
      shape.positions[i] = {(uv.x - 0.5f) * scale.x, 0, 0};
      shape.normals[i]   = {1, 0, 0};
      shape.texcoords[i] = uv * uvscale;
      shape.radius[i]    = lerp(rad.x, rad.y, uv.x);
    }
  }

  shape.lines.resize(steps.x * steps.y);
  for (int j = 0; j < steps.y; j++) {
    for (int i = 0; i < steps.x; i++) {
      shape.lines[j * steps.x + i] = {
          j * (steps.x + 1) + i, j * (steps.x + 1) + i + 1};
    }
  }

  return shape;
}

// Make point primitives. Returns points, pos, norm, texcoord, radius.
shape_data make_point(float radius) {
  auto shape      = shape_data{};
  shape.points    = {0};
  shape.positions = {{0, 0, 0}};
  shape.normals   = {{0, 0, 1}};
  shape.texcoords = {{0, 0}};
  shape.radius    = {radius};
  return shape;
}

// Generate a point set with points placed at the origin with texcoords
// varying along u.
shape_data make_points(int num, float uvscale, float radius) {
  auto shape = shape_data{};
  shape.points.resize(num);
  for (auto i : range(num)) shape.points[i] = i;
  shape.positions.assign(num, {0, 0, 0});
  shape.normals.assign(num, {0, 0, 1});
  shape.texcoords.assign(num, {0, 0});
  shape.radius.assign(num, radius);
  for (auto i : range(shape.texcoords.size()))
    shape.texcoords[i] = {(float)i / (float)num, 0};
  return shape;
}

shape_data make_points(const vec2i& steps, const vec2f& size,
    const vec2f& uvscale, const vec2f& radius) {
  auto shape  = make_rect(steps, size, uvscale);
  shape.quads = {};
  shape.points.resize(shape.positions.size());
  for (auto i : range(shape.positions.size())) shape.points[i] = (int)i;
  shape.radius.resize(shape.positions.size());
  for (auto i : range(shape.texcoords.size())) {
    shape.radius[i] = lerp(
        radius.x, radius.y, shape.texcoords[i].y / uvscale.y);
  }
  return shape;
}

shape_data make_random_points(
    int num, const vec3f& size, float uvscale, float radius, uint64_t seed) {
  auto shape = make_points(num, uvscale, radius);
  auto rng   = make_rng(seed);
  for (auto& position : shape.positions)
    position = (2 * rand3f(rng) - 1) * size;
  for (auto& texcoord : shape.texcoords) texcoord = rand2f(rng);
  return shape;
}

// Make a facevarying rect
fvshape_data make_fvrect(
    const vec2i& steps, const vec2f& scale, const vec2f& uvscale) {
  auto rect           = make_rect(steps, scale, uvscale);
  auto shape          = fvshape_data{};
  shape.positions     = rect.positions;
  shape.normals       = rect.normals;
  shape.texcoords     = rect.texcoords;
  shape.quadspos      = rect.quads;
  shape.quadsnorm     = rect.quads;
  shape.quadstexcoord = rect.quads;
  return shape;
}

// Make a facevarying box
fvshape_data make_fvbox(
    const vec3i& steps, const vec3f& scale, const vec3f& uvscale) {
  auto shape = fvshape_data{};
  make_fvbox(shape.quadspos, shape.quadsnorm, shape.quadstexcoord,
      shape.positions, shape.normals, shape.texcoords, steps, scale, uvscale);
  return shape;
}

// Make a facevarying sphere
fvshape_data make_fvsphere(int steps, float scale, float uvscale) {
  auto shape = fvshape_data{};
  make_fvsphere(shape.quadspos, shape.quadsnorm, shape.quadstexcoord,
      shape.positions, shape.normals, shape.texcoords, steps, scale, uvscale);
  return shape;
}

// Predefined meshes
shape_data make_monkey(float scale, int subdivisions) {
  extern vector<vec3f> suzanne_positions;
  extern vector<vec4i> suzanne_quads;

  auto shape = shape_data{};
  if (subdivisions == 0) {
    shape.quads     = suzanne_quads;
    shape.positions = suzanne_positions;
  } else {
    std::tie(shape.quads, shape.positions) = subdivide_quads(
        suzanne_quads, suzanne_positions, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}
shape_data make_quad(float scale, int subdivisions) {
  static const auto quad_positions = vector<vec3f>{
      {-1, -1, 0}, {+1, -1, 0}, {+1, +1, 0}, {-1, +1, 0}};
  static const auto quad_normals = vector<vec3f>{
      {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  static const auto quad_texcoords = vector<vec2f>{
      {0, 1}, {1, 1}, {1, 0}, {0, 0}};
  static const auto quad_quads = vector<vec4i>{{0, 1, 2, 3}};
  auto              shape      = shape_data{};
  if (subdivisions == 0) {
    shape.quads     = quad_quads;
    shape.positions = quad_positions;
    shape.normals   = quad_normals;
    shape.texcoords = quad_texcoords;
  } else {
    std::tie(shape.quads, shape.positions) = subdivide_quads(
        quad_quads, quad_positions, subdivisions);
    std::tie(shape.quads, shape.normals) = subdivide_quads(
        quad_quads, quad_normals, subdivisions);
    std::tie(shape.quads, shape.texcoords) = subdivide_quads(
        quad_quads, quad_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}
shape_data make_quady(float scale, int subdivisions) {
  static const auto quady_positions = vector<vec3f>{
      {-1, 0, -1}, {-1, 0, +1}, {+1, 0, +1}, {+1, 0, -1}};
  static const auto quady_normals = vector<vec3f>{
      {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
  static const auto quady_texcoords = vector<vec2f>{
      {0, 0}, {1, 0}, {1, 1}, {0, 1}};
  static const auto quady_quads = vector<vec4i>{{0, 1, 2, 3}};
  auto              shape       = shape_data{};
  if (subdivisions == 0) {
    shape.quads     = quady_quads;
    shape.positions = quady_positions;
    shape.normals   = quady_normals;
    shape.texcoords = quady_texcoords;
  } else {
    std::tie(shape.quads, shape.positions) = subdivide_quads(
        quady_quads, quady_positions, subdivisions);
    std::tie(shape.quads, shape.normals) = subdivide_quads(
        quady_quads, quady_normals, subdivisions);
    std::tie(shape.quads, shape.texcoords) = subdivide_quads(
        quady_quads, quady_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}
shape_data make_cube(float scale, int subdivisions) {
  static const auto cube_positions = vector<vec3f>{{-1, -1, +1}, {+1, -1, +1},
      {+1, +1, +1}, {-1, +1, +1}, {+1, -1, -1}, {-1, -1, -1}, {-1, +1, -1},
      {+1, +1, -1}, {+1, -1, +1}, {+1, -1, -1}, {+1, +1, -1}, {+1, +1, +1},
      {-1, -1, -1}, {-1, -1, +1}, {-1, +1, +1}, {-1, +1, -1}, {-1, +1, +1},
      {+1, +1, +1}, {+1, +1, -1}, {-1, +1, -1}, {+1, -1, +1}, {-1, -1, +1},
      {-1, -1, -1}, {+1, -1, -1}};
  static const auto cube_normals   = vector<vec3f>{{0, 0, +1}, {0, 0, +1},
        {0, 0, +1}, {0, 0, +1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0},
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}};
  static const auto cube_texcoords = vector<vec2f>{{0, 1}, {1, 1}, {1, 0},
      {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0},
      {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1},
      {1, 1}, {1, 0}, {0, 0}};
  static const auto cube_quads     = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
          {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19}, {20, 21, 22, 23}};

  auto shape = shape_data{};
  if (subdivisions == 0) {
    shape.quads     = cube_quads;
    shape.positions = cube_positions;
    shape.normals   = cube_normals;
    shape.texcoords = cube_texcoords;
  } else {
    std::tie(shape.quads, shape.positions) = subdivide_quads(
        cube_quads, cube_positions, subdivisions);
    std::tie(shape.quads, shape.normals) = subdivide_quads(
        cube_quads, cube_normals, subdivisions);
    std::tie(shape.quads, shape.texcoords) = subdivide_quads(
        cube_quads, cube_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}
fvshape_data make_fvcube(float scale, int subdivisions) {
  static const auto fvcube_positions = vector<vec3f>{{-1, -1, +1}, {+1, -1, +1},
      {+1, +1, +1}, {-1, +1, +1}, {+1, -1, -1}, {-1, -1, -1}, {-1, +1, -1},
      {+1, +1, -1}};
  static const auto fvcube_normals   = vector<vec3f>{{0, 0, +1}, {0, 0, +1},
        {0, 0, +1}, {0, 0, +1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0},
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}};
  static const auto fvcube_texcoords = vector<vec2f>{{0, 1}, {1, 1}, {1, 0},
      {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0},
      {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1},
      {1, 1}, {1, 0}, {0, 0}};
  static const auto fvcube_quadspos  = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
       {1, 4, 7, 2}, {5, 0, 3, 6}, {3, 2, 7, 6}, {1, 0, 5, 4}};
  static const auto fvcube_quadsnorm = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
      {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19}, {20, 21, 22, 23}};
  static const auto fvcube_quadstexcoord = vector<vec4i>{{0, 1, 2, 3},
      {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19},
      {20, 21, 22, 23}};

  auto shape = fvshape_data{};
  if (subdivisions == 0) {
    shape.quadspos      = fvcube_quadspos;
    shape.quadsnorm     = fvcube_quadsnorm;
    shape.quadstexcoord = fvcube_quadstexcoord;
    shape.positions     = fvcube_positions;
    shape.normals       = fvcube_normals;
    shape.texcoords     = fvcube_texcoords;
  } else {
    std::tie(shape.quadspos, shape.positions) = subdivide_quads(
        fvcube_quadspos, fvcube_positions, subdivisions);
    std::tie(shape.quadsnorm, shape.normals) = subdivide_quads(
        fvcube_quadsnorm, fvcube_normals, subdivisions);
    std::tie(shape.quadstexcoord, shape.texcoords) = subdivide_quads(
        fvcube_quadstexcoord, fvcube_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}
shape_data make_geosphere(float scale, int subdivisions) {
  // https://stackoverflow.com/questions/17705621/algorithm-for-a-geodesic-sphere
  const float X                   = 0.525731112119133606f;
  const float Z                   = 0.850650808352039932f;
  static auto geosphere_positions = vector<vec3f>{{-X, 0.0, Z}, {X, 0.0, Z},
      {-X, 0.0, -Z}, {X, 0.0, -Z}, {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X},
      {0.0, -Z, -X}, {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}};
  static auto geosphere_triangles = vector<vec3i>{{0, 1, 4}, {0, 4, 9},
      {9, 4, 5}, {4, 8, 5}, {4, 1, 8}, {8, 1, 10}, {8, 10, 3}, {5, 8, 3},
      {5, 3, 2}, {2, 3, 7}, {7, 3, 10}, {7, 10, 6}, {7, 6, 11}, {11, 6, 0},
      {0, 6, 1}, {6, 10, 1}, {9, 11, 0}, {9, 2, 11}, {9, 5, 2}, {7, 11, 2}};

  auto shape = shape_data{};
  if (subdivisions == 0) {
    shape.triangles = geosphere_triangles;
    shape.positions = geosphere_positions;
    shape.normals   = geosphere_positions;
  } else {
    std::tie(shape.triangles, shape.positions) = subdivide_triangles(
        geosphere_triangles, geosphere_positions, subdivisions);
    for (auto& position : shape.positions) position = normalize(position);
    shape.normals = shape.positions;
  }
  if (scale != 1) {
    for (auto& p : shape.positions) p *= scale;
  }
  return shape;
}

// Make a hair ball around a shape
shape_data make_hair(const shape_data& base, const vec2i& steps,
    const vec2f& len, const vec2f& rad, const vec2f& noise, const vec2f& clump,
    const vec2f& rotation, int seed) {
  auto points    = sample_shape(base, steps.y, seed);
  auto bpos      = vector<vec3f>{};
  auto bnorm     = vector<vec3f>{};
  auto btexcoord = vector<vec2f>{};
  for (auto& point : points) {
    bpos.push_back(eval_position(base, point.element, point.uv));
    bnorm.push_back(eval_normal(base, point.element, point.uv));
    btexcoord.push_back(eval_texcoord(base, point.element, point.uv));
  }

  auto rng  = make_rng(seed, 3);
  auto blen = vector<float>(bpos.size());
  for (auto& l : blen) {
    l = lerp(len.x, len.y, rand1f(rng));
  }

  auto cidx = vector<int>();
  if (clump.x > 0) {
    for (auto bidx : range((int)bpos.size())) {
      cidx.push_back(0);
      auto cdist = flt_max;
      for (auto c : range((int)clump.y)) {
        auto d = length(bpos[bidx] - bpos[c]);
        if (d < cdist) {
          cdist       = d;
          cidx.back() = c;
        }
      }
    }
  }

  auto shape = make_lines(steps, {1, 1}, {1, 1}, {1, 1});
  for (auto i : range((int)shape.positions.size())) {
    auto u             = shape.texcoords[i].x;
    auto bidx          = i / (steps.x + 1);
    shape.positions[i] = bpos[bidx] + bnorm[bidx] * u * blen[bidx];
    shape.normals[i]   = bnorm[bidx];
    shape.radius[i]    = lerp(rad.x, rad.y, u);
    if (clump.x > 0) {
      shape.positions[i] =
          shape.positions[i] +
          (shape.positions[i + (cidx[bidx] - bidx) * (steps.x + 1)] -
              shape.positions[i]) *
              u * clump.x;
    }
    if (noise.x > 0) {
      auto nx =
          (perlin_noise(shape.positions[i] * noise.y + vec3f{0, 0, 0}) * 2 -
              1) *
          noise.x;
      auto ny =
          (perlin_noise(shape.positions[i] * noise.y + vec3f{3, 7, 11}) * 2 -
              1) *
          noise.x;
      auto nz =
          (perlin_noise(shape.positions[i] * noise.y + vec3f{13, 17, 19}) * 2 -
              1) *
          noise.x;
      shape.positions[i] += {nx, ny, nz};
    }
  }

  if (clump.x > 0 || noise.x > 0 || rotation.x > 0) {
    shape.normals = lines_tangents(shape.lines, shape.positions);
  }

  return shape;
}

// Grow hairs around a shape
shape_data make_hair2(const shape_data& base, const vec2i& steps,
    const vec2f& len, const vec2f& radius, float noise, float gravity,
    int seed) {
  auto points     = sample_shape(base, steps.y, seed);
  auto bpositions = vector<vec3f>{};
  auto bnormals   = vector<vec3f>{};
  auto btexcoord  = vector<vec2f>{};
  for (auto& point : points) {
    bpositions.push_back(eval_position(base, point.element, point.uv));
    bnormals.push_back(eval_normal(base, point.element, point.uv));
    btexcoord.push_back(eval_texcoord(base, point.element, point.uv));
  }

  auto shape = make_lines(steps, {1, 1}, {1, 1}, radius);
  auto rng   = make_rng(seed);
  for (auto idx : range(steps.y)) {
    auto offset             = idx * (steps.x + 1);
    auto position           = bpositions[idx];
    auto direction          = bnormals[idx];
    auto length             = rand1f(rng) * (len.y - len.x) + len.x;
    shape.positions[offset] = position;
    for (auto iidx = 1; iidx <= steps.x; iidx++) {
      shape.positions[offset + iidx] = position;
      shape.positions[offset + iidx] += direction * length / (float)steps.x;
      shape.positions[offset + iidx] += (2 * rand3f(rng) - 1) * noise;
      shape.positions[offset + iidx] += vec3f{0, -gravity, 0};
      direction = normalize(shape.positions[offset + iidx] - position);
      position  = shape.positions[offset + iidx];
    }
  }

  shape.normals = lines_tangents(shape.lines, shape.positions);

  return shape;
}

// Make a heightfield mesh.
shape_data make_heightfield(const vec2i& size, const vector<float>& height) {
  auto shape = make_recty({size.x - 1, size.y - 1},
      vec2f{(float)size.x, (float)size.y} / (float)max(size), {1, 1});
  for (auto j : range(size.y))
    for (auto i : range(size.x))
      shape.positions[j * size.x + i].y = height[j * size.x + i];
  shape.normals = quads_normals(shape.quads, shape.positions);
  return shape;
}
shape_data make_heightfield(const vec2i& size, const vector<vec4f>& color) {
  auto shape = make_recty({size.x - 1, size.y - 1},
      vec2f{(float)size.x, (float)size.y} / (float)max(size), {1, 1});
  for (auto j : range(size.y))
    for (auto i : range(size.x))
      shape.positions[j * size.x + i].y = mean(xyz(color[j * size.x + i]));
  shape.normals = quads_normals(shape.quads, shape.positions);
  return shape;
}

// Convert points to small spheres and lines to small cylinders. This is
// intended for making very small primitives for display in interactive
// applications, so the spheres are low res.
shape_data points_to_spheres(
    const vector<vec3f>& vertices, int steps, float scale) {
  auto shape = shape_data{};
  for (auto& vertex : vertices) {
    auto sphere = make_sphere(steps, scale, 1);
    for (auto& position : sphere.positions) position += vertex;
    merge_shape_inplace(shape, sphere);
  }
  return shape;
}
shape_data polyline_to_cylinders(
    const vector<vec3f>& vertices, int steps, float scale) {
  auto shape = shape_data{};
  for (auto idx = 0; idx < (int)vertices.size() - 1; idx++) {
    auto cylinder = make_uvcylinder({steps, 1, 1}, {scale, 1}, {1, 1, 1});
    auto frame    = frame_fromz((vertices[idx] + vertices[idx + 1]) / 2,
           vertices[idx] - vertices[idx + 1]);
    auto length   = distance(vertices[idx], vertices[idx + 1]);
    for (auto& position : cylinder.positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : cylinder.normals)
      normal = transform_direction(frame, normal);
    merge_shape_inplace(shape, cylinder);
  }
  return shape;
}
shape_data lines_to_cylinders(
    const vector<vec3f>& vertices, int steps, float scale) {
  auto shape = shape_data{};
  for (auto idx = 0; idx < (int)vertices.size(); idx += 2) {
    auto cylinder = make_uvcylinder({steps, 1, 1}, {scale, 1}, {1, 1, 1});
    auto frame    = frame_fromz((vertices[idx + 0] + vertices[idx + 1]) / 2,
           vertices[idx + 0] - vertices[idx + 1]);
    auto length   = distance(vertices[idx + 0], vertices[idx + 1]);
    for (auto& position : cylinder.positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : cylinder.normals)
      normal = transform_direction(frame, normal);
    merge_shape_inplace(shape, cylinder);
  }
  return shape;
}
shape_data lines_to_cylinders(const vector<vec2i>& lines,
    const vector<vec3f>& positions, int steps, float scale) {
  auto shape = shape_data{};
  for (auto& line : lines) {
    auto cylinder = make_uvcylinder({steps, 1, 1}, {scale, 1}, {1, 1, 1});
    auto frame    = frame_fromz((positions[line.x] + positions[line.y]) / 2,
           positions[line.x] - positions[line.y]);
    auto length   = distance(positions[line.x], positions[line.y]);
    for (auto& position : cylinder.positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : cylinder.normals)
      normal = transform_direction(frame, normal);
    merge_shape_inplace(shape, cylinder);
  }
  return shape;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF COMPUTATION OF PER-VERTEX PROPERTIES
// -----------------------------------------------------------------------------
namespace yocto {

// Compute per-vertex tangents for lines.
vector<vec3f> lines_tangents(
    const vector<vec2i>& lines, const vector<vec3f>& positions) {
  auto tangents = vector<vec3f>{positions.size()};
  for (auto& tangent : tangents) tangent = {0, 0, 0};
  for (auto& l : lines) {
    auto tangent = line_tangent(positions[l.x], positions[l.y]);
    auto length  = line_length(positions[l.x], positions[l.y]);
    tangents[l.x] += tangent * length;
    tangents[l.y] += tangent * length;
  }
  for (auto& tangent : tangents) tangent = normalize(tangent);
  return tangents;
}

// Compute per-vertex normals for triangles.
vector<vec3f> triangles_normals(
    const vector<vec3i>& triangles, const vector<vec3f>& positions) {
  auto normals = vector<vec3f>{positions.size()};
  for (auto& normal : normals) normal = {0, 0, 0};
  for (auto& t : triangles) {
    auto normal = triangle_normal(
        positions[t.x], positions[t.y], positions[t.z]);
    auto area = triangle_area(positions[t.x], positions[t.y], positions[t.z]);
    normals[t.x] += normal * area;
    normals[t.y] += normal * area;
    normals[t.z] += normal * area;
  }
  for (auto& normal : normals) normal = normalize(normal);
  return normals;
}

// Compute per-vertex normals for quads.
vector<vec3f> quads_normals(
    const vector<vec4i>& quads, const vector<vec3f>& positions) {
  auto normals = vector<vec3f>{positions.size()};
  for (auto& normal : normals) normal = {0, 0, 0};
  for (auto& q : quads) {
    auto normal = quad_normal(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    auto area = quad_area(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    normals[q.x] += normal * area;
    normals[q.y] += normal * area;
    normals[q.z] += normal * area;
    if (q.z != q.w) normals[q.w] += normal * area;
  }
  for (auto& normal : normals) normal = normalize(normal);
  return normals;
}

// Compute per-vertex tangents for lines.
void lines_tangents(vector<vec3f>& tangents, const vector<vec2i>& lines,
    const vector<vec3f>& positions) {
  if (tangents.size() != positions.size()) {
    throw std::out_of_range("array should be the same length");
  }
  for (auto& tangent : tangents) tangent = {0, 0, 0};
  for (auto& l : lines) {
    auto tangent = line_tangent(positions[l.x], positions[l.y]);
    auto length  = line_length(positions[l.x], positions[l.y]);
    tangents[l.x] += tangent * length;
    tangents[l.y] += tangent * length;
  }
  for (auto& tangent : tangents) tangent = normalize(tangent);
}

// Compute per-vertex normals for triangles.
void triangles_normals(vector<vec3f>& normals, const vector<vec3i>& triangles,
    const vector<vec3f>& positions) {
  if (normals.size() != positions.size()) {
    throw std::out_of_range("array should be the same length");
  }
  for (auto& normal : normals) normal = {0, 0, 0};
  for (auto& t : triangles) {
    auto normal = triangle_normal(
        positions[t.x], positions[t.y], positions[t.z]);
    auto area = triangle_area(positions[t.x], positions[t.y], positions[t.z]);
    normals[t.x] += normal * area;
    normals[t.y] += normal * area;
    normals[t.z] += normal * area;
  }
  for (auto& normal : normals) normal = normalize(normal);
}

// Compute per-vertex normals for quads.
void quads_normals(vector<vec3f>& normals, const vector<vec4i>& quads,
    const vector<vec3f>& positions) {
  if (normals.size() != positions.size()) {
    throw std::out_of_range("array should be the same length");
  }
  for (auto& normal : normals) normal = {0, 0, 0};
  for (auto& q : quads) {
    auto normal = quad_normal(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    auto area = quad_area(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    normals[q.x] += normal * area;
    normals[q.y] += normal * area;
    normals[q.z] += normal * area;
    if (q.z != q.w) normals[q.w] += normal * area;
  }
  for (auto& normal : normals) normal = normalize(normal);
}

// Compute per-vertex tangent frame for triangle meshes.
// Tangent space is defined by a four component vector.
// The first three components are the tangent with respect to the U texcoord.
// The fourth component is the sign of the tangent wrt the V texcoord.
// Tangent frame is useful in normal mapping.
vector<vec4f> triangles_tangent_spaces(const vector<vec3i>& triangles,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<vec2f>& texcoords) {
  auto tangu = vector<vec3f>(positions.size(), vec3f{0, 0, 0});
  auto tangv = vector<vec3f>(positions.size(), vec3f{0, 0, 0});
  for (auto t : triangles) {
    auto tutv = triangle_tangents_fromuv(positions[t.x], positions[t.y],
        positions[t.z], texcoords[t.x], texcoords[t.y], texcoords[t.z]);
    for (auto vid : {t.x, t.y, t.z}) tangu[vid] += normalize(tutv.first);
    for (auto vid : {t.x, t.y, t.z}) tangv[vid] += normalize(tutv.second);
  }
  for (auto& t : tangu) t = normalize(t);
  for (auto& t : tangv) t = normalize(t);

  auto tangent_spaces = vector<vec4f>(positions.size());
  for (auto& tangent : tangent_spaces) tangent = vec4f{0, 0, 0, 0};
  for (auto i : range(positions.size())) {
    tangu[i] = orthonormalize(tangu[i], normals[i]);
    auto s   = (dot(cross(normals[i], tangu[i]), tangv[i]) < 0) ? -1.0f : 1.0f;
    tangent_spaces[i] = {tangu[i].x, tangu[i].y, tangu[i].z, s};
  }
  return tangent_spaces;
}

// Apply skinning
pair<vector<vec3f>, vector<vec3f>> skin_vertices(const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms) {
  auto skinned_positions = vector<vec3f>{positions.size()};
  auto skinned_normals   = vector<vec3f>{positions.size()};
  for (auto i : range(positions.size())) {
    skinned_positions[i] =
        transform_point(xforms[joints[i].x], positions[i]) * weights[i].x +
        transform_point(xforms[joints[i].y], positions[i]) * weights[i].y +
        transform_point(xforms[joints[i].z], positions[i]) * weights[i].z +
        transform_point(xforms[joints[i].w], positions[i]) * weights[i].w;
  }
  for (auto i : range(normals.size())) {
    skinned_normals[i] = normalize(
        transform_direction(xforms[joints[i].x], normals[i]) * weights[i].x +
        transform_direction(xforms[joints[i].y], normals[i]) * weights[i].y +
        transform_direction(xforms[joints[i].z], normals[i]) * weights[i].z +
        transform_direction(xforms[joints[i].w], normals[i]) * weights[i].w);
  }
  return {skinned_positions, skinned_normals};
}

// Apply skinning as specified in Khronos glTF
pair<vector<vec3f>, vector<vec3f>> skin_matrices(const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms) {
  auto skinned_positions = vector<vec3f>{positions.size()};
  auto skinned_normals   = vector<vec3f>{positions.size()};
  for (auto i : range(positions.size())) {
    auto xform = xforms[joints[i].x] * weights[i].x +
                 xforms[joints[i].y] * weights[i].y +
                 xforms[joints[i].z] * weights[i].z +
                 xforms[joints[i].w] * weights[i].w;
    skinned_positions[i] = transform_point(xform, positions[i]);
    skinned_normals[i]   = normalize(transform_direction(xform, normals[i]));
  }
  return {skinned_positions, skinned_normals};
}

// Apply skinning
void skin_vertices(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms) {
  if (skinned_positions.size() != positions.size() ||
      skinned_normals.size() != normals.size()) {
    throw std::out_of_range("arrays should be the same size");
  }
  for (auto i : range(positions.size())) {
    skinned_positions[i] =
        transform_point(xforms[joints[i].x], positions[i]) * weights[i].x +
        transform_point(xforms[joints[i].y], positions[i]) * weights[i].y +
        transform_point(xforms[joints[i].z], positions[i]) * weights[i].z +
        transform_point(xforms[joints[i].w], positions[i]) * weights[i].w;
  }
  for (auto i : range(normals.size())) {
    skinned_normals[i] = normalize(
        transform_direction(xforms[joints[i].x], normals[i]) * weights[i].x +
        transform_direction(xforms[joints[i].y], normals[i]) * weights[i].y +
        transform_direction(xforms[joints[i].z], normals[i]) * weights[i].z +
        transform_direction(xforms[joints[i].w], normals[i]) * weights[i].w);
  }
}

// Apply skinning as specified in Khronos glTF
void skin_matrices(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms) {
  if (skinned_positions.size() != positions.size() ||
      skinned_normals.size() != normals.size()) {
    throw std::out_of_range("arrays should be the same size");
  }
  for (auto i : range(positions.size())) {
    auto xform = xforms[joints[i].x] * weights[i].x +
                 xforms[joints[i].y] * weights[i].y +
                 xforms[joints[i].z] * weights[i].z +
                 xforms[joints[i].w] * weights[i].w;
    skinned_positions[i] = transform_point(xform, positions[i]);
    skinned_normals[i]   = normalize(transform_direction(xform, normals[i]));
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// COMPUTATION OF PER VERTEX PROPETIES
// -----------------------------------------------------------------------------
namespace yocto {

// Flip vertex normals
vector<vec3f> flip_normals(const vector<vec3f>& normals) {
  auto flipped = normals;
  for (auto& n : flipped) n = -n;
  return flipped;
}
// Flip face orientation
vector<vec3i> flip_triangles(const vector<vec3i>& triangles) {
  auto flipped = triangles;
  for (auto& t : flipped) swap(t.y, t.z);
  return flipped;
}
vector<vec4i> flip_quads(const vector<vec4i>& quads) {
  auto flipped = quads;
  for (auto& q : flipped) {
    if (q.z != q.w) {
      swap(q.y, q.w);
    } else {
      swap(q.y, q.z);
      q.w = q.z;
    }
  }
  return flipped;
}

// Align vertex positions. Alignment is 0: none, 1: min, 2: max, 3: center.
vector<vec3f> align_vertices(
    const vector<vec3f>& positions, const vec3i& alignment) {
  auto bounds = invalidb3f;
  for (auto& p : positions) bounds = merge(bounds, p);
  auto offset = vec3f{0, 0, 0};
  switch (alignment.x) {
    case 0: break;
    case 1: offset.x = bounds.min.x; break;
    case 2: offset.x = (bounds.min.x + bounds.max.x) / 2; break;
    case 3: offset.x = bounds.max.x; break;
    default: throw std::invalid_argument{"invalid alignment"};
  }
  switch (alignment.y) {
    case 0: break;
    case 1: offset.y = bounds.min.y; break;
    case 2: offset.y = (bounds.min.y + bounds.max.y) / 2; break;
    case 3: offset.y = bounds.max.y; break;
    default: throw std::invalid_argument{"invalid alignment"};
  }
  switch (alignment.z) {
    case 0: break;
    case 1: offset.z = bounds.min.z; break;
    case 2: offset.z = (bounds.min.z + bounds.max.z) / 2; break;
    case 3: offset.z = bounds.max.z; break;
    default: throw std::invalid_argument{"invalid alignment"};
  }
  auto aligned = positions;
  for (auto& p : aligned) p -= offset;
  return aligned;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// EDGEA AND ADJACENCIES
// -----------------------------------------------------------------------------
namespace yocto {

// Initialize an edge map with elements.
edge_map make_edge_map(const vector<vec3i>& triangles) {
  auto emap = edge_map{};
  for (auto& t : triangles) {
    insert_edge(emap, {t.x, t.y});
    insert_edge(emap, {t.y, t.z});
    insert_edge(emap, {t.z, t.x});
  }
  return emap;
}
edge_map make_edge_map(const vector<vec4i>& quads) {
  auto emap = edge_map{};
  for (auto& q : quads) {
    insert_edge(emap, {q.x, q.y});
    insert_edge(emap, {q.y, q.z});
    if (q.z != q.w) insert_edge(emap, {q.z, q.w});
    insert_edge(emap, {q.w, q.x});
  }
  return emap;
}
void insert_edges(edge_map& emap, const vector<vec3i>& triangles) {
  for (auto& t : triangles) {
    insert_edge(emap, {t.x, t.y});
    insert_edge(emap, {t.y, t.z});
    insert_edge(emap, {t.z, t.x});
  }
}
void insert_edges(edge_map& emap, const vector<vec4i>& quads) {
  for (auto& q : quads) {
    insert_edge(emap, {q.x, q.y});
    insert_edge(emap, {q.y, q.z});
    if (q.z != q.w) insert_edge(emap, {q.z, q.w});
    insert_edge(emap, {q.w, q.x});
  }
}
// Insert an edge and return its index
int insert_edge(edge_map& emap, const vec2i& edge) {
  auto es = edge.x < edge.y ? edge : vec2i{edge.y, edge.x};
  auto it = emap.edges.find(es);
  if (it == emap.edges.end()) {
    auto data = edge_map::edge_data{(int)emap.edges.size(), 1};
    emap.edges.insert(it, {es, data});
    return data.index;
  } else {
    auto& data = it->second;
    data.nfaces += 1;
    return data.index;
  }
}
// Get number of edges
int num_edges(const edge_map& emap) { return (int)emap.edges.size(); }
// Get the edge index
int edge_index(const edge_map& emap, const vec2i& edge) {
  auto es       = edge.x < edge.y ? edge : vec2i{edge.y, edge.x};
  auto iterator = emap.edges.find(es);
  if (iterator == emap.edges.end()) return -1;
  return iterator->second.index;
}
// Get a list of edges, boundary edges, boundary vertices
vector<vec2i> get_edges(const edge_map& emap) {
  auto edges = vector<vec2i>(emap.edges.size());
  for (auto& [edge, data] : emap.edges) edges[data.index] = edge;
  return edges;
}
vector<vec2i> get_boundary(const edge_map& emap) {
  auto boundary = vector<vec2i>{};
  for (auto& [edge, data] : emap.edges) {
    if (data.nfaces < 2) boundary.push_back(edge);
  }
  return boundary;
}
vector<vec2i> get_edges(const vector<vec3i>& triangles) {
  return get_edges(make_edge_map(triangles));
}
vector<vec2i> get_edges(const vector<vec4i>& quads) {
  return get_edges(make_edge_map(quads));
}
vector<vec2i> get_edges(
    const vector<vec3i>& triangles, const vector<vec4i>& quads) {
  auto edges      = get_edges(triangles);
  auto more_edges = get_edges(quads);
  edges.insert(edges.end(), more_edges.begin(), more_edges.end());
  return edges;
}

// Build adjacencies between faces (sorted counter-clockwise)
vector<vec3i> face_adjacencies(const vector<vec3i>& triangles) {
  auto get_edge = [](const vec3i& triangle, int i) -> vec2i {
    auto x = triangle[i], y = triangle[i < 2 ? i + 1 : 0];
    return x < y ? vec2i{x, y} : vec2i{y, x};
  };
  auto adjacencies = vector<vec3i>{triangles.size(), vec3i{-1, -1, -1}};
  auto edge_map    = unordered_map<vec2i, int>();
  edge_map.reserve((size_t)(triangles.size() * 1.5));
  for (auto i = 0; i < (int)triangles.size(); ++i) {
    for (auto k = 0; k < 3; ++k) {
      auto edge = get_edge(triangles[i], k);
      auto it   = edge_map.find(edge);
      if (it == edge_map.end()) {
        edge_map.insert(it, {edge, i});
      } else {
        auto neighbor     = it->second;
        adjacencies[i][k] = neighbor;
        for (auto kk = 0; kk < 3; ++kk) {
          auto edge2 = get_edge(triangles[neighbor], kk);
          if (edge2 == edge) {
            adjacencies[neighbor][kk] = i;
            break;
          }
        }
      }
    }
  }
  return adjacencies;
}

// Build adjacencies between vertices (sorted counter-clockwise)
vector<vector<int>> vertex_adjacencies(
    const vector<vec3i>& triangles, const vector<vec3i>& adjacencies) {
  auto find_index = [](const vec3i& v, int x) {
    if (v.x == x) return 0;
    if (v.y == x) return 1;
    if (v.z == x) return 2;
    return -1;
  };

  // For each vertex, find any adjacent face.
  auto num_vertices     = 0;
  auto face_from_vertex = vector<int>(triangles.size() * 3, -1);

  for (auto i = 0; i < (int)triangles.size(); ++i) {
    for (auto k : range(3)) {
      face_from_vertex[triangles[i][k]] = i;
      num_vertices                      = max(num_vertices, triangles[i][k]);
    }
  }

  // Init result.
  auto result = vector<vector<int>>(num_vertices);

  // For each vertex, loop around it and build its adjacency.
  for (auto i = 0; i < num_vertices; ++i) {
    result[i].reserve(6);
    auto first_face = face_from_vertex[i];
    if (first_face == -1) continue;

    auto face = first_face;
    while (true) {
      auto k = find_index(triangles[face], i);
      k      = k != 0 ? k - 1 : 2;
      result[i].push_back(triangles[face][k]);
      face = adjacencies[face][k];
      if (face == -1) break;
      if (face == first_face) break;
    }
  }

  return result;
}

// Build adjacencies between each vertex and its adjacent faces.
// Adjacencies are sorted counter-clockwise and have same starting points as
// vertex_adjacencies()
vector<vector<int>> vertex_to_faces_adjacencies(
    const vector<vec3i>& triangles, const vector<vec3i>& adjacencies) {
  auto find_index = [](const vec3i& v, int x) {
    if (v.x == x) return 0;
    if (v.y == x) return 1;
    if (v.z == x) return 2;
    return -1;
  };

  // For each vertex, find any adjacent face.
  auto num_vertices     = 0;
  auto face_from_vertex = vector<int>(triangles.size() * 3, -1);

  for (auto i = 0; i < (int)triangles.size(); ++i) {
    for (auto k : range(3)) {
      face_from_vertex[triangles[i][k]] = i;
      num_vertices                      = max(num_vertices, triangles[i][k]);
    }
  }

  // Init result.
  auto result = vector<vector<int>>(num_vertices);

  // For each vertex, loop around it and build its adjacency.
  for (auto i = 0; i < num_vertices; ++i) {
    result[i].reserve(6);
    auto first_face = face_from_vertex[i];
    if (first_face == -1) continue;

    auto face = first_face;
    while (true) {
      auto k = find_index(triangles[face], i);
      k      = k != 0 ? k - 1 : 2;
      face   = adjacencies[face][k];
      result[i].push_back(face);
      if (face == -1) break;
      if (face == first_face) break;
    }
  }

  return result;
}

// Compute boundaries as a list of loops (sorted counter-clockwise)
vector<vector<int>> ordered_boundaries(const vector<vec3i>& triangles,
    const vector<vec3i>& adjacency, int num_vertices) {
  // map every boundary vertex to its next one
  auto next_vert = vector<int>(num_vertices, -1);
  for (auto i = 0; i < (int)triangles.size(); ++i) {
    for (auto k = 0; k < 3; ++k) {
      if (adjacency[i][k] == -1)
        next_vert[triangles[i][k]] = triangles[i][(k + 1) % 3];
    }
  }

  // result
  auto boundaries = vector<vector<int>>();

  // arrange boundary vertices in loops
  for (auto i : range(next_vert.size())) {
    if (next_vert[i] == -1) continue;

    // add new empty boundary
    boundaries.emplace_back();
    auto current = (int)i;

    while (true) {
      auto next = next_vert[current];
      if (next == -1) {
        return {};
      }
      next_vert[current] = -1;
      boundaries.back().push_back(current);

      // close loop if necessary
      if (next == i)
        break;
      else
        current = next;
    }
  }

  return boundaries;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR BVH
// -----------------------------------------------------------------------------
namespace yocto {

// Splits a BVH node using the middle heuristic. Returns split position and
// axis.
static pair<int, int> split_middle(vector<int>& primitives,
    const vector<bbox3f>& bboxes, const vector<vec3f>& centers, int start,
    int end) {
  // initialize split axis and position
  auto axis = 0;
  auto mid  = (start + end) / 2;

  // compute primintive bounds and size
  auto cbbox = invalidb3f;
  for (auto i = start; i < end; i++)
    cbbox = merge(cbbox, centers[primitives[i]]);
  auto csize = cbbox.max - cbbox.min;
  if (csize == vec3f{0, 0, 0}) return {mid, axis};

  // split along largest
  if (csize.x >= csize.y && csize.x >= csize.z) axis = 0;
  if (csize.y >= csize.x && csize.y >= csize.z) axis = 1;
  if (csize.z >= csize.x && csize.z >= csize.y) axis = 2;

  // split the space in the middle along the largest axis
  auto cmiddle = (cbbox.max + cbbox.min) / 2;
  auto middle  = cmiddle[axis];
  mid = (int)(std::partition(primitives.data() + start, primitives.data() + end,
                  [axis, middle, &centers](
                      auto a) { return centers[a][axis] < middle; }) -
              primitives.data());

  // if we were not able to split, just break the primitives in half
  if (mid == start || mid == end) {
    axis = 0;
    mid  = (start + end) / 2;
    // throw std::runtime_error("bad bvh split");
  }

  return {mid, axis};
}

// Maximum number of primitives per BVH node.
const int bvh_max_prims = 4;

// Build BVH nodes
static bvh_tree make_bvh(vector<bbox3f>& bboxes) {
  // bvh
  auto bvh = bvh_tree{};

  // prepare to build nodes
  bvh.nodes.clear();
  bvh.nodes.reserve(bboxes.size() * 2);

  // prepare primitives
  bvh.primitives.resize(bboxes.size());
  for (auto idx : range(bboxes.size())) bvh.primitives[idx] = (int)idx;

  // prepare centers
  auto centers = vector<vec3f>(bboxes.size());
  for (auto idx : range(bboxes.size())) centers[idx] = center(bboxes[idx]);

  // queue up first node
  auto queue = deque<vec3i>{{0, 0, (int)bboxes.size()}};
  bvh.nodes.emplace_back();

  // create nodes until the queue is empty
  while (!queue.empty()) {
    // grab node to work on
    auto next = queue.front();
    queue.pop_front();
    auto nodeid = next.x, start = next.y, end = next.z;

    // grab node
    auto& node = bvh.nodes[nodeid];

    // compute bounds
    node.bbox = invalidb3f;
    for (auto i = start; i < end; i++)
      node.bbox = merge(node.bbox, bboxes[bvh.primitives[i]]);

    // split into two children
    if (end - start > bvh_max_prims) {
      // get split
      auto [mid, axis] = split_middle(
          bvh.primitives, bboxes, centers, start, end);

      // make an internal node
      node.internal = true;
      node.axis     = (int8_t)axis;
      node.num      = 2;
      node.start    = (int)bvh.nodes.size();
      bvh.nodes.emplace_back();
      bvh.nodes.emplace_back();
      queue.push_back({node.start + 0, start, mid});
      queue.push_back({node.start + 1, mid, end});
    } else {
      // Make a leaf node
      node.internal = false;
      node.num      = (int16_t)(end - start);
      node.start    = start;
    }
  }

  // cleanup
  bvh.nodes.shrink_to_fit();

  // done
  return bvh;
}

// Update bvh
static void update_bvh(bvh_tree& bvh, const vector<bbox3f>& bboxes) {
  for (auto nodeid = (int)bvh.nodes.size() - 1; nodeid >= 0; nodeid--) {
    auto& node = bvh.nodes[nodeid];
    node.bbox  = invalidb3f;
    if (node.internal) {
      for (auto idx : range(2)) {
        node.bbox = merge(node.bbox, bvh.nodes[node.start + idx].bbox);
      }
    } else {
      for (auto idx : range(node.num)) {
        node.bbox = merge(node.bbox, bboxes[bvh.primitives[node.start + idx]]);
      }
    }
  }
}

// Build shape bvh
bvh_tree make_points_bvh(const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(points.size());
  for (auto idx : range(bboxes.size())) {
    auto& p     = points[idx];
    bboxes[idx] = point_bounds(positions[p], radius[p]);
  }

  // build nodes
  return make_bvh(bboxes);
}
bvh_tree make_lines_bvh(const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(lines.size());
  for (auto idx : range(bboxes.size())) {
    auto& l     = lines[idx];
    bboxes[idx] = line_bounds(
        positions[l.x], positions[l.y], radius[l.x], radius[l.y]);
  }

  // build nodes
  return make_bvh(bboxes);
}
bvh_tree make_triangles_bvh(const vector<vec3i>& triangles,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(triangles.size());
  for (auto idx : range(bboxes.size())) {
    auto& t     = triangles[idx];
    bboxes[idx] = triangle_bounds(
        positions[t.x], positions[t.y], positions[t.z]);
  }

  // build nodes
  return make_bvh(bboxes);
}
bvh_tree make_quads_bvh(const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(quads.size());
  for (auto idx : range(bboxes.size())) {
    auto& q     = quads[idx];
    bboxes[idx] = quad_bounds(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
  }

  // build nodes
  return make_bvh(bboxes);
}

void update_points_bvh(bvh_tree& bvh, const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(points.size());
  for (auto idx : range(bboxes.size())) {
    auto& p     = points[idx];
    bboxes[idx] = point_bounds(positions[p], radius[p]);
  }

  // update nodes
  update_bvh(bvh, bboxes);
}
void update_lines_bvh(bvh_tree& bvh, const vector<vec2i>& lines,
    const vector<vec3f>& positions, const vector<float>& radius) {
  // build primitives
  auto bboxes = vector<bbox3f>(lines.size());
  for (auto idx : range(bboxes.size())) {
    auto& l     = lines[idx];
    bboxes[idx] = line_bounds(
        positions[l.x], positions[l.y], radius[l.x], radius[l.y]);
  }

  // update nodes
  update_bvh(bvh, bboxes);
}
void update_triangles_bvh(bvh_tree& bvh, const vector<vec3i>& triangles,
    const vector<vec3f>& positions) {
  // build primitives
  auto bboxes = vector<bbox3f>(triangles.size());
  for (auto idx : range(bboxes.size())) {
    auto& t     = triangles[idx];
    bboxes[idx] = triangle_bounds(
        positions[t.x], positions[t.y], positions[t.z]);
  }

  // update nodes
  update_bvh(bvh, bboxes);
}
void update_quads_bvh(
    bvh_tree& bvh, const vector<vec4i>& quads, const vector<vec3f>& positions) {
  // build primitives
  auto bboxes = vector<bbox3f>(quads.size());
  for (auto idx : range(bboxes.size())) {
    auto& q     = quads[idx];
    bboxes[idx] = quad_bounds(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
  }

  // update nodes
  update_bvh(bvh, bboxes);
}

// Intersect ray with a bvh.
template <typename Intersect>
static shape_intersection intersect_elements_bvh(const bvh_tree& bvh,
    Intersect&& intersect_element, const ray3f& ray_, bool find_any) {
  // check empty
  if (bvh.nodes.empty()) return {};

  // node stack
  auto node_stack        = array<int, 128>{};
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // shared variables
  auto intersection = shape_intersection{};

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
    if (node.internal) {
      // for internal nodes, attempts to proceed along the
      // split axis from smallest to largest nodes
      if (ray_dsign[node.axis]) {
        node_stack[node_cur++] = node.start + 0;
        node_stack[node_cur++] = node.start + 1;
      } else {
        node_stack[node_cur++] = node.start + 1;
        node_stack[node_cur++] = node.start + 0;
      }
    } else {
      for (auto idx : range(node.num)) {
        auto primitive     = bvh.primitives[node.start + idx];
        auto eintersection = intersect_element(primitive, ray);
        if (!eintersection.hit) continue;
        intersection = {
            primitive, eintersection.uv, eintersection.distance, true};
        ray.tmax = eintersection.distance;
      }
    }

    // check for early exit
    if (find_any && intersection.hit) return intersection;
  }

  return intersection;
}

// Intersect ray with a bvh.
shape_intersection intersect_points_bvh(const bvh_tree& bvh,
    const vector<int>& points, const vector<vec3f>& positions,
    const vector<float>& radius, const ray3f& ray, bool find_any) {
  return intersect_elements_bvh(
      bvh,
      [&points, &positions, &radius](int idx, const ray3f& ray) {
        auto& p = points[idx];
        return intersect_point(ray, positions[p], radius[p]);
      },
      ray, find_any);
}
shape_intersection intersect_lines_bvh(const bvh_tree& bvh,
    const vector<vec2i>& lines, const vector<vec3f>& positions,
    const vector<float>& radius, const ray3f& ray, bool find_any) {
  return intersect_elements_bvh(
      bvh,
      [&lines, &positions, &radius](int idx, const ray3f& ray) {
        auto& l = lines[idx];
        return intersect_line(
            ray, positions[l.x], positions[l.y], radius[l.x], radius[l.y]);
      },
      ray, find_any);
}
shape_intersection intersect_triangles_bvh(const bvh_tree& bvh,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const ray3f& ray, bool find_any) {
  return intersect_elements_bvh(
      bvh,
      [&triangles, &positions](int idx, const ray3f& ray) {
        auto& t = triangles[idx];
        return intersect_triangle(
            ray, positions[t.x], positions[t.y], positions[t.z]);
      },
      ray, find_any);
}
shape_intersection intersect_quads_bvh(const bvh_tree& bvh,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const ray3f& ray, bool find_any) {
  return intersect_elements_bvh(
      bvh,
      [&quads, &positions](int idx, const ray3f& ray) {
        auto& t = quads[idx];
        return intersect_quad(ray, positions[t.x], positions[t.y],
            positions[t.z], positions[t.w]);
      },
      ray, find_any);
}

// Intersect ray with a bvh.
template <typename Overlap>
static shape_intersection overlap_elements_bvh(const bvh_tree& bvh,
    Overlap&& overlap_element, const vec3f& pos, float max_distance,
    bool find_any) {
  // check if empty
  if (bvh.nodes.empty()) return {};

  // node stack
  auto node_stack        = array<int, 128>{};
  auto node_cur          = 0;
  node_stack[node_cur++] = 0;

  // hit
  auto intersection = shape_intersection{};

  // walking stack
  while (node_cur) {
    // grab node
    auto& node = bvh.nodes[node_stack[--node_cur]];

    // intersect bbox
    if (!overlap_bbox(pos, max_distance, node.bbox)) continue;

    // intersect node, switching based on node type
    // for each type, iterate over the the primitive list
    if (node.internal) {
      // internal node
      node_stack[node_cur++] = node.start + 0;
      node_stack[node_cur++] = node.start + 1;
    } else {
      for (auto idx : range(node.num)) {
        auto primitive     = bvh.primitives[node.start + idx];
        auto eintersection = overlap_element(primitive, pos, max_distance);
        if (!eintersection.hit) continue;
        intersection = {
            primitive, eintersection.uv, eintersection.distance, true};
        max_distance = eintersection.distance;
      }
    }

    // check for early exit
    if (find_any && intersection.hit) return intersection;
  }

  return intersection;
}

// Find a shape element that overlaps a point within a given distance
// max distance, returning either the closest or any overlap depending on
// `find_any`. Returns the point distance, the instance id, the shape element
// index and the element barycentric coordinates.
shape_intersection overlap_points_bvh(const bvh_tree& bvh,
    const vector<int>& points, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any) {
  return overlap_elements_bvh(
      bvh,
      [&points, &positions, &radius](
          int idx, const vec3f& pos, float max_distance) {
        auto& p = points[idx];
        return overlap_point(pos, max_distance, positions[p], radius[p]);
      },
      pos, max_distance, find_any);
}
shape_intersection overlap_lines_bvh(const bvh_tree& bvh,
    const vector<vec2i>& lines, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any) {
  return overlap_elements_bvh(
      bvh,
      [&lines, &positions, &radius](
          int idx, const vec3f& pos, float max_distance) {
        auto& l = lines[idx];
        return overlap_line(pos, max_distance, positions[l.x], positions[l.y],
            radius[l.x], radius[l.y]);
      },
      pos, max_distance, find_any);
}
shape_intersection overlap_triangles_bvh(const bvh_tree& bvh,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any) {
  return overlap_elements_bvh(
      bvh,
      [&triangles, &positions, &radius](
          int idx, const vec3f& pos, float max_distance) {
        auto& t = triangles[idx];
        return overlap_triangle(pos, max_distance, positions[t.x],
            positions[t.y], positions[t.z], radius[t.x], radius[t.y],
            radius[t.z]);
      },
      pos, max_distance, find_any);
}
shape_intersection overlap_quads_bvh(const bvh_tree& bvh,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<float>& radius, const vec3f& pos, float max_distance,
    bool find_any) {
  return overlap_elements_bvh(
      bvh,
      [&quads, &positions, &radius](
          int idx, const vec3f& pos, float max_distance) {
        auto& q = quads[idx];
        return overlap_quad(pos, max_distance, positions[q.x], positions[q.y],
            positions[q.z], positions[q.w], radius[q.x], radius[q.y],
            radius[q.z], radius[q.w]);
      },
      pos, max_distance, find_any);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// HASH GRID AND NEAREST NEIGHBORS
// -----------------------------------------------------------------------------

namespace yocto {

// Gets the cell index
vec3i get_cell_index(const hash_grid& grid, const vec3f& position) {
  auto scaledpos = position * grid.cell_inv_size;
  return vec3i{(int)scaledpos.x, (int)scaledpos.y, (int)scaledpos.z};
}

// Create a hash_grid
hash_grid make_hash_grid(float cell_size) {
  auto grid          = hash_grid{};
  grid.cell_size     = cell_size;
  grid.cell_inv_size = 1 / cell_size;
  return grid;
}
hash_grid make_hash_grid(const vector<vec3f>& positions, float cell_size) {
  auto grid          = hash_grid{};
  grid.cell_size     = cell_size;
  grid.cell_inv_size = 1 / cell_size;
  for (auto& position : positions) insert_vertex(grid, position);
  return grid;
}
// Inserts a point into the grid
int insert_vertex(hash_grid& grid, const vec3f& position) {
  auto vertex_id = (int)grid.positions.size();
  auto cell      = get_cell_index(grid, position);
  grid.cells[cell].push_back(vertex_id);
  grid.positions.push_back(position);
  return vertex_id;
}
// Finds the nearest neighbors within a given radius
void find_neighbors(const hash_grid& grid, vector<int>& neighbors,
    const vec3f& position, float max_radius, int skip_id) {
  auto cell        = get_cell_index(grid, position);
  auto cell_radius = (int)(max_radius * grid.cell_inv_size) + 1;
  neighbors.clear();
  auto max_radius_squared = max_radius * max_radius;
  for (auto k = -cell_radius; k <= cell_radius; k++) {
    for (auto j = -cell_radius; j <= cell_radius; j++) {
      for (auto i = -cell_radius; i <= cell_radius; i++) {
        auto ncell         = cell + vec3i{i, j, k};
        auto cell_iterator = grid.cells.find(ncell);
        if (cell_iterator == grid.cells.end()) continue;
        auto& ncell_vertices = cell_iterator->second;
        for (auto vertex_id : ncell_vertices) {
          if (distance_squared(grid.positions[vertex_id], position) >
              max_radius_squared)
            continue;
          if (vertex_id == skip_id) continue;
          neighbors.push_back(vertex_id);
        }
      }
    }
  }
}
void find_neighbors(const hash_grid& grid, vector<int>& neighbors,
    const vec3f& position, float max_radius) {
  find_neighbors(grid, neighbors, position, max_radius, -1);
}
void find_neighbors(const hash_grid& grid, vector<int>& neighbors, int vertex,
    float max_radius) {
  find_neighbors(grid, neighbors, grid.positions[vertex], max_radius, vertex);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE ELEMENT CONVERSION AND GROUPING
// -----------------------------------------------------------------------------
namespace yocto {

// Convert quads to triangles
vector<vec3i> quads_to_triangles(const vector<vec4i>& quads) {
  auto triangles = vector<vec3i>{};
  triangles.reserve(quads.size() * 2);
  for (auto& q : quads) {
    triangles.push_back({q.x, q.y, q.w});
    if (q.z != q.w) triangles.push_back({q.z, q.w, q.y});
  }
  return triangles;
}

// Convert triangles to quads by creating degenerate quads
vector<vec4i> triangles_to_quads(const vector<vec3i>& triangles) {
  auto quads = vector<vec4i>{};
  quads.reserve(triangles.size());
  for (auto& t : triangles) quads.push_back({t.x, t.y, t.z, t.z});
  return quads;
}

// Convert beziers to lines using 3 lines for each bezier.
vector<vec2i> bezier_to_lines(const vector<vec4i>& beziers) {
  auto lines = vector<vec2i>{};
  lines.reserve(beziers.size() * 3);
  for (auto b : beziers) {
    lines.push_back({b.x, b.y});
    lines.push_back({b.y, b.z});
    lines.push_back({b.z, b.w});
  }
  return lines;
}

// Convert face varying data to single primitives. Returns the quads indices
// and filled vectors for pos, norm and texcoord.
void split_facevarying(vector<vec4i>& split_quads,
    vector<vec3f>& split_positions, vector<vec3f>& split_normals,
    vector<vec2f>& split_texcoords, const vector<vec4i>& quadspos,
    const vector<vec4i>& quadsnorm, const vector<vec4i>& quadstexcoord,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<vec2f>& texcoords) {
  // make faces unique
  unordered_map<vec3i, int> vert_map;
  split_quads.resize(quadspos.size());
  for (auto fid : range(quadspos.size())) {
    for (auto c : range(4)) {
      auto v = vec3i{
          (&quadspos[fid].x)[c],
          (!quadsnorm.empty()) ? (&quadsnorm[fid].x)[c] : -1,
          (!quadstexcoord.empty()) ? (&quadstexcoord[fid].x)[c] : -1,
      };
      auto it = vert_map.find(v);
      if (it == vert_map.end()) {
        auto s = (int)vert_map.size();
        vert_map.insert(it, {v, s});
        (&split_quads[fid].x)[c] = s;
      } else {
        (&split_quads[fid].x)[c] = it->second;
      }
    }
  }

  // fill vert data
  split_positions.clear();
  if (!positions.empty()) {
    split_positions.resize(vert_map.size());
    for (auto& [vert, index] : vert_map) {
      split_positions[index] = positions[vert.x];
    }
  }
  split_normals.clear();
  if (!normals.empty()) {
    split_normals.resize(vert_map.size());
    for (auto& [vert, index] : vert_map) {
      split_normals[index] = normals[vert.y];
    }
  }
  split_texcoords.clear();
  if (!texcoords.empty()) {
    split_texcoords.resize(vert_map.size());
    for (auto& [vert, index] : vert_map) {
      split_texcoords[index] = texcoords[vert.z];
    }
  }
}

// Weld vertices within a threshold.
pair<vector<vec3f>, vector<int>> weld_vertices(
    const vector<vec3f>& positions, float threshold) {
  auto indices   = vector<int>(positions.size());
  auto welded    = vector<vec3f>{};
  auto grid      = make_hash_grid(threshold);
  auto neighbors = vector<int>{};
  for (auto vertex : range(positions.size())) {
    auto& position = positions[vertex];
    find_neighbors(grid, neighbors, position, threshold);
    if (neighbors.empty()) {
      welded.push_back(position);
      indices[vertex] = (int)welded.size() - 1;
      insert_vertex(grid, position);
    } else {
      indices[vertex] = neighbors.front();
    }
  }
  return {welded, indices};
}
pair<vector<vec3i>, vector<vec3f>> weld_triangles(
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    float threshold) {
  auto [wpositions, indices] = weld_vertices(positions, threshold);
  auto wtriangles            = triangles;
  for (auto& t : wtriangles) t = {indices[t.x], indices[t.y], indices[t.z]};
  return {wtriangles, wpositions};
}
pair<vector<vec4i>, vector<vec3f>> weld_quads(const vector<vec4i>& quads,
    const vector<vec3f>& positions, float threshold) {
  auto [wpositions, indices] = weld_vertices(positions, threshold);
  auto wquads                = quads;
  for (auto& q : wquads)
    q = {
        indices[q.x],
        indices[q.y],
        indices[q.z],
        indices[q.w],
    };
  return {wquads, wpositions};
}

// Merge shape elements
void merge_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& tangents, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec2i>& merge_lines, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_tangents,
    const vector<vec2f>& merge_texturecoords,
    const vector<float>& merge_radius) {
  auto merge_verts = (int)positions.size();
  for (auto& l : merge_lines)
    lines.push_back({l.x + merge_verts, l.y + merge_verts});
  positions.insert(
      positions.end(), merge_positions.begin(), merge_positions.end());
  tangents.insert(tangents.end(), merge_tangents.begin(), merge_tangents.end());
  texcoords.insert(
      texcoords.end(), merge_texturecoords.begin(), merge_texturecoords.end());
  radius.insert(radius.end(), merge_radius.begin(), merge_radius.end());
}
void merge_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3i>& merge_triangles, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords) {
  auto merge_verts = (int)positions.size();
  for (auto& t : merge_triangles)
    triangles.push_back(
        {t.x + merge_verts, t.y + merge_verts, t.z + merge_verts});
  positions.insert(
      positions.end(), merge_positions.begin(), merge_positions.end());
  normals.insert(normals.end(), merge_normals.begin(), merge_normals.end());
  texcoords.insert(
      texcoords.end(), merge_texturecoords.begin(), merge_texturecoords.end());
}
void merge_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec4i>& merge_quads, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords) {
  auto merge_verts = (int)positions.size();
  for (auto& q : merge_quads)
    quads.push_back({q.x + merge_verts, q.y + merge_verts, q.z + merge_verts,
        q.w + merge_verts});
  positions.insert(
      positions.end(), merge_positions.begin(), merge_positions.end());
  normals.insert(normals.end(), merge_normals.begin(), merge_normals.end());
  texcoords.insert(
      texcoords.end(), merge_texturecoords.begin(), merge_texturecoords.end());
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE SUBDIVISION
// -----------------------------------------------------------------------------
namespace yocto {

// Subdivide lines.
template <typename T>
static pair<vector<vec2i>, vector<T>> subdivide_lines_impl(
    const vector<vec2i>& lines, const vector<T>& vertices) {
  // early exit
  if (lines.empty() || vertices.empty()) return {lines, vertices};
  // create vertices
  auto tvertices = vector<T>{};
  tvertices.reserve(vertices.size() + lines.size());
  for (auto& vertex : vertices) tvertices.push_back(vertex);
  for (auto& line : lines) {
    tvertices.push_back((vertices[line.x] + vertices[line.y]) / 2);
  }
  // create lines
  auto tlines = vector<vec2i>{};
  tlines.reserve(lines.size() * 2);
  auto line_vertex = [nverts = (int)vertices.size()](
                         size_t line_id) { return nverts + (int)line_id; };
  for (auto&& [line_id, line] : enumerate(lines)) {
    tlines.push_back({line.x, line_vertex(line_id)});
    tlines.push_back({line_vertex(line_id), line.y});
  }
  // done
  return {tlines, tvertices};
}

// Subdivide triangle.
template <typename T>
static pair<vector<vec3i>, vector<T>> subdivide_triangles_impl(
    const vector<vec3i>& triangles, const vector<T>& vertices) {
  // early exit
  if (triangles.empty() || vertices.empty()) return {triangles, vertices};
  // get edges
  auto emap  = make_edge_map(triangles);
  auto edges = get_edges(emap);
  // create vertices
  auto tvertices = vector<T>{};
  tvertices.reserve(vertices.size() + edges.size());
  for (auto& vertex : vertices) tvertices.push_back(vertex);
  for (auto& edge : edges)
    tvertices.push_back((vertices[edge.x] + vertices[edge.y]) / 2);
  // create triangles
  auto ttriangles = vector<vec3i>{};
  ttriangles.reserve(triangles.size() * 4);
  auto edge_vertex = [&emap, nverts = (int)vertices.size()](const vec2i& edge) {
    return nverts + edge_index(emap, edge);
  };
  for (auto& triangle : triangles) {
    ttriangles.push_back({triangle.x, edge_vertex({triangle.x, triangle.y}),
        edge_vertex({triangle.z, triangle.x})});
    ttriangles.push_back({triangle.y, edge_vertex({triangle.y, triangle.z}),
        edge_vertex({triangle.x, triangle.y})});
    ttriangles.push_back({triangle.z, edge_vertex({triangle.z, triangle.x}),
        edge_vertex({triangle.y, triangle.z})});
    ttriangles.push_back({edge_vertex({triangle.x, triangle.y}),
        edge_vertex({triangle.y, triangle.z}),
        edge_vertex({triangle.z, triangle.x})});
  }
  // done
  return {ttriangles, tvertices};
}

// Subdivide quads.
template <typename T>
static pair<vector<vec4i>, vector<T>> subdivide_quads_impl(
    const vector<vec4i>& quads, const vector<T>& vertices) {
  // early exit
  if (quads.empty() || vertices.empty()) return {quads, vertices};
  // get edges
  auto emap  = make_edge_map(quads);
  auto edges = get_edges(emap);
  // create vertices
  auto tvertices = vector<T>{};
  tvertices.reserve(vertices.size() + edges.size() + quads.size());
  for (auto& vertex : vertices) tvertices.push_back(vertex);
  for (auto& edge : edges)
    tvertices.push_back((vertices[edge.x] + vertices[edge.y]) / 2);
  for (auto& quad : quads) {
    if (quad.z != quad.w) {
      tvertices.push_back((vertices[quad.x] + vertices[quad.y] +
                              vertices[quad.z] + vertices[quad.w]) /
                          4);
    } else {
      tvertices.push_back(
          (vertices[quad.x] + vertices[quad.y] + vertices[quad.z]) / 3);
    }
  }
  // create quads
  auto tquads = vector<vec4i>{};
  tquads.reserve(quads.size() * 4);
  auto edge_vertex = [&emap, nverts = (int)vertices.size()](const vec2i& edge) {
    return nverts + edge_index(emap, edge);
  };
  auto quad_vertex = [nverts    = (int)vertices.size(),
                         nedges = (int)edges.size()](size_t quad_id) {
    return nverts + nedges + (int)quad_id;
  };
  for (auto&& [quad_id, quad] : enumerate(quads)) {
    if (quad.z != quad.w) {
      tquads.push_back({quad.x, edge_vertex({quad.x, quad.y}),
          quad_vertex(quad_id), edge_vertex({quad.w, quad.x})});
      tquads.push_back({quad.y, edge_vertex({quad.y, quad.z}),
          quad_vertex(quad_id), edge_vertex({quad.x, quad.y})});
      tquads.push_back({quad.z, edge_vertex({quad.z, quad.w}),
          quad_vertex(quad_id), edge_vertex({quad.y, quad.z})});
      tquads.push_back({quad.w, edge_vertex({quad.w, quad.x}),
          quad_vertex(quad_id), edge_vertex({quad.z, quad.w})});
    } else {
      tquads.push_back({quad.x, edge_vertex({quad.x, quad.y}),
          quad_vertex(quad_id), edge_vertex({quad.z, quad.x})});
      tquads.push_back({quad.y, edge_vertex({quad.y, quad.z}),
          quad_vertex(quad_id), edge_vertex({quad.x, quad.y})});
      tquads.push_back({quad.z, edge_vertex({quad.z, quad.x}),
          quad_vertex(quad_id), edge_vertex({quad.y, quad.z})});
    }
  }
  // done
  return {tquads, tvertices};
}

// Subdivide beziers.
template <typename T>
static pair<vector<vec4i>, vector<T>> subdivide_beziers_impl(
    const vector<vec4i>& beziers, const vector<T>& vertices) {
  // early exit
  if (beziers.empty() || vertices.empty()) return {beziers, vertices};
  // get edges
  auto vmap      = unordered_map<int, int>();
  auto tvertices = vector<T>();
  auto tbeziers  = vector<vec4i>();
  for (auto& bezier : beziers) {
    if (vmap.find(bezier.x) == vmap.end()) {
      vmap[bezier.x] = (int)tvertices.size();
      tvertices.push_back(vertices[bezier.x]);
    }
    if (vmap.find(bezier.w) == vmap.end()) {
      vmap[bezier.w] = (int)tvertices.size();
      tvertices.push_back(vertices[bezier.w]);
    }
    auto bo = (int)tvertices.size();
    tbeziers.push_back({vmap.at(bezier.x), bo + 0, bo + 1, bo + 2});
    tbeziers.push_back({bo + 2, bo + 3, bo + 4, vmap.at(bezier.w)});
    tvertices.push_back(vertices[bezier.x] / 2 + vertices[bezier.y] / 2);
    tvertices.push_back(vertices[bezier.x] / 4 + vertices[bezier.y] / 2 +
                        vertices[bezier.z] / 4);
    tvertices.push_back(
        vertices[bezier.x] / 8 + vertices[bezier.y] * ((float)3 / (float)8) +
        vertices[bezier.z] * ((float)3 / (float)8) + vertices[bezier.w] / 8);
    tvertices.push_back(vertices[bezier.y] / 4 + vertices[bezier.z] / 2 +
                        vertices[bezier.w] / 4);
    tvertices.push_back(vertices[bezier.z] / 2 + vertices[bezier.w] / 2);
  }

  // done
  return {tbeziers, tvertices};
}

// Subdivide catmullclark.
template <typename T>
static pair<vector<vec4i>, vector<T>> subdivide_catmullclark_impl(
    const vector<vec4i>& quads, const vector<T>& vertices, bool lock_boundary) {
  // early exit
  if (quads.empty() || vertices.empty()) return {quads, vertices};
  // get edges
  auto emap     = make_edge_map(quads);
  auto edges    = get_edges(emap);
  auto boundary = get_boundary(emap);

  // split elements ------------------------------------
  // create vertices
  auto tvertices = vector<T>{};
  tvertices.reserve(vertices.size() + edges.size() + quads.size());
  for (auto& vertex : vertices) tvertices.push_back(vertex);
  for (auto& edge : edges)
    tvertices.push_back((vertices[edge.x] + vertices[edge.y]) / 2);
  for (auto& quad : quads) {
    if (quad.z != quad.w) {
      tvertices.push_back((vertices[quad.x] + vertices[quad.y] +
                              vertices[quad.z] + vertices[quad.w]) /
                          4);
    } else {
      tvertices.push_back(
          (vertices[quad.x] + vertices[quad.y] + vertices[quad.z]) / 3);
    }
  }
  // create quads
  auto tquads = vector<vec4i>{};
  tquads.reserve(quads.size() * 4);
  auto edge_vertex = [&emap, nverts = (int)vertices.size()](const vec2i& edge) {
    return nverts + edge_index(emap, edge);
  };
  auto quad_vertex = [nverts    = (int)vertices.size(),
                         nedges = (int)edges.size()](size_t quad_id) {
    return nverts + nedges + (int)quad_id;
  };
  for (auto&& [quad_id, quad] : enumerate(quads)) {
    if (quad.z != quad.w) {
      tquads.push_back({quad.x, edge_vertex({quad.x, quad.y}),
          quad_vertex(quad_id), edge_vertex({quad.w, quad.x})});
      tquads.push_back({quad.y, edge_vertex({quad.y, quad.z}),
          quad_vertex(quad_id), edge_vertex({quad.x, quad.y})});
      tquads.push_back({quad.z, edge_vertex({quad.z, quad.w}),
          quad_vertex(quad_id), edge_vertex({quad.y, quad.z})});
      tquads.push_back({quad.w, edge_vertex({quad.w, quad.x}),
          quad_vertex(quad_id), edge_vertex({quad.z, quad.w})});
    } else {
      tquads.push_back({quad.x, edge_vertex({quad.x, quad.y}),
          quad_vertex(quad_id), edge_vertex({quad.z, quad.x})});
      tquads.push_back({quad.y, edge_vertex({quad.y, quad.z}),
          quad_vertex(quad_id), edge_vertex({quad.x, quad.y})});
      tquads.push_back({quad.z, edge_vertex({quad.z, quad.x}),
          quad_vertex(quad_id), edge_vertex({quad.y, quad.z})});
    }
  }

  // split boundary
  auto tboundary = vector<vec2i>{};
  tboundary.reserve(boundary.size());
  for (auto& edge : boundary) {
    tboundary.push_back({edge.x, edge_vertex(edge)});
    tboundary.push_back({edge_vertex(edge), edge.y});
  }

  // setup creases -----------------------------------
  auto tcrease_edges = vector<vec2i>{};
  auto tcrease_verts = vector<int>{};
  if (lock_boundary) {
    for (auto& b : tboundary) {
      tcrease_verts.push_back(b.x);
      tcrease_verts.push_back(b.y);
    }
  } else {
    for (auto& b : tboundary) tcrease_edges.push_back(b);
  }

  // define vertices valence ---------------------------
  auto tvert_val = vector<int>(tvertices.size(), 2);
  for (auto& edge : tboundary) {
    tvert_val[edge.x] = (lock_boundary) ? 0 : 1;
    tvert_val[edge.y] = (lock_boundary) ? 0 : 1;
  }

  // averaging pass ----------------------------------
  auto avert  = vector<T>(tvertices.size(), T());
  auto acount = vector<int>(tvertices.size(), 0);
  for (auto& point : tcrease_verts) {
    if (tvert_val[point] != 0) continue;
    avert[point] += tvertices[point];
    acount[point] += 1;
  }
  for (auto& edge : tcrease_edges) {
    auto centroid = (tvertices[edge.x] + tvertices[edge.y]) / 2;
    for (auto vid : {edge.x, edge.y}) {
      if (tvert_val[vid] != 1) continue;
      avert[vid] += centroid;
      acount[vid] += 1;
    }
  }
  for (auto& quad : tquads) {
    auto centroid = (tvertices[quad.x] + tvertices[quad.y] + tvertices[quad.z] +
                        tvertices[quad.w]) /
                    4;
    for (auto vid : {quad.x, quad.y, quad.z, quad.w}) {
      if (tvert_val[vid] != 2) continue;
      avert[vid] += centroid;
      acount[vid] += 1;
    }
  }
  for (auto i : range(tvertices.size())) avert[i] /= (float)acount[i];

  // correction pass ----------------------------------
  // p = p + (avg_p - p) * (4/avg_count)
  for (auto i : range(tvertices.size())) {
    if (tvert_val[i] != 2) continue;
    avert[i] = tvertices[i] +
               (avert[i] - tvertices[i]) * (4 / (float)acount[i]);
  }
  tvertices = avert;

  // done
  return {tquads, tvertices};
}

pair<vector<vec2i>, vector<float>> subdivide_lines(
    const vector<vec2i>& lines, const vector<float>& vertices) {
  return subdivide_lines_impl(lines, vertices);
}
pair<vector<vec2i>, vector<vec2f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec2f>& vertices) {
  return subdivide_lines_impl(lines, vertices);
}
pair<vector<vec2i>, vector<vec3f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec3f>& vertices) {
  return subdivide_lines_impl(lines, vertices);
}
pair<vector<vec2i>, vector<vec4f>> subdivide_lines(
    const vector<vec2i>& lines, const vector<vec4f>& vertices) {
  return subdivide_lines_impl(lines, vertices);
}

pair<vector<vec3i>, vector<float>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<float>& vertices) {
  return subdivide_triangles_impl(triangles, vertices);
}
pair<vector<vec3i>, vector<vec2f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec2f>& vertices) {
  return subdivide_triangles_impl(triangles, vertices);
}
pair<vector<vec3i>, vector<vec3f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec3f>& vertices) {
  return subdivide_triangles_impl(triangles, vertices);
}
pair<vector<vec3i>, vector<vec4f>> subdivide_triangles(
    const vector<vec3i>& triangles, const vector<vec4f>& vertices) {
  return subdivide_triangles_impl(triangles, vertices);
}

pair<vector<vec4i>, vector<float>> subdivide_quads(
    const vector<vec4i>& quads, const vector<float>& vertices) {
  return subdivide_quads_impl(quads, vertices);
}
pair<vector<vec4i>, vector<vec2f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec2f>& vertices) {
  return subdivide_quads_impl(quads, vertices);
}
pair<vector<vec4i>, vector<vec3f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec3f>& vertices) {
  return subdivide_quads_impl(quads, vertices);
}
pair<vector<vec4i>, vector<vec4f>> subdivide_quads(
    const vector<vec4i>& quads, const vector<vec4f>& vertices) {
  return subdivide_quads_impl(quads, vertices);
}

pair<vector<vec4i>, vector<float>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<float>& vertices) {
  return subdivide_beziers_impl(beziers, vertices);
}
pair<vector<vec4i>, vector<vec2f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec2f>& vertices) {
  return subdivide_beziers_impl(beziers, vertices);
}
pair<vector<vec4i>, vector<vec3f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec3f>& vertices) {
  return subdivide_beziers_impl(beziers, vertices);
}
pair<vector<vec4i>, vector<vec4f>> subdivide_beziers(
    const vector<vec4i>& beziers, const vector<vec4f>& vertices) {
  return subdivide_beziers_impl(beziers, vertices);
}

pair<vector<vec4i>, vector<float>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<float>& vertices,
    bool lock_boundary) {
  return subdivide_catmullclark_impl(quads, vertices, lock_boundary);
}
pair<vector<vec4i>, vector<vec2f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec2f>& vertices,
    bool lock_boundary) {
  return subdivide_catmullclark_impl(quads, vertices, lock_boundary);
}
pair<vector<vec4i>, vector<vec3f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec3f>& vertices,
    bool lock_boundary) {
  return subdivide_catmullclark_impl(quads, vertices, lock_boundary);
}
pair<vector<vec4i>, vector<vec4f>> subdivide_catmullclark(
    const vector<vec4i>& quads, const vector<vec4f>& vertices,
    bool lock_boundary) {
  return subdivide_catmullclark_impl(quads, vertices, lock_boundary);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE SAMPLING
// -----------------------------------------------------------------------------
namespace yocto {

// Pick a point in a point set uniformly.
int sample_points(int npoints, float re) { return sample_uniform(npoints, re); }
int sample_points(const vector<float>& cdf, float re) {
  return sample_discrete(cdf, re);
}
vector<float> sample_points_cdf(int npoints) {
  auto cdf = vector<float>(npoints);
  for (auto i : range(cdf.size())) cdf[i] = 1 + (i != 0 ? cdf[i - 1] : 0);
  return cdf;
}
void sample_points_cdf(vector<float>& cdf, int npoints) {
  for (auto i : range(cdf.size())) cdf[i] = 1 + (i != 0 ? cdf[i - 1] : 0);
}

// Pick a point on lines uniformly.
pair<int, float> sample_lines(const vector<float>& cdf, float re, float ru) {
  return {sample_discrete(cdf, re), ru};
}
vector<float> sample_lines_cdf(
    const vector<vec2i>& lines, const vector<vec3f>& positions) {
  auto cdf = vector<float>(lines.size());
  for (auto i : range(cdf.size())) {
    auto& l = lines[i];
    auto  w = line_length(positions[l.x], positions[l.y]);
    cdf[i]  = w + (i != 0 ? cdf[i - 1] : 0);
  }
  return cdf;
}
void sample_lines_cdf(vector<float>& cdf, const vector<vec2i>& lines,
    const vector<vec3f>& positions) {
  for (auto i : range(cdf.size())) {
    auto& l = lines[i];
    auto  w = line_length(positions[l.x], positions[l.y]);
    cdf[i]  = w + (i != 0 ? cdf[i - 1] : 0);
  }
}

// Pick a point on a triangle mesh uniformly.
pair<int, vec2f> sample_triangles(
    const vector<float>& cdf, float re, const vec2f& ruv) {
  return {sample_discrete(cdf, re), sample_triangle(ruv)};
}
vector<float> sample_triangles_cdf(
    const vector<vec3i>& triangles, const vector<vec3f>& positions) {
  auto cdf = vector<float>(triangles.size());
  for (auto i : range(cdf.size())) {
    auto& t = triangles[i];
    auto  w = triangle_area(positions[t.x], positions[t.y], positions[t.z]);
    cdf[i]  = w + (i != 0 ? cdf[i - 1] : 0);
  }
  return cdf;
}
void sample_triangles_cdf(vector<float>& cdf, const vector<vec3i>& triangles,
    const vector<vec3f>& positions) {
  for (auto i : range(cdf.size())) {
    auto& t = triangles[i];
    auto  w = triangle_area(positions[t.x], positions[t.y], positions[t.z]);
    cdf[i]  = w + (i != 0 ? cdf[i - 1] : 0);
  }
}

// Pick a point on a quad mesh uniformly.
pair<int, vec2f> sample_quads(
    const vector<float>& cdf, float re, const vec2f& ruv) {
  return {sample_discrete(cdf, re), ruv};
}
pair<int, vec2f> sample_quads(const vector<vec4i>& quads,
    const vector<float>& cdf, float re, const vec2f& ruv) {
  auto element = sample_discrete(cdf, re);
  if (quads[element].z == quads[element].w) {
    return {element, sample_triangle(ruv)};
  } else {
    return {element, ruv};
  }
}
vector<float> sample_quads_cdf(
    const vector<vec4i>& quads, const vector<vec3f>& positions) {
  auto cdf = vector<float>(quads.size());
  for (auto i : range(cdf.size())) {
    auto& q = quads[i];
    auto  w = quad_area(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    cdf[i] = w + (i ? cdf[i - 1] : 0);
  }
  return cdf;
}
void sample_quads_cdf(vector<float>& cdf, const vector<vec4i>& quads,
    const vector<vec3f>& positions) {
  for (auto i : range(cdf.size())) {
    auto& q = quads[i];
    auto  w = quad_area(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    cdf[i] = w + (i ? cdf[i - 1] : 0);
  }
}

// Samples a set of points over a triangle mesh uniformly. The rng function
// takes the point index and returns vec3f numbers uniform directibuted in
// [0,1]^3. unorm and texcoord are optional.
void sample_triangles(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texcoords,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texcoords, int npoints,
    int seed) {
  sampled_positions.resize(npoints);
  sampled_normals.resize(npoints);
  sampled_texcoords.resize(npoints);
  auto cdf = sample_triangles_cdf(triangles, positions);
  auto rng = make_rng(seed);
  for (auto i : range(npoints)) {
    auto  sample         = sample_triangles(cdf, rand1f(rng), rand2f(rng));
    auto& t              = triangles[sample.first];
    auto  uv             = sample.second;
    sampled_positions[i] = interpolate_triangle(
        positions[t.x], positions[t.y], positions[t.z], uv);
    if (!sampled_normals.empty()) {
      sampled_normals[i] = normalize(
          interpolate_triangle(normals[t.x], normals[t.y], normals[t.z], uv));
    } else {
      sampled_normals[i] = triangle_normal(
          positions[t.x], positions[t.y], positions[t.z]);
    }
    if (!sampled_texcoords.empty()) {
      sampled_texcoords[i] = interpolate_triangle(
          texcoords[t.x], texcoords[t.y], texcoords[t.z], uv);
    } else {
      sampled_texcoords[i] = vec2f{0, 0};
    }
  }
}

// Samples a set of points over a triangle mesh uniformly. The rng function
// takes the point index and returns vec3f numbers uniform directibuted in
// [0,1]^3. unorm and texcoord are optional.
void sample_quads(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texcoords,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texcoords, int npoints,
    int seed) {
  sampled_positions.resize(npoints);
  sampled_normals.resize(npoints);
  sampled_texcoords.resize(npoints);
  auto cdf = sample_quads_cdf(quads, positions);
  auto rng = make_rng(seed);
  for (auto i : range(npoints)) {
    auto  sample         = sample_quads(cdf, rand1f(rng), rand2f(rng));
    auto& q              = quads[sample.first];
    auto  uv             = sample.second;
    sampled_positions[i] = interpolate_quad(
        positions[q.x], positions[q.y], positions[q.z], positions[q.w], uv);
    if (!sampled_normals.empty()) {
      sampled_normals[i] = normalize(interpolate_quad(
          normals[q.x], normals[q.y], normals[q.z], normals[q.w], uv));
    } else {
      sampled_normals[i] = quad_normal(
          positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
    }
    if (!sampled_texcoords.empty()) {
      sampled_texcoords[i] = interpolate_quad(
          texcoords[q.x], texcoords[q.y], texcoords[q.z], texcoords[q.w], uv);
    } else {
      sampled_texcoords[i] = vec2f{0, 0};
    }
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// EXAMPLE SHAPES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a quad.
void make_rect(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale) {
  positions.resize((steps.x + 1) * (steps.y + 1));
  normals.resize((steps.x + 1) * (steps.y + 1));
  texcoords.resize((steps.x + 1) * (steps.y + 1));
  for (auto j : range(steps.y + 1)) {
    for (auto i : range(steps.x + 1)) {
      auto uv = vec2f{i / (float)steps.x, j / (float)steps.y};
      positions[j * (steps.x + 1) + i] = {
          (2 * uv.x - 1) * scale.x, (2 * uv.y - 1) * scale.y, 0};
      normals[j * (steps.x + 1) + i]   = {0, 0, 1};
      texcoords[j * (steps.x + 1) + i] = vec2f{uv.x, 1 - uv.y} * uvscale;
    }
  }

  quads.resize(steps.x * steps.y);
  for (auto j : range(steps.y)) {
    for (auto i : range(steps.x)) {
      quads[j * steps.x + i] = {j * (steps.x + 1) + i,
          j * (steps.x + 1) + i + 1, (j + 1) * (steps.x + 1) + i + 1,
          (j + 1) * (steps.x + 1) + i};
    }
  }
}

void make_bulged_rect(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float height) {
  make_rect(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (height != 0) {
    height      = min(height, min(scale));
    auto radius = (1 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i : range(positions.size())) {
      auto pn      = normalize(positions[i] - center);
      positions[i] = center + pn * radius;
      normals[i]   = pn;
    }
  }
}

// Make a quad.
void make_recty(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale) {
  make_rect(quads, positions, normals, texcoords, steps, scale, uvscale);
  for (auto& position : positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
}

void make_bulged_recty(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float height) {
  make_bulged_rect(
      quads, positions, normals, texcoords, steps, scale, uvscale, height);
  for (auto& position : positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
}

// Make a cube.
void make_box(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec3f& uvscale) {
  quads.clear();
  positions.clear();
  normals.clear();
  texcoords.clear();
  auto qquads         = vector<vec4i>{};
  auto qpositions     = vector<vec3f>{};
  auto qnormals       = vector<vec3f>{};
  auto qtexturecoords = vector<vec2f>{};
  // + z
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.x, steps.y},
      {scale.x, scale.y}, {uvscale.x, uvscale.y});
  for (auto& p : qpositions) p = {p.x, p.y, scale.z};
  for (auto& n : qnormals) n = {0, 0, 1};
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
  // - z
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.x, steps.y},
      {scale.x, scale.y}, {uvscale.x, uvscale.y});
  for (auto& p : qpositions) p = {-p.x, p.y, -scale.z};
  for (auto& n : qnormals) n = {0, 0, -1};
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
  // + x
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.z, steps.y},
      {scale.z, scale.y}, {uvscale.z, uvscale.y});
  for (auto& p : qpositions) p = {scale.x, p.y, -p.x};
  for (auto& n : qnormals) n = {1, 0, 0};
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
  // - x
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.z, steps.y},
      {scale.z, scale.y}, {uvscale.z, uvscale.y});
  for (auto& p : qpositions) p = {-scale.x, p.y, p.x};
  for (auto& n : qnormals) n = {-1, 0, 0};
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
  // + y
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.x, steps.z},
      {scale.x, scale.z}, {uvscale.x, uvscale.z});
  for (auto i : range(qpositions.size())) {
    qpositions[i] = {qpositions[i].x, scale.y, -qpositions[i].y};
    qnormals[i]   = {0, 1, 0};
  }
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
  // - y
  make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.x, steps.z},
      {scale.x, scale.z}, {uvscale.x, uvscale.z});
  for (auto i : range(qpositions.size())) {
    qpositions[i] = {qpositions[i].x, -scale.y, qpositions[i].y};
    qnormals[i]   = {0, -1, 0};
  }
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexturecoords);
}

void make_rounded_box(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec3f& uvscale, float radius) {
  make_box(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (radius != 0) {
    radius = min(radius, min(scale));
    auto c = scale - radius;
    for (auto i : range(positions.size())) {
      auto pc = vec3f{
          abs(positions[i].x), abs(positions[i].y), abs(positions[i].z)};
      auto ps = vec3f{positions[i].x < 0 ? -1.0f : 1.0f,
          positions[i].y < 0 ? -1.0f : 1.0f, positions[i].z < 0 ? -1.0f : 1.0f};
      if (pc.x >= c.x && pc.y >= c.y && pc.z >= c.z) {
        auto pn      = normalize(pc - c);
        positions[i] = c + radius * pn;
        normals[i]   = pn;
      } else if (pc.x >= c.x && pc.y >= c.y) {
        auto pn      = normalize((pc - c) * vec3f{1, 1, 0});
        positions[i] = {c.x + radius * pn.x, c.y + radius * pn.y, pc.z};
        normals[i]   = pn;
      } else if (pc.x >= c.x && pc.z >= c.z) {
        auto pn      = normalize((pc - c) * vec3f{1, 0, 1});
        positions[i] = {c.x + radius * pn.x, pc.y, c.z + radius * pn.z};
        normals[i]   = pn;
      } else if (pc.y >= c.y && pc.z >= c.z) {
        auto pn      = normalize((pc - c) * vec3f{0, 1, 1});
        positions[i] = {pc.x, c.y + radius * pn.y, c.z + radius * pn.z};
        normals[i]   = pn;
      } else {
        continue;
      }
      positions[i] *= ps;
      normals[i] *= ps;
    }
  }
}

void make_rect_stack(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& scale, const vec2f& uvscale) {
  auto qquads         = vector<vec4i>{};
  auto qpositions     = vector<vec3f>{};
  auto qnormals       = vector<vec3f>{};
  auto qtexturecoords = vector<vec2f>{};
  for (auto i : range(steps.z + 1)) {
    make_rect(qquads, qpositions, qnormals, qtexturecoords, {steps.x, steps.y},
        {scale.x, scale.y}, uvscale);
    for (auto& p : qpositions) p.z = (-1 + 2 * (float)i / steps.z) * scale.z;
    merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
        qnormals, qtexturecoords);
  }
}

void make_floor(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale) {
  make_rect(quads, positions, normals, texcoords, steps, scale, uvscale);
  for (auto& position : positions)
    position = {position.x, position.z, -position.y};
  for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
}

void make_bent_floor(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& scale, const vec2f& uvscale, float radius) {
  make_floor(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (radius != 0) {
    radius     = min(radius, scale.y);
    auto start = (scale.y - radius) / 2;
    auto end   = start + radius;
    for (auto i : range(positions.size())) {
      if (positions[i].z < -end) {
        positions[i] = {positions[i].x, -positions[i].z - end + radius, -end};
        normals[i]   = {0, 0, 1};
      } else if (positions[i].z < -start && positions[i].z >= -end) {
        auto phi     = (pif / 2) * (-positions[i].z - start) / radius;
        positions[i] = {positions[i].x, -cos(phi) * radius + radius,
            -sin(phi) * radius - start};
        normals[i]   = {0, cos(phi), sin(phi)};
      } else {
      }
    }
  }
}

// Generate a sphere
void make_sphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale) {
  make_box(quads, positions, normals, texcoords, {steps, steps, steps},
      {scale, scale, scale}, {uvscale, uvscale, uvscale});
  for (auto& p : positions) p = normalize(p) * scale;
  normals = positions;
  for (auto& n : normals) n = normalize(n);
}

// Generate a uvsphere
void make_uvsphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale) {
  make_rect(quads, positions, normals, texcoords, steps, {1, 1}, {1, 1});
  for (auto i : range(positions.size())) {
    auto uv      = texcoords[i];
    auto a       = vec2f{2 * pif * uv.x, pif * (1 - uv.y)};
    positions[i] = vec3f{cos(a.x) * sin(a.y), sin(a.x) * sin(a.y), cos(a.y)} *
                   scale;
    normals[i]   = normalize(positions[i]);
    texcoords[i] = uv * uvscale;
  }
}

void make_capped_uvsphere(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale, float cap) {
  make_uvsphere(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (cap != 0) {
    cap        = min(cap, scale / 2);
    auto zflip = (scale - cap);
    for (auto i : range(positions.size())) {
      if (positions[i].z > zflip) {
        positions[i].z = 2 * zflip - positions[i].z;
        normals[i].x   = -normals[i].x;
        normals[i].y   = -normals[i].y;
      } else if (positions[i].z < -zflip) {
        positions[i].z = 2 * (-zflip) - positions[i].z;
        normals[i].x   = -normals[i].x;
        normals[i].y   = -normals[i].y;
      }
    }
  }
}

// Generate a uvsphere
void make_uvspherey(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale) {
  make_uvsphere(quads, positions, normals, texcoords, steps, scale, uvscale);
  for (auto& position : positions)
    position = {position.x, position.z, position.y};
  for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
  for (auto& texcoord : texcoords) texcoord = {texcoord.x, 1 - texcoord.y};
  for (auto& quad : quads) quad = {quad.x, quad.w, quad.z, quad.y};
}

void make_capped_uvspherey(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale, float cap) {
  make_capped_uvsphere(
      quads, positions, normals, texcoords, steps, scale, uvscale, cap);
  for (auto& position : positions)
    position = {position.x, position.z, position.y};
  for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
  for (auto& texcoord : texcoords) texcoord = {texcoord.x, 1 - texcoord.y};
  for (auto& quad : quads) quad = {quad.x, quad.w, quad.z, quad.y};
}

// Generate a disk
void make_disk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale) {
  make_rect(quads, positions, normals, texcoords, {steps, steps}, {1, 1},
      {uvscale, uvscale});
  for (auto& position : positions) {
    // Analytical Methods for Squaring the Disc, by C. Fong
    // https://arxiv.org/abs/1509.06344
    auto xy = vec2f{position.x, position.y};
    auto uv = vec2f{
        xy.x * sqrt(1 - xy.y * xy.y / 2), xy.y * sqrt(1 - xy.x * xy.x / 2)};
    position = vec3f{uv.x, uv.y, 0} * scale;
  }
}

void make_bulged_disk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float scale,
    float uvscale, float height) {
  make_disk(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (height != 0) {
    height      = min(height, scale);
    auto radius = (1 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i : range(positions.size())) {
      auto pn      = normalize(positions[i] - center);
      positions[i] = center + pn * radius;
      normals[i]   = pn;
    }
  }
}

// Generate a uvdisk
void make_uvdisk(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    float scale, const vec2f& uvscale) {
  make_rect(quads, positions, normals, texcoords, steps, {1, 1}, {1, 1});
  for (auto i : range(positions.size())) {
    auto uv      = texcoords[i];
    auto phi     = 2 * pif * uv.x;
    positions[i] = vec3f{cos(phi) * uv.y, sin(phi) * uv.y, 0} * scale;
    normals[i]   = {0, 0, 1};
    texcoords[i] = uv * uvscale;
  }
}

// Generate a uvcylinder
void make_uvcylinder(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec2f& scale, const vec3f& uvscale) {
  auto qquads     = vector<vec4i>{};
  auto qpositions = vector<vec3f>{};
  auto qnormals   = vector<vec3f>{};
  auto qtexcoords = vector<vec2f>{};
  // side
  make_rect(qquads, qpositions, qnormals, qtexcoords, {steps.x, steps.y},
      {1, 1}, {1, 1});
  for (auto i : range(qpositions.size())) {
    auto uv       = qtexcoords[i];
    auto phi      = 2 * pif * uv.x;
    qpositions[i] = {
        cos(phi) * scale.x, sin(phi) * scale.x, (2 * uv.y - 1) * scale.y};
    qnormals[i]   = {cos(phi), sin(phi), 0};
    qtexcoords[i] = uv * vec2f{uvscale.x, uvscale.y};
  }
  for (auto& quad : qquads) quad = {quad.x, quad.w, quad.z, quad.y};
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexcoords);
  // top
  make_rect(qquads, qpositions, qnormals, qtexcoords, {steps.x, steps.z},
      {1, 1}, {1, 1});
  for (auto i : range(qpositions.size())) {
    auto uv         = qtexcoords[i];
    auto phi        = 2 * pif * uv.x;
    qpositions[i]   = {cos(phi) * uv.y * scale.x, sin(phi) * uv.y * scale.x, 0};
    qnormals[i]     = {0, 0, 1};
    qtexcoords[i]   = uv * vec2f{uvscale.x, uvscale.z};
    qpositions[i].z = scale.y;
  }
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexcoords);
  // bottom
  make_rect(qquads, qpositions, qnormals, qtexcoords, {steps.x, steps.z},
      {1, 1}, {1, 1});
  for (auto i : range(qpositions.size())) {
    auto uv         = qtexcoords[i];
    auto phi        = 2 * pif * uv.x;
    qpositions[i]   = {cos(phi) * uv.y * scale.x, sin(phi) * uv.y * scale.x, 0};
    qnormals[i]     = {0, 0, 1};
    qtexcoords[i]   = uv * vec2f{uvscale.x, uvscale.z};
    qpositions[i].z = -scale.y;
    qnormals[i]     = -qnormals[i];
  }
  for (auto& qquad : qquads) swap(qquad.x, qquad.z);
  merge_quads(quads, positions, normals, texcoords, qquads, qpositions,
      qnormals, qtexcoords);
}

// Generate a uvcylinder
void make_rounded_uvcylinder(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec2f& scale, const vec3f& uvscale, float radius) {
  make_uvcylinder(quads, positions, normals, texcoords, steps, scale, uvscale);
  if (radius != 0) {
    radius = min(radius, min(scale));
    auto c = scale - radius;
    for (auto i : range(positions.size())) {
      auto phi = atan2(positions[i].y, positions[i].x);
      auto r   = length(vec2f{positions[i].x, positions[i].y});
      auto z   = positions[i].z;
      auto pc  = vec2f{r, abs(z)};
      auto ps  = (z < 0) ? -1.0f : 1.0f;
      if (pc.x >= c.x && pc.y >= c.y) {
        auto pn      = normalize(pc - c);
        positions[i] = {cos(phi) * (c.x + radius * pn.x),
            sin(phi) * (c.x + radius * pn.x), ps * (c.y + radius * pn.y)};
        normals[i]   = {cos(phi) * pn.x, sin(phi) * pn.x, ps * pn.y};
      } else {
        continue;
      }
    }
  }
}

// Generate lines set along a quad.
void make_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvscale,
    const vec2f& rad) {
  positions.resize((steps.x + 1) * steps.y);
  normals.resize((steps.x + 1) * steps.y);
  texcoords.resize((steps.x + 1) * steps.y);
  radius.resize((steps.x + 1) * steps.y);
  if (steps.y > 1) {
    for (auto j : range(steps.y)) {
      for (auto i : range(steps.x + 1)) {
        auto uv = vec2f{i / (float)steps.x, j / (float)(steps.y - 1)};
        positions[j * (steps.x + 1) + i] = {
            (uv.x - 0.5f) * size.x, (uv.y - 0.5f) * size.y, 0};
        normals[j * (steps.x + 1) + i]   = {1, 0, 0};
        texcoords[j * (steps.x + 1) + i] = uv * uvscale;
        radius[j * (steps.x + 1) + i]    = lerp(rad.x, rad.y, uv.x);
      }
    }
  } else {
    for (auto i : range(steps.x + 1)) {
      auto uv      = vec2f{i / (float)steps.x, 0};
      positions[i] = {(uv.x - 0.5f) * size.x, 0, 0};
      normals[i]   = {1, 0, 0};
      texcoords[i] = uv * uvscale;
      radius[i]    = lerp(rad.x, rad.y, uv.x);
    }
  }

  lines.resize(steps.x * steps.y);
  for (int j = 0; j < steps.y; j++) {
    for (int i = 0; i < steps.x; i++) {
      lines[j * steps.x + i] = {
          j * (steps.x + 1) + i, j * (steps.x + 1) + i + 1};
    }
  }
}

// Generate a point at the origin.
void make_point(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    float point_radius) {
  points    = {0};
  positions = {{0, 0, 0}};
  normals   = {{0, 0, 1}};
  texcoords = {{0, 0}};
  radius    = {point_radius};
}

// Generate a point set with points placed at the origin with texcoords
// varying along u.
void make_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    int num, float uvscale, float point_radius) {
  points.resize(num);
  for (auto i : range(num)) points[i] = i;
  positions.assign(num, {0, 0, 0});
  normals.assign(num, {0, 0, 1});
  texcoords.assign(num, {0, 0});
  radius.assign(num, point_radius);
  for (auto i : range(texcoords.size()))
    texcoords[i] = {(float)i / (float)num, 0};
}

// Generate a point set along a quad.
void make_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvscale,
    const vec2f& rad) {
  auto quads = vector<vec4i>{};
  make_rect(quads, positions, normals, texcoords, steps, size, uvscale);
  points.resize(positions.size());
  for (auto i : range(positions.size())) points[i] = (int)i;
  radius.resize(positions.size());
  for (auto i : range(texcoords.size()))
    radius[i] = lerp(rad.x, rad.y, texcoords[i].y / uvscale.y);
}

// Generate a point set.
void make_random_points(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    int num, const vec3f& size, float uvscale, float point_radius,
    uint64_t seed) {
  make_points(points, positions, normals, texcoords, radius, num, uvscale,
      point_radius);
  auto rng = make_rng(seed);
  for (auto& position : positions) position = (2 * rand3f(rng) - 1) * size;
  for (auto& texcoord : texcoords) texcoord = rand2f(rng);
}

// Make a bezier circle. Returns bezier, pos.
void make_bezier_circle(
    vector<vec4i>& beziers, vector<vec3f>& positions, float size) {
  // constant from http://spencermortensen.com/articles/bezier-circle/
  const auto  c              = 0.551915024494f;
  static auto circle_pos     = vector<vec3f>{{1, 0, 0}, {1, c, 0}, {c, 1, 0},
          {0, 1, 0}, {-c, 1, 0}, {-1, c, 0}, {-1, 0, 0}, {-1, -c, 0}, {-c, -1, 0},
          {0, -1, 0}, {c, -1, 0}, {1, -c, 0}};
  static auto circle_beziers = vector<vec4i>{
      {0, 1, 2, 3}, {3, 4, 5, 6}, {6, 7, 8, 9}, {9, 10, 11, 0}};
  positions = circle_pos;
  beziers   = circle_beziers;
  for (auto& p : positions) p *= size;
}

// Make fvquad
void make_fvrect(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvscale) {
  make_rect(quadspos, positions, normals, texcoords, steps, size, uvscale);
  quadsnorm     = quadspos;
  quadstexcoord = quadspos;
}
void make_fvbox(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvscale) {
  make_box(quadspos, positions, normals, texcoords, steps, size, uvscale);
  quadsnorm                     = quadspos;
  quadstexcoord                 = quadspos;
  std::tie(quadspos, positions) = weld_quads(
      quadspos, positions, 0.1f * min(size) / max(steps));
}
void make_fvsphere(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, int steps, float size,
    float uvscale) {
  make_fvbox(quadspos, quadsnorm, quadstexcoord, positions, normals, texcoords,
      {steps, steps, steps}, {size, size, size}, {uvscale, uvscale, uvscale});
  quadsnorm = quadspos;
  normals   = positions;
  for (auto& n : normals) n = normalize(n);
}

// Predefined meshes
void make_monkey(vector<vec4i>& quads, vector<vec3f>& positions, float scale,
    int subdivisions) {
  extern vector<vec3f> suzanne_positions;
  extern vector<vec4i> suzanne_quads;

  if (subdivisions == 0) {
    quads     = suzanne_quads;
    positions = suzanne_positions;
  } else {
    std::tie(quads, positions) = subdivide_quads(
        suzanne_quads, suzanne_positions, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

void make_quad(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions) {
  static const auto quad_positions = vector<vec3f>{
      {-1, -1, 0}, {+1, -1, 0}, {+1, +1, 0}, {-1, +1, 0}};
  static const auto quad_normals = vector<vec3f>{
      {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  static const auto quad_texcoords = vector<vec2f>{
      {0, 1}, {1, 1}, {1, 0}, {0, 0}};
  static const auto quad_quads = vector<vec4i>{{0, 1, 2, 3}};
  if (subdivisions == 0) {
    quads     = quad_quads;
    positions = quad_positions;
    normals   = quad_normals;
    texcoords = quad_texcoords;
  } else {
    std::tie(quads, positions) = subdivide_quads(
        quad_quads, quad_positions, subdivisions);
    std::tie(quads, normals) = subdivide_quads(
        quad_quads, quad_normals, subdivisions);
    std::tie(quads, texcoords) = subdivide_quads(
        quad_quads, quad_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

void make_quady(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions) {
  static const auto quady_positions = vector<vec3f>{
      {-1, 0, -1}, {-1, 0, +1}, {+1, 0, +1}, {+1, 0, -1}};
  static const auto quady_normals = vector<vec3f>{
      {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
  static const auto quady_texcoords = vector<vec2f>{
      {0, 0}, {1, 0}, {1, 1}, {0, 1}};
  static const auto quady_quads = vector<vec4i>{{0, 1, 2, 3}};
  if (subdivisions == 0) {
    quads     = quady_quads;
    positions = quady_positions;
    normals   = quady_normals;
    texcoords = quady_texcoords;
  } else {
    std::tie(quads, positions) = subdivide_quads(
        quady_quads, quady_positions, subdivisions);
    std::tie(quads, normals) = subdivide_quads(
        quady_quads, quady_normals, subdivisions);
    std::tie(quads, texcoords) = subdivide_quads(
        quady_quads, quady_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

void make_cube(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions) {
  static const auto cube_positions = vector<vec3f>{{-1, -1, +1}, {+1, -1, +1},
      {+1, +1, +1}, {-1, +1, +1}, {+1, -1, -1}, {-1, -1, -1}, {-1, +1, -1},
      {+1, +1, -1}, {+1, -1, +1}, {+1, -1, -1}, {+1, +1, -1}, {+1, +1, +1},
      {-1, -1, -1}, {-1, -1, +1}, {-1, +1, +1}, {-1, +1, -1}, {-1, +1, +1},
      {+1, +1, +1}, {+1, +1, -1}, {-1, +1, -1}, {+1, -1, +1}, {-1, -1, +1},
      {-1, -1, -1}, {+1, -1, -1}};
  static const auto cube_normals   = vector<vec3f>{{0, 0, +1}, {0, 0, +1},
        {0, 0, +1}, {0, 0, +1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0},
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}};
  static const auto cube_texcoords = vector<vec2f>{{0, 1}, {1, 1}, {1, 0},
      {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0},
      {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1},
      {1, 1}, {1, 0}, {0, 0}};
  static const auto cube_quads     = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
          {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19}, {20, 21, 22, 23}};
  if (subdivisions == 0) {
    quads     = cube_quads;
    positions = cube_positions;
    normals   = cube_normals;
    texcoords = cube_texcoords;
  } else {
    std::tie(quads, positions) = subdivide_quads(
        cube_quads, cube_positions, subdivisions);
    std::tie(quads, normals) = subdivide_quads(
        cube_quads, cube_normals, subdivisions);
    std::tie(quads, texcoords) = subdivide_quads(
        cube_quads, cube_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

void make_fvcube(vector<vec4i>& quadspos, vector<vec4i>& quadsnorm,
    vector<vec4i>& quadstexcoord, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float scale,
    int subdivisions) {
  static const auto fvcube_positions = vector<vec3f>{{-1, -1, +1}, {+1, -1, +1},
      {+1, +1, +1}, {-1, +1, +1}, {+1, -1, -1}, {-1, -1, -1}, {-1, +1, -1},
      {+1, +1, -1}};
  static const auto fvcube_normals   = vector<vec3f>{{0, 0, +1}, {0, 0, +1},
        {0, 0, +1}, {0, 0, +1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {+1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0}, {0, +1, 0},
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}};
  static const auto fvcube_texcoords = vector<vec2f>{{0, 1}, {1, 1}, {1, 0},
      {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0},
      {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1},
      {1, 1}, {1, 0}, {0, 0}};
  static const auto fvcube_quadspos  = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
       {1, 4, 7, 2}, {5, 0, 3, 6}, {3, 2, 7, 6}, {1, 0, 5, 4}};
  static const auto fvcube_quadsnorm = vector<vec4i>{{0, 1, 2, 3}, {4, 5, 6, 7},
      {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19}, {20, 21, 22, 23}};
  static const auto fvcube_quadstexcoord = vector<vec4i>{{0, 1, 2, 3},
      {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15}, {16, 17, 18, 19},
      {20, 21, 22, 23}};
  if (subdivisions == 0) {
    quadspos      = fvcube_quadspos;
    quadsnorm     = fvcube_quadsnorm;
    quadstexcoord = fvcube_quadstexcoord;
    positions     = fvcube_positions;
    normals       = fvcube_normals;
    texcoords     = fvcube_texcoords;
  } else {
    std::tie(quadspos, positions) = subdivide_quads(
        fvcube_quadspos, fvcube_positions, subdivisions);
    std::tie(quadsnorm, normals) = subdivide_quads(
        fvcube_quadsnorm, fvcube_normals, subdivisions);
    std::tie(quadstexcoord, texcoords) = subdivide_quads(
        fvcube_quadstexcoord, fvcube_texcoords, subdivisions);
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

void make_geosphere(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, float scale, int subdivisions) {
  // https://stackoverflow.com/questions/17705621/algorithm-for-a-geodesic-sphere
  const float X                   = 0.525731112119133606f;
  const float Z                   = 0.850650808352039932f;
  static auto geosphere_positions = vector<vec3f>{{-X, 0.0, Z}, {X, 0.0, Z},
      {-X, 0.0, -Z}, {X, 0.0, -Z}, {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X},
      {0.0, -Z, -X}, {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}};
  static auto geosphere_triangles = vector<vec3i>{{0, 1, 4}, {0, 4, 9},
      {9, 4, 5}, {4, 8, 5}, {4, 1, 8}, {8, 1, 10}, {8, 10, 3}, {5, 8, 3},
      {5, 3, 2}, {2, 3, 7}, {7, 3, 10}, {7, 10, 6}, {7, 6, 11}, {11, 6, 0},
      {0, 6, 1}, {6, 10, 1}, {9, 11, 0}, {9, 2, 11}, {9, 5, 2}, {7, 11, 2}};
  if (subdivisions == 0) {
    triangles = geosphere_triangles;
    positions = geosphere_positions;
    normals   = geosphere_positions;
  } else {
    std::tie(triangles, positions) = subdivide_triangles(
        geosphere_triangles, geosphere_positions, subdivisions);
    for (auto& position : positions) position = normalize(position);
    normals = positions;
  }
  if (scale != 1) {
    for (auto& p : positions) p *= scale;
  }
}

// Make a hair ball around a shape
void make_hair(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec3i>& striangles, const vector<vec4i>& squads,
    const vector<vec3f>& spos, const vector<vec3f>& snorm,
    const vector<vec2f>& stexcoord, const vec2i& steps, const vec2f& len,
    const vec2f& rad, const vec2f& noise, const vec2f& clump,
    const vec2f& rotation, int seed) {
  auto alltriangles    = striangles;
  auto quads_triangles = quads_to_triangles(squads);
  alltriangles.insert(
      alltriangles.end(), quads_triangles.begin(), quads_triangles.end());
  auto bpos      = vector<vec3f>{};
  auto bnorm     = vector<vec3f>{};
  auto btexcoord = vector<vec2f>{};
  sample_triangles(bpos, bnorm, btexcoord, alltriangles, spos, snorm, stexcoord,
      steps.y, seed);

  auto rng  = make_rng(seed, 3);
  auto blen = vector<float>(bpos.size());
  for (auto& l : blen) {
    l = lerp(len.x, len.y, rand1f(rng));
  }

  auto cidx = vector<int>();
  if (clump.x > 0) {
    for (auto bidx : range((int)bpos.size())) {
      cidx.push_back(0);
      auto cdist = flt_max;
      for (auto c : range((int)clump.y)) {
        auto d = length(bpos[bidx] - bpos[c]);
        if (d < cdist) {
          cdist       = d;
          cidx.back() = c;
        }
      }
    }
  }

  make_lines(lines, positions, normals, texcoords, radius, steps, {1, 1},
      {1, 1}, {1, 1});
  for (auto i : range((int)positions.size())) {
    auto u       = texcoords[i].x;
    auto bidx    = i / (steps.x + 1);
    positions[i] = bpos[bidx] + bnorm[bidx] * u * blen[bidx];
    normals[i]   = bnorm[bidx];
    radius[i]    = lerp(rad.x, rad.y, u);
    if (clump.x > 0) {
      positions[i] =
          positions[i] +
          (positions[i + (cidx[bidx] - bidx) * (steps.x + 1)] - positions[i]) *
              u * clump.x;
    }
    if (noise.x > 0) {
      auto nx =
          (perlin_noise(positions[i] * noise.y + vec3f{0, 0, 0}) * 2 - 1) *
          noise.x;
      auto ny =
          (perlin_noise(positions[i] * noise.y + vec3f{3, 7, 11}) * 2 - 1) *
          noise.x;
      auto nz =
          (perlin_noise(positions[i] * noise.y + vec3f{13, 17, 19}) * 2 - 1) *
          noise.x;
      positions[i] += {nx, ny, nz};
    }
  }

  if (clump.x > 0 || noise.x > 0 || rotation.x > 0) {
    normals = lines_tangents(lines, positions);
  }
}

// Grow hairs around a shape
void make_hair2(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, vector<float>& radius,
    const vector<vec3i>& striangles, const vector<vec4i>& squads,
    const vector<vec3f>& spos, const vector<vec3f>& snorm,
    const vector<vec2f>& stexcoord, const vec2i& steps, const vec2f& len,
    const vec2f& rad, float noise, float gravity, int seed) {
  auto alltriangles    = striangles;
  auto quads_triangles = quads_to_triangles(squads);
  alltriangles.insert(
      alltriangles.end(), quads_triangles.begin(), quads_triangles.end());
  auto bpositions = vector<vec3f>{};
  auto bnormals   = vector<vec3f>{};
  auto btexcoord  = vector<vec2f>{};
  sample_triangles(bpositions, bnormals, btexcoord, alltriangles, spos, snorm,
      stexcoord, steps.y, seed);

  make_lines(
      lines, positions, normals, texcoords, radius, steps, {1, 1}, {1, 1}, rad);
  auto rng = make_rng(seed);
  for (auto idx : range(steps.y)) {
    auto offset       = idx * (steps.x + 1);
    auto position     = bpositions[idx];
    auto direction    = bnormals[idx];
    auto length       = rand1f(rng) * (len.y - len.x) + len.x;
    positions[offset] = position;
    for (auto iidx = 1; iidx <= steps.x; iidx++) {
      positions[offset + iidx] = position;
      positions[offset + iidx] += direction * length / (float)steps.x;
      positions[offset + iidx] += (2 * rand3f(rng) - 1) * noise;
      positions[offset + iidx] += vec3f{0, -gravity, 0};
      direction = normalize(positions[offset + iidx] - position);
      position  = positions[offset + iidx];
    }
  }

  normals = lines_tangents(lines, positions);
}

// Thickens a shape by copy9ing the shape content, rescaling it and flipping
// its normals. Note that this is very much not robust and only useful for
// trivial cases.
void make_shell(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, float thickness) {
  auto bbox = invalidb3f;
  for (auto p : positions) bbox = merge(bbox, p);
  auto center              = yocto::center(bbox);
  auto inner_quads         = quads;
  auto inner_positions     = positions;
  auto inner_normals       = normals;
  auto inner_texturecoords = texcoords;
  for (auto& p : inner_positions) p = (1 - thickness) * (p - center) + center;
  for (auto& n : inner_normals) n = -n;
  merge_quads(quads, positions, normals, texcoords, inner_quads,
      inner_positions, inner_normals, inner_texturecoords);
}

// Make a heightfield mesh.
void make_heightfield(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& size,
    const vector<float>& height) {
  make_recty(quads, positions, normals, texcoords, size - 1,
      vec2f{(float)size.x, (float)size.y} / (float)max(size), {1, 1});
  for (auto j : range(size.y))
    for (auto i : range(size.x))
      positions[j * size.x + i].y = height[j * size.x + i];
  normals = quads_normals(quads, positions);
}
void make_heightfield(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords, const vec2i& size,
    const vector<vec4f>& color) {
  make_recty(quads, positions, normals, texcoords, size - 1,
      vec2f{(float)size.x, (float)size.y} / (float)max(size), {1, 1});
  for (auto j : range(size.y))
    for (auto i : range(size.x))
      positions[j * size.x + i].y = mean(xyz(color[j * size.x + i]));
  normals = quads_normals(quads, positions);
}

// Convert points to small spheres and lines to small cylinders. This is
// intended for making very small primitives for display in interactive
// applications. It should probably be used without tecoords and maybe
// without normals if not lit.
void points_to_spheres(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps, float scale) {
  auto sphere_quads     = vector<vec4i>{};
  auto sphere_positions = vector<vec3f>{};
  auto sphere_normals   = vector<vec3f>{};
  auto sphere_texcoords = vector<vec2f>{};
  make_sphere(sphere_quads, sphere_positions, sphere_normals, sphere_texcoords,
      steps, scale, 1);
  for (auto& vertex : vertices) {
    auto transformed_positions = sphere_positions;
    for (auto& position : transformed_positions) position += vertex;
    merge_quads(quads, positions, normals, texcoords, sphere_quads,
        transformed_positions, sphere_normals, sphere_texcoords);
  }
}

void polyline_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps, float scale) {
  auto cylinder_quads     = vector<vec4i>{};
  auto cylinder_positions = vector<vec3f>{};
  auto cylinder_normals   = vector<vec3f>{};
  auto cylinder_texcoords = vector<vec2f>{};
  make_uvcylinder(cylinder_quads, cylinder_positions, cylinder_normals,
      cylinder_texcoords, {steps, 1, 1}, {scale, 1}, {1, 1, 1});
  for (auto idx = 0; idx < (int)vertices.size() - 1; idx++) {
    auto frame  = frame_fromz((vertices[idx] + vertices[idx + 1]) / 2,
         vertices[idx] - vertices[idx + 1]);
    auto length = distance(vertices[idx], vertices[idx + 1]);
    auto transformed_positions = cylinder_positions;
    auto transformed_normals   = cylinder_normals;
    for (auto& position : transformed_positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : transformed_normals)
      normal = transform_direction(frame, normal);
    merge_quads(quads, positions, normals, texcoords, cylinder_quads,
        transformed_positions, cylinder_normals, cylinder_texcoords);
  }
}

void lines_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec3f>& vertices, int steps, float scale) {
  auto cylinder_quads     = vector<vec4i>{};
  auto cylinder_positions = vector<vec3f>{};
  auto cylinder_normals   = vector<vec3f>{};
  auto cylinder_texcoords = vector<vec2f>{};
  make_uvcylinder(cylinder_quads, cylinder_positions, cylinder_normals,
      cylinder_texcoords, {steps, 1, 1}, {scale, 1}, {1, 1, 1});
  for (auto idx = 0; idx < (int)vertices.size(); idx += 2) {
    auto frame  = frame_fromz((vertices[idx + 0] + vertices[idx + 1]) / 2,
         vertices[idx + 0] - vertices[idx + 1]);
    auto length = distance(vertices[idx + 0], vertices[idx + 1]);
    auto transformed_positions = cylinder_positions;
    auto transformed_normals   = cylinder_normals;
    for (auto& position : transformed_positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : transformed_normals)
      normal = transform_direction(frame, normal);
    merge_quads(quads, positions, normals, texcoords, cylinder_quads,
        transformed_positions, cylinder_normals, cylinder_texcoords);
  }
}

void lines_to_cylinders(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texcoords,
    const vector<vec2i>& lines, const vector<vec3f>& vertices, int steps,
    float scale) {
  auto cylinder_quads     = vector<vec4i>{};
  auto cylinder_positions = vector<vec3f>{};
  auto cylinder_normals   = vector<vec3f>{};
  auto cylinder_texcoords = vector<vec2f>{};
  make_uvcylinder(cylinder_quads, cylinder_positions, cylinder_normals,
      cylinder_texcoords, {steps, 1, 1}, {scale, 1}, {1, 1, 1});
  for (auto& line : lines) {
    auto frame  = frame_fromz((vertices[line.x] + vertices[line.y]) / 2,
         vertices[line.x] - vertices[line.y]);
    auto length = distance(vertices[line.x], vertices[line.y]);
    auto transformed_positions = cylinder_positions;
    auto transformed_normals   = cylinder_normals;
    for (auto& position : transformed_positions)
      position = transform_point(frame, position * vec3f{1, 1, length / 2});
    for (auto& normal : transformed_normals)
      normal = transform_direction(frame, normal);
    merge_quads(quads, positions, normals, texcoords, cylinder_quads,
        transformed_positions, cylinder_normals, cylinder_texcoords);
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE DATA
// -----------------------------------------------------------------------------
namespace yocto {

vector<vec3f> suzanne_positions = vector<vec3f>{{0.4375, 0.1640625, 0.765625},
    {-0.4375, 0.1640625, 0.765625}, {0.5, 0.09375, 0.6875},
    {-0.5, 0.09375, 0.6875}, {0.546875, 0.0546875, 0.578125},
    {-0.546875, 0.0546875, 0.578125}, {0.3515625, -0.0234375, 0.6171875},
    {-0.3515625, -0.0234375, 0.6171875}, {0.3515625, 0.03125, 0.71875},
    {-0.3515625, 0.03125, 0.71875}, {0.3515625, 0.1328125, 0.78125},
    {-0.3515625, 0.1328125, 0.78125}, {0.2734375, 0.1640625, 0.796875},
    {-0.2734375, 0.1640625, 0.796875}, {0.203125, 0.09375, 0.7421875},
    {-0.203125, 0.09375, 0.7421875}, {0.15625, 0.0546875, 0.6484375},
    {-0.15625, 0.0546875, 0.6484375}, {0.078125, 0.2421875, 0.65625},
    {-0.078125, 0.2421875, 0.65625}, {0.140625, 0.2421875, 0.7421875},
    {-0.140625, 0.2421875, 0.7421875}, {0.2421875, 0.2421875, 0.796875},
    {-0.2421875, 0.2421875, 0.796875}, {0.2734375, 0.328125, 0.796875},
    {-0.2734375, 0.328125, 0.796875}, {0.203125, 0.390625, 0.7421875},
    {-0.203125, 0.390625, 0.7421875}, {0.15625, 0.4375, 0.6484375},
    {-0.15625, 0.4375, 0.6484375}, {0.3515625, 0.515625, 0.6171875},
    {-0.3515625, 0.515625, 0.6171875}, {0.3515625, 0.453125, 0.71875},
    {-0.3515625, 0.453125, 0.71875}, {0.3515625, 0.359375, 0.78125},
    {-0.3515625, 0.359375, 0.78125}, {0.4375, 0.328125, 0.765625},
    {-0.4375, 0.328125, 0.765625}, {0.5, 0.390625, 0.6875},
    {-0.5, 0.390625, 0.6875}, {0.546875, 0.4375, 0.578125},
    {-0.546875, 0.4375, 0.578125}, {0.625, 0.2421875, 0.5625},
    {-0.625, 0.2421875, 0.5625}, {0.5625, 0.2421875, 0.671875},
    {-0.5625, 0.2421875, 0.671875}, {0.46875, 0.2421875, 0.7578125},
    {-0.46875, 0.2421875, 0.7578125}, {0.4765625, 0.2421875, 0.7734375},
    {-0.4765625, 0.2421875, 0.7734375}, {0.4453125, 0.3359375, 0.78125},
    {-0.4453125, 0.3359375, 0.78125}, {0.3515625, 0.375, 0.8046875},
    {-0.3515625, 0.375, 0.8046875}, {0.265625, 0.3359375, 0.8203125},
    {-0.265625, 0.3359375, 0.8203125}, {0.2265625, 0.2421875, 0.8203125},
    {-0.2265625, 0.2421875, 0.8203125}, {0.265625, 0.15625, 0.8203125},
    {-0.265625, 0.15625, 0.8203125}, {0.3515625, 0.2421875, 0.828125},
    {-0.3515625, 0.2421875, 0.828125}, {0.3515625, 0.1171875, 0.8046875},
    {-0.3515625, 0.1171875, 0.8046875}, {0.4453125, 0.15625, 0.78125},
    {-0.4453125, 0.15625, 0.78125}, {0.0, 0.4296875, 0.7421875},
    {0.0, 0.3515625, 0.8203125}, {0.0, -0.6796875, 0.734375},
    {0.0, -0.3203125, 0.78125}, {0.0, -0.1875, 0.796875},
    {0.0, -0.7734375, 0.71875}, {0.0, 0.40625, 0.6015625},
    {0.0, 0.5703125, 0.5703125}, {0.0, 0.8984375, -0.546875},
    {0.0, 0.5625, -0.8515625}, {0.0, 0.0703125, -0.828125},
    {0.0, -0.3828125, -0.3515625}, {0.203125, -0.1875, 0.5625},
    {-0.203125, -0.1875, 0.5625}, {0.3125, -0.4375, 0.5703125},
    {-0.3125, -0.4375, 0.5703125}, {0.3515625, -0.6953125, 0.5703125},
    {-0.3515625, -0.6953125, 0.5703125}, {0.3671875, -0.890625, 0.53125},
    {-0.3671875, -0.890625, 0.53125}, {0.328125, -0.9453125, 0.5234375},
    {-0.328125, -0.9453125, 0.5234375}, {0.1796875, -0.96875, 0.5546875},
    {-0.1796875, -0.96875, 0.5546875}, {0.0, -0.984375, 0.578125},
    {0.4375, -0.140625, 0.53125}, {-0.4375, -0.140625, 0.53125},
    {0.6328125, -0.0390625, 0.5390625}, {-0.6328125, -0.0390625, 0.5390625},
    {0.828125, 0.1484375, 0.4453125}, {-0.828125, 0.1484375, 0.4453125},
    {0.859375, 0.4296875, 0.59375}, {-0.859375, 0.4296875, 0.59375},
    {0.7109375, 0.484375, 0.625}, {-0.7109375, 0.484375, 0.625},
    {0.4921875, 0.6015625, 0.6875}, {-0.4921875, 0.6015625, 0.6875},
    {0.3203125, 0.7578125, 0.734375}, {-0.3203125, 0.7578125, 0.734375},
    {0.15625, 0.71875, 0.7578125}, {-0.15625, 0.71875, 0.7578125},
    {0.0625, 0.4921875, 0.75}, {-0.0625, 0.4921875, 0.75},
    {0.1640625, 0.4140625, 0.7734375}, {-0.1640625, 0.4140625, 0.7734375},
    {0.125, 0.3046875, 0.765625}, {-0.125, 0.3046875, 0.765625},
    {0.203125, 0.09375, 0.7421875}, {-0.203125, 0.09375, 0.7421875},
    {0.375, 0.015625, 0.703125}, {-0.375, 0.015625, 0.703125},
    {0.4921875, 0.0625, 0.671875}, {-0.4921875, 0.0625, 0.671875},
    {0.625, 0.1875, 0.6484375}, {-0.625, 0.1875, 0.6484375},
    {0.640625, 0.296875, 0.6484375}, {-0.640625, 0.296875, 0.6484375},
    {0.6015625, 0.375, 0.6640625}, {-0.6015625, 0.375, 0.6640625},
    {0.4296875, 0.4375, 0.71875}, {-0.4296875, 0.4375, 0.71875},
    {0.25, 0.46875, 0.7578125}, {-0.25, 0.46875, 0.7578125},
    {0.0, -0.765625, 0.734375}, {0.109375, -0.71875, 0.734375},
    {-0.109375, -0.71875, 0.734375}, {0.1171875, -0.8359375, 0.7109375},
    {-0.1171875, -0.8359375, 0.7109375}, {0.0625, -0.8828125, 0.6953125},
    {-0.0625, -0.8828125, 0.6953125}, {0.0, -0.890625, 0.6875},
    {0.0, -0.1953125, 0.75}, {0.0, -0.140625, 0.7421875},
    {0.1015625, -0.1484375, 0.7421875}, {-0.1015625, -0.1484375, 0.7421875},
    {0.125, -0.2265625, 0.75}, {-0.125, -0.2265625, 0.75},
    {0.0859375, -0.2890625, 0.7421875}, {-0.0859375, -0.2890625, 0.7421875},
    {0.3984375, -0.046875, 0.671875}, {-0.3984375, -0.046875, 0.671875},
    {0.6171875, 0.0546875, 0.625}, {-0.6171875, 0.0546875, 0.625},
    {0.7265625, 0.203125, 0.6015625}, {-0.7265625, 0.203125, 0.6015625},
    {0.7421875, 0.375, 0.65625}, {-0.7421875, 0.375, 0.65625},
    {0.6875, 0.4140625, 0.7265625}, {-0.6875, 0.4140625, 0.7265625},
    {0.4375, 0.546875, 0.796875}, {-0.4375, 0.546875, 0.796875},
    {0.3125, 0.640625, 0.8359375}, {-0.3125, 0.640625, 0.8359375},
    {0.203125, 0.6171875, 0.8515625}, {-0.203125, 0.6171875, 0.8515625},
    {0.1015625, 0.4296875, 0.84375}, {-0.1015625, 0.4296875, 0.84375},
    {0.125, -0.1015625, 0.8125}, {-0.125, -0.1015625, 0.8125},
    {0.2109375, -0.4453125, 0.7109375}, {-0.2109375, -0.4453125, 0.7109375},
    {0.25, -0.703125, 0.6875}, {-0.25, -0.703125, 0.6875},
    {0.265625, -0.8203125, 0.6640625}, {-0.265625, -0.8203125, 0.6640625},
    {0.234375, -0.9140625, 0.6328125}, {-0.234375, -0.9140625, 0.6328125},
    {0.1640625, -0.9296875, 0.6328125}, {-0.1640625, -0.9296875, 0.6328125},
    {0.0, -0.9453125, 0.640625}, {0.0, 0.046875, 0.7265625},
    {0.0, 0.2109375, 0.765625}, {0.328125, 0.4765625, 0.7421875},
    {-0.328125, 0.4765625, 0.7421875}, {0.1640625, 0.140625, 0.75},
    {-0.1640625, 0.140625, 0.75}, {0.1328125, 0.2109375, 0.7578125},
    {-0.1328125, 0.2109375, 0.7578125}, {0.1171875, -0.6875, 0.734375},
    {-0.1171875, -0.6875, 0.734375}, {0.078125, -0.4453125, 0.75},
    {-0.078125, -0.4453125, 0.75}, {0.0, -0.4453125, 0.75},
    {0.0, -0.328125, 0.7421875}, {0.09375, -0.2734375, 0.78125},
    {-0.09375, -0.2734375, 0.78125}, {0.1328125, -0.2265625, 0.796875},
    {-0.1328125, -0.2265625, 0.796875}, {0.109375, -0.1328125, 0.78125},
    {-0.109375, -0.1328125, 0.78125}, {0.0390625, -0.125, 0.78125},
    {-0.0390625, -0.125, 0.78125}, {0.0, -0.203125, 0.828125},
    {0.046875, -0.1484375, 0.8125}, {-0.046875, -0.1484375, 0.8125},
    {0.09375, -0.15625, 0.8125}, {-0.09375, -0.15625, 0.8125},
    {0.109375, -0.2265625, 0.828125}, {-0.109375, -0.2265625, 0.828125},
    {0.078125, -0.25, 0.8046875}, {-0.078125, -0.25, 0.8046875},
    {0.0, -0.2890625, 0.8046875}, {0.2578125, -0.3125, 0.5546875},
    {-0.2578125, -0.3125, 0.5546875}, {0.1640625, -0.2421875, 0.7109375},
    {-0.1640625, -0.2421875, 0.7109375}, {0.1796875, -0.3125, 0.7109375},
    {-0.1796875, -0.3125, 0.7109375}, {0.234375, -0.25, 0.5546875},
    {-0.234375, -0.25, 0.5546875}, {0.0, -0.875, 0.6875},
    {0.046875, -0.8671875, 0.6875}, {-0.046875, -0.8671875, 0.6875},
    {0.09375, -0.8203125, 0.7109375}, {-0.09375, -0.8203125, 0.7109375},
    {0.09375, -0.7421875, 0.7265625}, {-0.09375, -0.7421875, 0.7265625},
    {0.0, -0.78125, 0.65625}, {0.09375, -0.75, 0.6640625},
    {-0.09375, -0.75, 0.6640625}, {0.09375, -0.8125, 0.640625},
    {-0.09375, -0.8125, 0.640625}, {0.046875, -0.8515625, 0.6328125},
    {-0.046875, -0.8515625, 0.6328125}, {0.0, -0.859375, 0.6328125},
    {0.171875, 0.21875, 0.78125}, {-0.171875, 0.21875, 0.78125},
    {0.1875, 0.15625, 0.7734375}, {-0.1875, 0.15625, 0.7734375},
    {0.3359375, 0.4296875, 0.7578125}, {-0.3359375, 0.4296875, 0.7578125},
    {0.2734375, 0.421875, 0.7734375}, {-0.2734375, 0.421875, 0.7734375},
    {0.421875, 0.3984375, 0.7734375}, {-0.421875, 0.3984375, 0.7734375},
    {0.5625, 0.3515625, 0.6953125}, {-0.5625, 0.3515625, 0.6953125},
    {0.5859375, 0.2890625, 0.6875}, {-0.5859375, 0.2890625, 0.6875},
    {0.578125, 0.1953125, 0.6796875}, {-0.578125, 0.1953125, 0.6796875},
    {0.4765625, 0.1015625, 0.71875}, {-0.4765625, 0.1015625, 0.71875},
    {0.375, 0.0625, 0.7421875}, {-0.375, 0.0625, 0.7421875},
    {0.2265625, 0.109375, 0.78125}, {-0.2265625, 0.109375, 0.78125},
    {0.1796875, 0.296875, 0.78125}, {-0.1796875, 0.296875, 0.78125},
    {0.2109375, 0.375, 0.78125}, {-0.2109375, 0.375, 0.78125},
    {0.234375, 0.359375, 0.7578125}, {-0.234375, 0.359375, 0.7578125},
    {0.1953125, 0.296875, 0.7578125}, {-0.1953125, 0.296875, 0.7578125},
    {0.2421875, 0.125, 0.7578125}, {-0.2421875, 0.125, 0.7578125},
    {0.375, 0.0859375, 0.7265625}, {-0.375, 0.0859375, 0.7265625},
    {0.4609375, 0.1171875, 0.703125}, {-0.4609375, 0.1171875, 0.703125},
    {0.546875, 0.2109375, 0.671875}, {-0.546875, 0.2109375, 0.671875},
    {0.5546875, 0.28125, 0.671875}, {-0.5546875, 0.28125, 0.671875},
    {0.53125, 0.3359375, 0.6796875}, {-0.53125, 0.3359375, 0.6796875},
    {0.4140625, 0.390625, 0.75}, {-0.4140625, 0.390625, 0.75},
    {0.28125, 0.3984375, 0.765625}, {-0.28125, 0.3984375, 0.765625},
    {0.3359375, 0.40625, 0.75}, {-0.3359375, 0.40625, 0.75},
    {0.203125, 0.171875, 0.75}, {-0.203125, 0.171875, 0.75},
    {0.1953125, 0.2265625, 0.75}, {-0.1953125, 0.2265625, 0.75},
    {0.109375, 0.4609375, 0.609375}, {-0.109375, 0.4609375, 0.609375},
    {0.1953125, 0.6640625, 0.6171875}, {-0.1953125, 0.6640625, 0.6171875},
    {0.3359375, 0.6875, 0.59375}, {-0.3359375, 0.6875, 0.59375},
    {0.484375, 0.5546875, 0.5546875}, {-0.484375, 0.5546875, 0.5546875},
    {0.6796875, 0.453125, 0.4921875}, {-0.6796875, 0.453125, 0.4921875},
    {0.796875, 0.40625, 0.4609375}, {-0.796875, 0.40625, 0.4609375},
    {0.7734375, 0.1640625, 0.375}, {-0.7734375, 0.1640625, 0.375},
    {0.6015625, 0.0, 0.4140625}, {-0.6015625, 0.0, 0.4140625},
    {0.4375, -0.09375, 0.46875}, {-0.4375, -0.09375, 0.46875},
    {0.0, 0.8984375, 0.2890625}, {0.0, 0.984375, -0.078125},
    {0.0, -0.1953125, -0.671875}, {0.0, -0.4609375, 0.1875},
    {0.0, -0.9765625, 0.4609375}, {0.0, -0.8046875, 0.34375},
    {0.0, -0.5703125, 0.3203125}, {0.0, -0.484375, 0.28125},
    {0.8515625, 0.234375, 0.0546875}, {-0.8515625, 0.234375, 0.0546875},
    {0.859375, 0.3203125, -0.046875}, {-0.859375, 0.3203125, -0.046875},
    {0.7734375, 0.265625, -0.4375}, {-0.7734375, 0.265625, -0.4375},
    {0.4609375, 0.4375, -0.703125}, {-0.4609375, 0.4375, -0.703125},
    {0.734375, -0.046875, 0.0703125}, {-0.734375, -0.046875, 0.0703125},
    {0.59375, -0.125, -0.1640625}, {-0.59375, -0.125, -0.1640625},
    {0.640625, -0.0078125, -0.4296875}, {-0.640625, -0.0078125, -0.4296875},
    {0.3359375, 0.0546875, -0.6640625}, {-0.3359375, 0.0546875, -0.6640625},
    {0.234375, -0.3515625, 0.40625}, {-0.234375, -0.3515625, 0.40625},
    {0.1796875, -0.4140625, 0.2578125}, {-0.1796875, -0.4140625, 0.2578125},
    {0.2890625, -0.7109375, 0.3828125}, {-0.2890625, -0.7109375, 0.3828125},
    {0.25, -0.5, 0.390625}, {-0.25, -0.5, 0.390625},
    {0.328125, -0.9140625, 0.3984375}, {-0.328125, -0.9140625, 0.3984375},
    {0.140625, -0.7578125, 0.3671875}, {-0.140625, -0.7578125, 0.3671875},
    {0.125, -0.5390625, 0.359375}, {-0.125, -0.5390625, 0.359375},
    {0.1640625, -0.9453125, 0.4375}, {-0.1640625, -0.9453125, 0.4375},
    {0.21875, -0.28125, 0.4296875}, {-0.21875, -0.28125, 0.4296875},
    {0.2109375, -0.2265625, 0.46875}, {-0.2109375, -0.2265625, 0.46875},
    {0.203125, -0.171875, 0.5}, {-0.203125, -0.171875, 0.5},
    {0.2109375, -0.390625, 0.1640625}, {-0.2109375, -0.390625, 0.1640625},
    {0.296875, -0.3125, -0.265625}, {-0.296875, -0.3125, -0.265625},
    {0.34375, -0.1484375, -0.5390625}, {-0.34375, -0.1484375, -0.5390625},
    {0.453125, 0.8671875, -0.3828125}, {-0.453125, 0.8671875, -0.3828125},
    {0.453125, 0.9296875, -0.0703125}, {-0.453125, 0.9296875, -0.0703125},
    {0.453125, 0.8515625, 0.234375}, {-0.453125, 0.8515625, 0.234375},
    {0.4609375, 0.5234375, 0.4296875}, {-0.4609375, 0.5234375, 0.4296875},
    {0.7265625, 0.40625, 0.3359375}, {-0.7265625, 0.40625, 0.3359375},
    {0.6328125, 0.453125, 0.28125}, {-0.6328125, 0.453125, 0.28125},
    {0.640625, 0.703125, 0.0546875}, {-0.640625, 0.703125, 0.0546875},
    {0.796875, 0.5625, 0.125}, {-0.796875, 0.5625, 0.125},
    {0.796875, 0.6171875, -0.1171875}, {-0.796875, 0.6171875, -0.1171875},
    {0.640625, 0.75, -0.1953125}, {-0.640625, 0.75, -0.1953125},
    {0.640625, 0.6796875, -0.4453125}, {-0.640625, 0.6796875, -0.4453125},
    {0.796875, 0.5390625, -0.359375}, {-0.796875, 0.5390625, -0.359375},
    {0.6171875, 0.328125, -0.5859375}, {-0.6171875, 0.328125, -0.5859375},
    {0.484375, 0.0234375, -0.546875}, {-0.484375, 0.0234375, -0.546875},
    {0.8203125, 0.328125, -0.203125}, {-0.8203125, 0.328125, -0.203125},
    {0.40625, -0.171875, 0.1484375}, {-0.40625, -0.171875, 0.1484375},
    {0.4296875, -0.1953125, -0.2109375}, {-0.4296875, -0.1953125, -0.2109375},
    {0.890625, 0.40625, -0.234375}, {-0.890625, 0.40625, -0.234375},
    {0.7734375, -0.140625, -0.125}, {-0.7734375, -0.140625, -0.125},
    {1.0390625, -0.1015625, -0.328125}, {-1.0390625, -0.1015625, -0.328125},
    {1.28125, 0.0546875, -0.4296875}, {-1.28125, 0.0546875, -0.4296875},
    {1.3515625, 0.3203125, -0.421875}, {-1.3515625, 0.3203125, -0.421875},
    {1.234375, 0.5078125, -0.421875}, {-1.234375, 0.5078125, -0.421875},
    {1.0234375, 0.4765625, -0.3125}, {-1.0234375, 0.4765625, -0.3125},
    {1.015625, 0.4140625, -0.2890625}, {-1.015625, 0.4140625, -0.2890625},
    {1.1875, 0.4375, -0.390625}, {-1.1875, 0.4375, -0.390625},
    {1.265625, 0.2890625, -0.40625}, {-1.265625, 0.2890625, -0.40625},
    {1.2109375, 0.078125, -0.40625}, {-1.2109375, 0.078125, -0.40625},
    {1.03125, -0.0390625, -0.3046875}, {-1.03125, -0.0390625, -0.3046875},
    {0.828125, -0.0703125, -0.1328125}, {-0.828125, -0.0703125, -0.1328125},
    {0.921875, 0.359375, -0.21875}, {-0.921875, 0.359375, -0.21875},
    {0.9453125, 0.3046875, -0.2890625}, {-0.9453125, 0.3046875, -0.2890625},
    {0.8828125, -0.0234375, -0.2109375}, {-0.8828125, -0.0234375, -0.2109375},
    {1.0390625, 0.0, -0.3671875}, {-1.0390625, 0.0, -0.3671875},
    {1.1875, 0.09375, -0.4453125}, {-1.1875, 0.09375, -0.4453125},
    {1.234375, 0.25, -0.4453125}, {-1.234375, 0.25, -0.4453125},
    {1.171875, 0.359375, -0.4375}, {-1.171875, 0.359375, -0.4375},
    {1.0234375, 0.34375, -0.359375}, {-1.0234375, 0.34375, -0.359375},
    {0.84375, 0.2890625, -0.2109375}, {-0.84375, 0.2890625, -0.2109375},
    {0.8359375, 0.171875, -0.2734375}, {-0.8359375, 0.171875, -0.2734375},
    {0.7578125, 0.09375, -0.2734375}, {-0.7578125, 0.09375, -0.2734375},
    {0.8203125, 0.0859375, -0.2734375}, {-0.8203125, 0.0859375, -0.2734375},
    {0.84375, 0.015625, -0.2734375}, {-0.84375, 0.015625, -0.2734375},
    {0.8125, -0.015625, -0.2734375}, {-0.8125, -0.015625, -0.2734375},
    {0.7265625, 0.0, -0.0703125}, {-0.7265625, 0.0, -0.0703125},
    {0.71875, -0.0234375, -0.171875}, {-0.71875, -0.0234375, -0.171875},
    {0.71875, 0.0390625, -0.1875}, {-0.71875, 0.0390625, -0.1875},
    {0.796875, 0.203125, -0.2109375}, {-0.796875, 0.203125, -0.2109375},
    {0.890625, 0.2421875, -0.265625}, {-0.890625, 0.2421875, -0.265625},
    {0.890625, 0.234375, -0.3203125}, {-0.890625, 0.234375, -0.3203125},
    {0.8125, -0.015625, -0.3203125}, {-0.8125, -0.015625, -0.3203125},
    {0.8515625, 0.015625, -0.3203125}, {-0.8515625, 0.015625, -0.3203125},
    {0.828125, 0.078125, -0.3203125}, {-0.828125, 0.078125, -0.3203125},
    {0.765625, 0.09375, -0.3203125}, {-0.765625, 0.09375, -0.3203125},
    {0.84375, 0.171875, -0.3203125}, {-0.84375, 0.171875, -0.3203125},
    {1.0390625, 0.328125, -0.4140625}, {-1.0390625, 0.328125, -0.4140625},
    {1.1875, 0.34375, -0.484375}, {-1.1875, 0.34375, -0.484375},
    {1.2578125, 0.2421875, -0.4921875}, {-1.2578125, 0.2421875, -0.4921875},
    {1.2109375, 0.0859375, -0.484375}, {-1.2109375, 0.0859375, -0.484375},
    {1.046875, 0.0, -0.421875}, {-1.046875, 0.0, -0.421875},
    {0.8828125, -0.015625, -0.265625}, {-0.8828125, -0.015625, -0.265625},
    {0.953125, 0.2890625, -0.34375}, {-0.953125, 0.2890625, -0.34375},
    {0.890625, 0.109375, -0.328125}, {-0.890625, 0.109375, -0.328125},
    {0.9375, 0.0625, -0.3359375}, {-0.9375, 0.0625, -0.3359375},
    {1.0, 0.125, -0.3671875}, {-1.0, 0.125, -0.3671875},
    {0.9609375, 0.171875, -0.3515625}, {-0.9609375, 0.171875, -0.3515625},
    {1.015625, 0.234375, -0.375}, {-1.015625, 0.234375, -0.375},
    {1.0546875, 0.1875, -0.3828125}, {-1.0546875, 0.1875, -0.3828125},
    {1.109375, 0.2109375, -0.390625}, {-1.109375, 0.2109375, -0.390625},
    {1.0859375, 0.2734375, -0.390625}, {-1.0859375, 0.2734375, -0.390625},
    {1.0234375, 0.4375, -0.484375}, {-1.0234375, 0.4375, -0.484375},
    {1.25, 0.46875, -0.546875}, {-1.25, 0.46875, -0.546875},
    {1.3671875, 0.296875, -0.5}, {-1.3671875, 0.296875, -0.5},
    {1.3125, 0.0546875, -0.53125}, {-1.3125, 0.0546875, -0.53125},
    {1.0390625, -0.0859375, -0.4921875}, {-1.0390625, -0.0859375, -0.4921875},
    {0.7890625, -0.125, -0.328125}, {-0.7890625, -0.125, -0.328125},
    {0.859375, 0.3828125, -0.3828125}, {-0.859375, 0.3828125, -0.3828125}};

vector<vec4i> suzanne_quads = vector<vec4i>{{46, 0, 2, 44}, {3, 1, 47, 45},
    {44, 2, 4, 42}, {5, 3, 45, 43}, {2, 8, 6, 4}, {7, 9, 3, 5}, {0, 10, 8, 2},
    {9, 11, 1, 3}, {10, 12, 14, 8}, {15, 13, 11, 9}, {8, 14, 16, 6},
    {17, 15, 9, 7}, {14, 20, 18, 16}, {19, 21, 15, 17}, {12, 22, 20, 14},
    {21, 23, 13, 15}, {22, 24, 26, 20}, {27, 25, 23, 21}, {20, 26, 28, 18},
    {29, 27, 21, 19}, {26, 32, 30, 28}, {31, 33, 27, 29}, {24, 34, 32, 26},
    {33, 35, 25, 27}, {34, 36, 38, 32}, {39, 37, 35, 33}, {32, 38, 40, 30},
    {41, 39, 33, 31}, {38, 44, 42, 40}, {43, 45, 39, 41}, {36, 46, 44, 38},
    {45, 47, 37, 39}, {46, 36, 50, 48}, {51, 37, 47, 49}, {36, 34, 52, 50},
    {53, 35, 37, 51}, {34, 24, 54, 52}, {55, 25, 35, 53}, {24, 22, 56, 54},
    {57, 23, 25, 55}, {22, 12, 58, 56}, {59, 13, 23, 57}, {12, 10, 62, 58},
    {63, 11, 13, 59}, {10, 0, 64, 62}, {65, 1, 11, 63}, {0, 46, 48, 64},
    {49, 47, 1, 65}, {88, 173, 175, 90}, {175, 174, 89, 90}, {86, 171, 173, 88},
    {174, 172, 87, 89}, {84, 169, 171, 86}, {172, 170, 85, 87},
    {82, 167, 169, 84}, {170, 168, 83, 85}, {80, 165, 167, 82},
    {168, 166, 81, 83}, {78, 91, 145, 163}, {146, 92, 79, 164},
    {91, 93, 147, 145}, {148, 94, 92, 146}, {93, 95, 149, 147},
    {150, 96, 94, 148}, {95, 97, 151, 149}, {152, 98, 96, 150},
    {97, 99, 153, 151}, {154, 100, 98, 152}, {99, 101, 155, 153},
    {156, 102, 100, 154}, {101, 103, 157, 155}, {158, 104, 102, 156},
    {103, 105, 159, 157}, {160, 106, 104, 158}, {105, 107, 161, 159},
    {162, 108, 106, 160}, {107, 66, 67, 161}, {67, 66, 108, 162},
    {109, 127, 159, 161}, {160, 128, 110, 162}, {127, 178, 157, 159},
    {158, 179, 128, 160}, {125, 155, 157, 178}, {158, 156, 126, 179},
    {123, 153, 155, 125}, {156, 154, 124, 126}, {121, 151, 153, 123},
    {154, 152, 122, 124}, {119, 149, 151, 121}, {152, 150, 120, 122},
    {117, 147, 149, 119}, {150, 148, 118, 120}, {115, 145, 147, 117},
    {148, 146, 116, 118}, {113, 163, 145, 115}, {146, 164, 114, 116},
    {113, 180, 176, 163}, {176, 181, 114, 164}, {109, 161, 67, 111},
    {67, 162, 110, 112}, {111, 67, 177, 182}, {177, 67, 112, 183},
    {176, 180, 182, 177}, {183, 181, 176, 177}, {134, 136, 175, 173},
    {175, 136, 135, 174}, {132, 134, 173, 171}, {174, 135, 133, 172},
    {130, 132, 171, 169}, {172, 133, 131, 170}, {165, 186, 184, 167},
    {185, 187, 166, 168}, {130, 169, 167, 184}, {168, 170, 131, 185},
    {143, 189, 188, 186}, {188, 189, 144, 187}, {184, 186, 188, 68},
    {188, 187, 185, 68}, {129, 130, 184, 68}, {185, 131, 129, 68},
    {141, 192, 190, 143}, {191, 193, 142, 144}, {139, 194, 192, 141},
    {193, 195, 140, 142}, {138, 196, 194, 139}, {195, 197, 138, 140},
    {137, 70, 196, 138}, {197, 70, 137, 138}, {189, 143, 190, 69},
    {191, 144, 189, 69}, {69, 190, 205, 207}, {206, 191, 69, 207},
    {70, 198, 199, 196}, {200, 198, 70, 197}, {196, 199, 201, 194},
    {202, 200, 197, 195}, {194, 201, 203, 192}, {204, 202, 195, 193},
    {192, 203, 205, 190}, {206, 204, 193, 191}, {198, 203, 201, 199},
    {202, 204, 198, 200}, {198, 207, 205, 203}, {206, 207, 198, 204},
    {138, 139, 163, 176}, {164, 140, 138, 176}, {139, 141, 210, 163},
    {211, 142, 140, 164}, {141, 143, 212, 210}, {213, 144, 142, 211},
    {143, 186, 165, 212}, {166, 187, 144, 213}, {80, 208, 212, 165},
    {213, 209, 81, 166}, {208, 214, 210, 212}, {211, 215, 209, 213},
    {78, 163, 210, 214}, {211, 164, 79, 215}, {130, 129, 71, 221},
    {71, 129, 131, 222}, {132, 130, 221, 219}, {222, 131, 133, 220},
    {134, 132, 219, 217}, {220, 133, 135, 218}, {136, 134, 217, 216},
    {218, 135, 136, 216}, {216, 217, 228, 230}, {229, 218, 216, 230},
    {217, 219, 226, 228}, {227, 220, 218, 229}, {219, 221, 224, 226},
    {225, 222, 220, 227}, {221, 71, 223, 224}, {223, 71, 222, 225},
    {223, 230, 228, 224}, {229, 230, 223, 225}, {182, 180, 233, 231},
    {234, 181, 183, 232}, {111, 182, 231, 253}, {232, 183, 112, 254},
    {109, 111, 253, 255}, {254, 112, 110, 256}, {180, 113, 251, 233},
    {252, 114, 181, 234}, {113, 115, 249, 251}, {250, 116, 114, 252},
    {115, 117, 247, 249}, {248, 118, 116, 250}, {117, 119, 245, 247},
    {246, 120, 118, 248}, {119, 121, 243, 245}, {244, 122, 120, 246},
    {121, 123, 241, 243}, {242, 124, 122, 244}, {123, 125, 239, 241},
    {240, 126, 124, 242}, {125, 178, 235, 239}, {236, 179, 126, 240},
    {178, 127, 237, 235}, {238, 128, 179, 236}, {127, 109, 255, 237},
    {256, 110, 128, 238}, {237, 255, 257, 275}, {258, 256, 238, 276},
    {235, 237, 275, 277}, {276, 238, 236, 278}, {239, 235, 277, 273},
    {278, 236, 240, 274}, {241, 239, 273, 271}, {274, 240, 242, 272},
    {243, 241, 271, 269}, {272, 242, 244, 270}, {245, 243, 269, 267},
    {270, 244, 246, 268}, {247, 245, 267, 265}, {268, 246, 248, 266},
    {249, 247, 265, 263}, {266, 248, 250, 264}, {251, 249, 263, 261},
    {264, 250, 252, 262}, {233, 251, 261, 279}, {262, 252, 234, 280},
    {255, 253, 259, 257}, {260, 254, 256, 258}, {253, 231, 281, 259},
    {282, 232, 254, 260}, {231, 233, 279, 281}, {280, 234, 232, 282},
    {66, 107, 283, 72}, {284, 108, 66, 72}, {107, 105, 285, 283},
    {286, 106, 108, 284}, {105, 103, 287, 285}, {288, 104, 106, 286},
    {103, 101, 289, 287}, {290, 102, 104, 288}, {101, 99, 291, 289},
    {292, 100, 102, 290}, {99, 97, 293, 291}, {294, 98, 100, 292},
    {97, 95, 295, 293}, {296, 96, 98, 294}, {95, 93, 297, 295},
    {298, 94, 96, 296}, {93, 91, 299, 297}, {300, 92, 94, 298},
    {307, 308, 327, 337}, {328, 308, 307, 338}, {306, 307, 337, 335},
    {338, 307, 306, 336}, {305, 306, 335, 339}, {336, 306, 305, 340},
    {88, 90, 305, 339}, {305, 90, 89, 340}, {86, 88, 339, 333},
    {340, 89, 87, 334}, {84, 86, 333, 329}, {334, 87, 85, 330},
    {82, 84, 329, 331}, {330, 85, 83, 332}, {329, 335, 337, 331},
    {338, 336, 330, 332}, {329, 333, 339, 335}, {340, 334, 330, 336},
    {325, 331, 337, 327}, {338, 332, 326, 328}, {80, 82, 331, 325},
    {332, 83, 81, 326}, {208, 341, 343, 214}, {344, 342, 209, 215},
    {80, 325, 341, 208}, {342, 326, 81, 209}, {78, 214, 343, 345},
    {344, 215, 79, 346}, {78, 345, 299, 91}, {300, 346, 79, 92},
    {76, 323, 351, 303}, {352, 324, 76, 303}, {303, 351, 349, 77},
    {350, 352, 303, 77}, {77, 349, 347, 304}, {348, 350, 77, 304},
    {304, 347, 327, 308}, {328, 348, 304, 308}, {325, 327, 347, 341},
    {348, 328, 326, 342}, {295, 297, 317, 309}, {318, 298, 296, 310},
    {75, 315, 323, 76}, {324, 316, 75, 76}, {301, 357, 355, 302},
    {356, 358, 301, 302}, {302, 355, 353, 74}, {354, 356, 302, 74},
    {74, 353, 315, 75}, {316, 354, 74, 75}, {291, 293, 361, 363},
    {362, 294, 292, 364}, {363, 361, 367, 365}, {368, 362, 364, 366},
    {365, 367, 369, 371}, {370, 368, 366, 372}, {371, 369, 375, 373},
    {376, 370, 372, 374}, {313, 377, 373, 375}, {374, 378, 314, 376},
    {315, 353, 373, 377}, {374, 354, 316, 378}, {353, 355, 371, 373},
    {372, 356, 354, 374}, {355, 357, 365, 371}, {366, 358, 356, 372},
    {357, 359, 363, 365}, {364, 360, 358, 366}, {289, 291, 363, 359},
    {364, 292, 290, 360}, {73, 359, 357, 301}, {358, 360, 73, 301},
    {283, 285, 287, 289}, {288, 286, 284, 290}, {283, 289, 359, 73},
    {360, 290, 284, 73}, {293, 295, 309, 361}, {310, 296, 294, 362},
    {309, 311, 367, 361}, {368, 312, 310, 362}, {311, 381, 369, 367},
    {370, 382, 312, 368}, {313, 375, 369, 381}, {370, 376, 314, 382},
    {347, 349, 385, 383}, {386, 350, 348, 384}, {317, 383, 385, 319},
    {386, 384, 318, 320}, {297, 299, 383, 317}, {384, 300, 298, 318},
    {299, 343, 341, 383}, {342, 344, 300, 384}, {313, 321, 379, 377},
    {380, 322, 314, 378}, {315, 377, 379, 323}, {380, 378, 316, 324},
    {319, 385, 379, 321}, {380, 386, 320, 322}, {349, 351, 379, 385},
    {380, 352, 350, 386}, {399, 387, 413, 401}, {414, 388, 400, 402},
    {399, 401, 403, 397}, {404, 402, 400, 398}, {397, 403, 405, 395},
    {406, 404, 398, 396}, {395, 405, 407, 393}, {408, 406, 396, 394},
    {393, 407, 409, 391}, {410, 408, 394, 392}, {391, 409, 411, 389},
    {412, 410, 392, 390}, {409, 419, 417, 411}, {418, 420, 410, 412},
    {407, 421, 419, 409}, {420, 422, 408, 410}, {405, 423, 421, 407},
    {422, 424, 406, 408}, {403, 425, 423, 405}, {424, 426, 404, 406},
    {401, 427, 425, 403}, {426, 428, 402, 404}, {401, 413, 415, 427},
    {416, 414, 402, 428}, {317, 319, 443, 441}, {444, 320, 318, 442},
    {319, 389, 411, 443}, {412, 390, 320, 444}, {309, 317, 441, 311},
    {442, 318, 310, 312}, {381, 429, 413, 387}, {414, 430, 382, 388},
    {411, 417, 439, 443}, {440, 418, 412, 444}, {437, 445, 443, 439},
    {444, 446, 438, 440}, {433, 445, 437, 435}, {438, 446, 434, 436},
    {431, 447, 445, 433}, {446, 448, 432, 434}, {429, 447, 431, 449},
    {432, 448, 430, 450}, {413, 429, 449, 415}, {450, 430, 414, 416},
    {311, 447, 429, 381}, {430, 448, 312, 382}, {311, 441, 445, 447},
    {446, 442, 312, 448}, {415, 449, 451, 475}, {452, 450, 416, 476},
    {449, 431, 461, 451}, {462, 432, 450, 452}, {431, 433, 459, 461},
    {460, 434, 432, 462}, {433, 435, 457, 459}, {458, 436, 434, 460},
    {435, 437, 455, 457}, {456, 438, 436, 458}, {437, 439, 453, 455},
    {454, 440, 438, 456}, {439, 417, 473, 453}, {474, 418, 440, 454},
    {427, 415, 475, 463}, {476, 416, 428, 464}, {425, 427, 463, 465},
    {464, 428, 426, 466}, {423, 425, 465, 467}, {466, 426, 424, 468},
    {421, 423, 467, 469}, {468, 424, 422, 470}, {419, 421, 469, 471},
    {470, 422, 420, 472}, {417, 419, 471, 473}, {472, 420, 418, 474},
    {457, 455, 479, 477}, {480, 456, 458, 478}, {477, 479, 481, 483},
    {482, 480, 478, 484}, {483, 481, 487, 485}, {488, 482, 484, 486},
    {485, 487, 489, 491}, {490, 488, 486, 492}, {463, 475, 485, 491},
    {486, 476, 464, 492}, {451, 483, 485, 475}, {486, 484, 452, 476},
    {451, 461, 477, 483}, {478, 462, 452, 484}, {457, 477, 461, 459},
    {462, 478, 458, 460}, {453, 473, 479, 455}, {480, 474, 454, 456},
    {471, 481, 479, 473}, {480, 482, 472, 474}, {469, 487, 481, 471},
    {482, 488, 470, 472}, {467, 489, 487, 469}, {488, 490, 468, 470},
    {465, 491, 489, 467}, {490, 492, 466, 468}, {391, 389, 503, 501},
    {504, 390, 392, 502}, {393, 391, 501, 499}, {502, 392, 394, 500},
    {395, 393, 499, 497}, {500, 394, 396, 498}, {397, 395, 497, 495},
    {498, 396, 398, 496}, {399, 397, 495, 493}, {496, 398, 400, 494},
    {387, 399, 493, 505}, {494, 400, 388, 506}, {493, 501, 503, 505},
    {504, 502, 494, 506}, {493, 495, 499, 501}, {500, 496, 494, 502},
    {313, 381, 387, 505}, {388, 382, 314, 506}, {313, 505, 503, 321},
    {504, 506, 314, 322}, {319, 321, 503, 389}, {504, 322, 320, 390},
    // ttriangles
    {60, 64, 48, 48}, {49, 65, 61, 61}, {62, 64, 60, 60}, {61, 65, 63, 63},
    {60, 58, 62, 62}, {63, 59, 61, 61}, {60, 56, 58, 58}, {59, 57, 61, 61},
    {60, 54, 56, 56}, {57, 55, 61, 61}, {60, 52, 54, 54}, {55, 53, 61, 61},
    {60, 50, 52, 52}, {53, 51, 61, 61}, {60, 48, 50, 50}, {51, 49, 61, 61},
    {224, 228, 226, 226}, {227, 229, 225, 255}, {72, 283, 73, 73},
    {73, 284, 72, 72}, {341, 347, 383, 383}, {384, 348, 342, 342},
    {299, 345, 343, 343}, {344, 346, 300, 300}, {323, 379, 351, 351},
    {352, 380, 324, 324}, {441, 443, 445, 445}, {446, 444, 442, 442},
    {463, 491, 465, 465}, {466, 492, 464, 464}, {495, 497, 499, 499},
    {500, 498, 496, 496}};

}  // namespace yocto