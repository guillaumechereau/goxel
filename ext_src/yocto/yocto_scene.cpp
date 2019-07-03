//
// Implementation for Yocto/Scene.
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

#include "yocto_scene.h"
#include "yocto_random.h"
#include "yocto_shape.h"

#include <assert.h>
#include <unordered_map>

#include "ext/filesystem.hpp"
namespace fs = ghc::filesystem;

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Computes a shape bounding box.
bbox3f compute_bounds(const yocto_shape& shape) {
  auto bbox = invalidb3f;
  for (auto p : shape.positions) bbox = merge(bbox, p);
  return bbox;
}

// Updates the scene and scene's instances bounding boxes
bbox3f compute_bounds(const yocto_scene& scene) {
  auto shape_bbox = vector<bbox3f>(scene.shapes.size());
  for (auto shape_id = 0; shape_id < scene.shapes.size(); shape_id++)
    shape_bbox[shape_id] = compute_bounds(scene.shapes[shape_id]);
  auto bbox = invalidb3f;
  for (auto& instance : scene.instances) {
    bbox = merge(
        bbox, transform_bbox(instance.frame, shape_bbox[instance.shape]));
  }
  return bbox;
}

// Compute vertex normals
vector<vec3f> compute_normals(const yocto_shape& shape) {
  if (!shape.points.empty()) {
    return vector<vec3f>{
        shape.positions.size(),
    };
  } else if (!shape.lines.empty()) {
    return compute_tangents(shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    return compute_normals(shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    return compute_normals(shape.quads, shape.positions);
  } else if (!shape.quadspos.empty()) {
    return compute_normals(shape.quadspos, shape.positions);
  } else {
    throw std::runtime_error("unknown element type");
  }
}
void compute_normals(const yocto_shape& shape, vector<vec3f>& normals) {
  normals.assign(shape.positions.size(), {0, 0, 1});
  if (!shape.points.empty()) {
  } else if (!shape.lines.empty()) {
    compute_tangents(normals, shape.lines, shape.positions);
  } else if (!shape.triangles.empty()) {
    compute_normals(normals, shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    compute_normals(normals, shape.quads, shape.positions);
  } else if (!shape.quadspos.empty()) {
    compute_normals(normals, shape.quadspos, shape.positions);
  } else {
    throw std::runtime_error("unknown element type");
  }
}

// Apply subdivision and displacement rules.
void subdivide_shape(yocto_shape& shape, int subdivisions, bool catmullclark,
    bool update_normals) {
  if (!subdivisions) return;
  if (!shape.points.empty()) {
    throw std::runtime_error("point subdivision not supported");
  } else if (!shape.lines.empty()) {
    subdivide_lines(shape.lines, shape.positions, shape.normals,
        shape.texcoords, shape.colors, shape.radius, shape.lines,
        shape.positions, shape.normals, shape.texcoords, shape.colors,
        shape.radius, subdivisions);
  } else if (!shape.triangles.empty()) {
    subdivide_triangles(shape.triangles, shape.positions, shape.normals,
        shape.texcoords, shape.colors, shape.radius, shape.triangles,
        shape.positions, shape.normals, shape.texcoords, shape.colors,
        shape.radius, subdivisions);
  } else if (!shape.quads.empty() && !catmullclark) {
    subdivide_quads(shape.quads, shape.positions, shape.normals,
        shape.texcoords, shape.colors, shape.radius, shape.quads,
        shape.positions, shape.normals, shape.texcoords, shape.colors,
        shape.radius, subdivisions);
  } else if (!shape.quads.empty() && catmullclark) {
    subdivide_catmullclark(shape.quads, shape.positions, shape.normals,
        shape.texcoords, shape.colors, shape.radius, shape.quads,
        shape.positions, shape.normals, shape.texcoords, shape.colors,
        shape.radius, subdivisions);
  } else if (!shape.quadspos.empty() && !catmullclark) {
    subdivide_quads(shape.quadspos, shape.positions, shape.quadspos,
        shape.positions, subdivisions);
    subdivide_quads(shape.quadsnorm, shape.normals, shape.quadsnorm,
        shape.normals, subdivisions);
    subdivide_quads(shape.quadstexcoord, shape.texcoords, shape.quadstexcoord,
        shape.texcoords, subdivisions);
  } else if (!shape.quadspos.empty() && catmullclark) {
    subdivide_catmullclark(shape.quadspos, shape.positions, shape.quadspos,
        shape.positions, subdivisions);
    subdivide_catmullclark(shape.quadstexcoord, shape.texcoords,
        shape.quadstexcoord, shape.texcoords, subdivisions, true);
  } else {
    throw std::runtime_error("empty shape");
  }

  if (update_normals) {
    if (!shape.quadspos.empty()) {
      shape.quadsnorm = shape.quadspos;
    }
    compute_normals(shape, shape.normals);
  }
}
// Apply displacement to a shape
void displace_shape(yocto_shape& shape, const yocto_texture& displacement,
    float scale, bool update_normals) {
  if (shape.texcoords.empty()) {
    throw std::runtime_error("missing texture coordinates");
    return;
  }

  // simple case
  if (shape.quadspos.empty()) {
    auto normals = shape.normals;
    if (shape.normals.empty()) compute_normals(shape, normals);
    for (auto vid = 0; vid < shape.positions.size(); vid++) {
      auto disp = mean(
          xyz(eval_texture(displacement, shape.texcoords[vid], true)));
      if (!is_hdr_filename(displacement.uri)) disp -= 0.5f;
      shape.positions[vid] += normals[vid] * scale * disp;
    }
    if (update_normals || !shape.normals.empty()) {
      compute_normals(shape, shape.normals);
    }
  } else {
    // facevarying case
    auto offset = vector<float>(shape.positions.size(), 0);
    auto count  = vector<int>(shape.positions.size(), 0);
    for (auto fid = 0; fid < shape.quadspos.size(); fid++) {
      auto qpos = shape.quadspos[fid];
      auto qtxt = shape.quadstexcoord[fid];
      for (auto i = 0; i < 4; i++) {
        auto disp = mean(
            xyz(eval_texture(displacement, shape.texcoords[qtxt[i]], true)));
        if (!is_hdr_filename(displacement.uri)) disp -= 0.5f;
        offset[qpos[i]] += scale * disp;
        count[qpos[i]] += 1;
      }
    }
    auto normals = vector<vec3f>{shape.positions.size()};
    compute_normals(normals, shape.quadspos, shape.positions);
    for (auto vid = 0; vid < shape.positions.size(); vid++) {
      shape.positions[vid] += normals[vid] * offset[vid] / count[vid];
    }
    if (update_normals || !shape.normals.empty()) {
      shape.quadsnorm = shape.quadspos;
      compute_normals(shape, shape.normals);
    }
  }
}

void tesselate_subdiv(yocto_scene& scene, yocto_subdiv& subdiv) {
  auto& shape         = scene.shapes[subdiv.shape];
  shape.positions     = subdiv.positions;
  shape.normals       = subdiv.normals;
  shape.texcoords     = subdiv.texcoords;
  shape.colors        = subdiv.colors;
  shape.radius        = subdiv.radius;
  shape.points        = subdiv.points;
  shape.lines         = subdiv.lines;
  shape.triangles     = subdiv.triangles;
  shape.quads         = subdiv.quads;
  shape.quadspos      = subdiv.quadspos;
  shape.quadsnorm     = subdiv.quadsnorm;
  shape.quadstexcoord = subdiv.quadstexcoord;
  shape.lines         = subdiv.lines;
  if (subdiv.subdivisions) {
    subdivide_shape(
        shape, subdiv.subdivisions, subdiv.catmullclark, subdiv.smooth);
  }
  if (subdiv.displacement_tex >= 0) {
    displace_shape(shape, scene.textures[subdiv.displacement_tex],
        subdiv.displacement, subdiv.smooth);
  }
}

// Updates tesselation.
void tesselate_subdivs(yocto_scene& scene) {
  for (auto& subdiv : scene.subdivs) tesselate_subdiv(scene, subdiv);
}

// Update animation transforms
void update_transforms(yocto_scene& scene, yocto_animation& animation,
    float time, const string& anim_group) {
  if (anim_group != "" && anim_group != animation.group) return;

  if (!animation.translations.empty()) {
    auto value = vec3f{0, 0, 0};
    switch (animation.interpolation) {
      case yocto_animation::interpolation_type::step:
        value = keyframe_step(animation.times, animation.translations, time);
        break;
      case yocto_animation::interpolation_type::linear:
        value = keyframe_linear(animation.times, animation.translations, time);
        break;
      case yocto_animation::interpolation_type::bezier:
        value = keyframe_bezier(animation.times, animation.translations, time);
        break;
      default: throw std::runtime_error("should not have been here");
    }
    for (auto target : animation.targets)
      scene.nodes[target].translation = value;
  }
  if (!animation.rotations.empty()) {
    auto value = vec4f{0, 0, 0, 1};
    switch (animation.interpolation) {
      case yocto_animation::interpolation_type::step:
        value = keyframe_step(animation.times, animation.rotations, time);
        break;
      case yocto_animation::interpolation_type::linear:
        value = keyframe_linear(animation.times, animation.rotations, time);
        break;
      case yocto_animation::interpolation_type::bezier:
        value = keyframe_bezier(animation.times, animation.rotations, time);
        break;
    }
    for (auto target : animation.targets) scene.nodes[target].rotation = value;
  }
  if (!animation.scales.empty()) {
    auto value = vec3f{1, 1, 1};
    switch (animation.interpolation) {
      case yocto_animation::interpolation_type::step:
        value = keyframe_step(animation.times, animation.scales, time);
        break;
      case yocto_animation::interpolation_type::linear:
        value = keyframe_linear(animation.times, animation.scales, time);
        break;
      case yocto_animation::interpolation_type::bezier:
        value = keyframe_bezier(animation.times, animation.scales, time);
        break;
    }
    for (auto target : animation.targets) scene.nodes[target].scale = value;
  }
}

// Update node transforms
void update_transforms(yocto_scene& scene, yocto_scene_node& node,
    const frame3f& parent = identity3x4f) {
  auto frame = parent * node.local * translation_frame(node.translation) *
               rotation_frame(node.rotation) * scaling_frame(node.scale);
  if (node.instance >= 0) scene.instances[node.instance].frame = frame;
  if (node.camera >= 0) scene.cameras[node.camera].frame = frame;
  if (node.environment >= 0) scene.environments[node.environment].frame = frame;
  for (auto child : node.children)
    update_transforms(scene, scene.nodes[child], frame);
}

// Update node transforms
void update_transforms(
    yocto_scene& scene, float time, const string& anim_group) {
  for (auto& agr : scene.animations)
    update_transforms(scene, agr, time, anim_group);
  for (auto& node : scene.nodes) node.children.clear();
  for (auto node_id = 0; node_id < scene.nodes.size(); node_id++) {
    auto& node = scene.nodes[node_id];
    if (node.parent >= 0) scene.nodes[node.parent].children.push_back(node_id);
  }
  for (auto& node : scene.nodes)
    if (node.parent < 0) update_transforms(scene, node);
}

// Compute animation range
vec2f compute_animation_range(
    const yocto_scene& scene, const string& anim_group) {
  if (scene.animations.empty()) return zero2f;
  auto range = vec2f{+flt_max, -flt_max};
  for (auto& animation : scene.animations) {
    if (anim_group != "" && animation.group != anim_group) continue;
    range.x = min(range.x, animation.times.front());
    range.y = max(range.y, animation.times.back());
  }
  if (range.y < range.x) return zero2f;
  return range;
}

// Generate a distribution for sampling a shape uniformly based on area/length.
void sample_shape_cdf(const yocto_shape& shape, vector<float>& cdf) {
  cdf.clear();
  if (!shape.triangles.empty()) {
    sample_triangles_cdf(cdf, shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    sample_quads_cdf(cdf, shape.quads, shape.positions);
  } else if (!shape.lines.empty()) {
    sample_lines_cdf(cdf, shape.lines, shape.positions);
  } else if (!shape.points.empty()) {
    sample_points_cdf(cdf, shape.points.size());
  } else if (!shape.quadspos.empty()) {
    sample_quads_cdf(cdf, shape.quadspos, shape.positions);
  } else {
    throw std::runtime_error("empty shape");
  }
}
vector<float> sample_shape_cdf(const yocto_shape& shape) {
  if (!shape.triangles.empty()) {
    return sample_triangles_cdf(shape.triangles, shape.positions);
  } else if (!shape.quads.empty()) {
    return sample_quads_cdf(shape.quads, shape.positions);
  } else if (!shape.lines.empty()) {
    return sample_lines_cdf(shape.lines, shape.positions);
  } else if (!shape.points.empty()) {
    return sample_points_cdf(shape.points.size());
  } else if (!shape.quadspos.empty()) {
    return sample_quads_cdf(shape.quadspos, shape.positions);
  } else {
    throw std::runtime_error("empty shape");
    return {};
  }
}

// Sample a shape based on a distribution.
pair<int, vec2f> sample_shape(const yocto_shape& shape,
    const vector<float>& cdf, float re, const vec2f& ruv) {
  // TODO: implement sampling without cdf
  if (cdf.empty()) return {};
  if (!shape.triangles.empty()) {
    return sample_triangles(cdf, re, ruv);
  } else if (!shape.quads.empty()) {
    return sample_quads(cdf, re, ruv);
  } else if (!shape.lines.empty()) {
    return {sample_lines(cdf, re, ruv.x).first, ruv};
  } else if (!shape.points.empty()) {
    return {sample_points(cdf, re), ruv};
  } else if (!shape.quadspos.empty()) {
    return sample_quads(cdf, re, ruv);
  } else {
    return {0, zero2f};
  }
}

float sample_shape_pdf(const yocto_shape& shape, const vector<float>& cdf,
    int element, const vec2f& uv) {
  // prob triangle * area triangle = area triangle mesh
  return 1 / cdf.back();
}

// Update environment CDF for sampling.
vector<float> sample_environment_cdf(
    const yocto_scene& scene, const yocto_environment& environment) {
  if (environment.emission_tex < 0) return {};
  auto& texture    = scene.textures[environment.emission_tex];
  auto  size       = texture_size(texture);
  auto  texels_cdf = vector<float>(size.x * size.y);
  if (size != zero2i) {
    for (auto i = 0; i < texels_cdf.size(); i++) {
      auto ij       = vec2i{i % size.x, i / size.x};
      auto th       = (ij.y + 0.5f) * pif / size.y;
      auto value    = lookup_texture(texture, ij.x, ij.y);
      texels_cdf[i] = max(xyz(value)) * sin(th);
      if (i) texels_cdf[i] += texels_cdf[i - 1];
    }
  } else {
    throw std::runtime_error("empty texture");
  }
  return texels_cdf;
}
void sample_environment_cdf(const yocto_scene& scene,
    const yocto_environment& environment, vector<float>& texels_cdf) {
  if (environment.emission_tex < 0) {
    texels_cdf.clear();
    return;
  }
  auto& texture = scene.textures[environment.emission_tex];
  auto  size    = texture_size(texture);
  texels_cdf.resize(size.x * size.y);
  if (size != zero2i) {
    for (auto i = 0; i < texels_cdf.size(); i++) {
      auto ij       = vec2i{i % size.x, i / size.x};
      auto th       = (ij.y + 0.5f) * pif / size.y;
      auto value    = lookup_texture(texture, ij.x, ij.y);
      texels_cdf[i] = max(xyz(value)) * sin(th);
      if (i) texels_cdf[i] += texels_cdf[i - 1];
    }
  } else {
    throw std::runtime_error("empty texture");
  }
}

// Sample an environment based on texels
vec3f sample_environment(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    float re, const vec2f& ruv) {
  if (!texels_cdf.empty() && environment.emission_tex >= 0) {
    auto& texture = scene.textures[environment.emission_tex];
    auto  idx     = sample_discrete(texels_cdf, re);
    auto  size    = texture_size(texture);
    auto  u       = (idx % size.x + 0.5f) / size.x;
    auto  v       = (idx / size.x + 0.5f) / size.y;
    return eval_direction(environment, {u, v});
  } else {
    return sample_sphere(ruv);
  }
}

// Sample an environment based on texels
float sample_environment_pdf(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    const vec3f& direction) {
  if (!texels_cdf.empty() && environment.emission_tex >= 0) {
    auto& texture  = scene.textures[environment.emission_tex];
    auto  size     = texture_size(texture);
    auto  texcoord = eval_texcoord(environment, direction);
    auto  i        = (int)(texcoord.x * size.x);
    auto  j        = (int)(texcoord.y * size.y);
    auto  idx      = j * size.x + i;
    auto  prob     = sample_discrete_pdf(texels_cdf, idx) / texels_cdf.back();
    auto  angle    = (2 * pif / size.x) * (pif / size.y) *
                 sin(pif * (j + 0.5f) / size.y);
    return prob / angle;
  } else {
    return sample_sphere_pdf(direction);
  }
}

bvh_scene make_bvh(const yocto_scene& scene, const bvh_params& params) {
  auto sbvhs = vector<bvh_shape>{scene.shapes.size()};
  for (auto idx = 0; idx < scene.shapes.size(); idx++) {
    auto& shape = scene.shapes[idx];
    auto& sbvh  = sbvhs[idx];
#if YOCTO_EMBREE
    // call Embree if needed
    if (params.use_embree) {
      if (params.embree_compact &&
          shape.positions.size() == shape.positions.capacity()) {
        ((yocto_shape&)shape).positions.reserve(shape.positions.size() + 1);
      }
    }
#endif
    if (!shape.points.empty()) {
      sbvh = make_points_bvh(shape.points, shape.positions, shape.radius);
    } else if (!shape.lines.empty()) {
      sbvh = make_lines_bvh(shape.lines, shape.positions, shape.radius);
    } else if (!shape.triangles.empty()) {
      sbvh = make_triangles_bvh(shape.triangles, shape.positions, shape.radius);
    } else if (!shape.quads.empty()) {
      sbvh = make_quads_bvh(shape.quads, shape.positions, shape.radius);
    } else if (!shape.quadspos.empty()) {
      sbvh = make_quadspos_bvh(shape.quadspos, shape.positions, shape.radius);
    } else {
      throw std::runtime_error("empty shape");
    }
  }

  auto bvh = make_instances_bvh(
      {&scene.instances[0].frame, (int)scene.instances.size(),
          sizeof(scene.instances[0])},
      sbvhs);
  build_bvh(bvh, params);
  return bvh;
}

void make_bvh(
    bvh_scene& bvh, const yocto_scene& scene, const bvh_params& params) {
  auto sbvhs = vector<bvh_shape>{scene.shapes.size()};
  for (auto idx = 0; idx < scene.shapes.size(); idx++) {
    auto& shape = scene.shapes[idx];
    auto& sbvh  = sbvhs[idx];
#if YOCTO_EMBREE
    // call Embree if needed
    if (params.use_embree) {
      if (params.embree_compact &&
          shape.positions.size() == shape.positions.capacity()) {
        ((yocto_shape&)shape).positions.reserve(shape.positions.size() + 1);
      }
    }
#endif
    if (!shape.points.empty()) {
      sbvh = make_points_bvh(shape.points, shape.positions, shape.radius);
    } else if (!shape.lines.empty()) {
      sbvh = make_lines_bvh(shape.lines, shape.positions, shape.radius);
    } else if (!shape.triangles.empty()) {
      sbvh = make_triangles_bvh(shape.triangles, shape.positions, shape.radius);
    } else if (!shape.quads.empty()) {
      sbvh = make_quads_bvh(shape.quads, shape.positions, shape.radius);
    } else if (!shape.quadspos.empty()) {
      sbvh = make_quadspos_bvh(shape.quadspos, shape.positions, shape.radius);
    } else {
      throw std::runtime_error("empty shape");
    }
  }

  bvh = {{&scene.instances[0].frame, (int)scene.instances.size(),
             sizeof(scene.instances[0])},
      sbvhs};

  build_bvh(bvh, params);
}

void refit_bvh(bvh_scene& bvh, const yocto_scene& scene,
    const vector<int>& updated_shapes, const bvh_params& params) {
  for (auto idx : updated_shapes) {
    auto& shape = scene.shapes[idx];
    auto& sbvh  = bvh.shapes[idx];
#if YOCTO_EMBREE
    // call Embree if needed
    if (params.use_embree) {
      if (params.embree_compact &&
          shape.positions.size() == shape.positions.capacity()) {
        ((yocto_shape&)shape).positions.reserve(shape.positions.size() + 1);
      }
    }
#endif
    sbvh.points    = shape.points;
    sbvh.lines     = shape.lines;
    sbvh.triangles = shape.triangles;
    sbvh.quads     = shape.quads;
    sbvh.quadspos  = shape.quadspos;
    sbvh.positions = shape.positions;
    sbvh.radius    = shape.radius;
  }
  if (!scene.instances.empty()) {
    bvh.instances = {&scene.instances[0].frame, (int)scene.instances.size(),
        sizeof(scene.instances[0])};
  } else {
    bvh.instances = {};
  }

  refit_bvh(bvh, updated_shapes, params);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR EVAL AND SAMPLING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Shape element normal.
vec3f eval_element_normal(const yocto_shape& shape, int element) {
  auto norm = zero3f;
  if (!shape.triangles.empty()) {
    auto t = shape.triangles[element];
    norm   = triangle_normal(
        shape.positions[t.x], shape.positions[t.y], shape.positions[t.z]);
  } else if (!shape.quads.empty()) {
    auto q = shape.quads[element];
    norm   = quad_normal(shape.positions[q.x], shape.positions[q.y],
        shape.positions[q.z], shape.positions[q.w]);
  } else if (!shape.lines.empty()) {
    auto l = shape.lines[element];
    norm   = line_tangent(shape.positions[l.x], shape.positions[l.y]);
  } else if (!shape.quadspos.empty()) {
    auto q = shape.quadspos[element];
    norm   = quad_normal(shape.positions[q.x], shape.positions[q.y],
        shape.positions[q.z], shape.positions[q.w]);
  } else {
    throw std::runtime_error("empty shape");
    norm = {0, 0, 1};
  }
  return norm;
}

// Shape element normal.
pair<vec3f, vec3f> eval_element_tangents(
    const yocto_shape& shape, int element, const vec2f& uv) {
  if (!shape.triangles.empty()) {
    auto t = shape.triangles[element];
    if (shape.texcoords.empty()) {
      return triangle_tangents_fromuv(shape.positions[t.x],
          shape.positions[t.y], shape.positions[t.z], {0, 0}, {1, 0}, {0, 1});
    } else {
      return triangle_tangents_fromuv(shape.positions[t.x],
          shape.positions[t.y], shape.positions[t.z], shape.texcoords[t.x],
          shape.texcoords[t.y], shape.texcoords[t.z]);
    }
  } else if (!shape.quads.empty()) {
    auto q = shape.quads[element];
    if (shape.texcoords.empty()) {
      return quad_tangents_fromuv(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w], {0, 0}, {1, 0}, {0, 1},
          {1, 1}, uv);
    } else {
      return quad_tangents_fromuv(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w], shape.texcoords[q.x],
          shape.texcoords[q.y], shape.texcoords[q.z], shape.texcoords[q.w], uv);
    }
  } else if (!shape.quadspos.empty()) {
    auto q = shape.quadspos[element];
    if (shape.texcoords.empty()) {
      return quad_tangents_fromuv(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w], {0, 0}, {1, 0}, {0, 1},
          {1, 1}, uv);
    } else {
      auto qt = shape.quadstexcoord[element];
      return quad_tangents_fromuv(shape.positions[q.x], shape.positions[q.y],
          shape.positions[q.z], shape.positions[q.w], shape.texcoords[qt.x],
          shape.texcoords[qt.y], shape.texcoords[qt.z], shape.texcoords[qt.w],
          uv);
    }
  } else {
    return {zero3f, zero3f};
  }
}
pair<mat3f, bool> eval_element_tangent_basis(
    const yocto_shape& shape, int element, const vec2f& uv) {
  auto z        = eval_element_normal(shape, element);
  auto tangents = eval_element_tangents(shape, element, uv);
  auto x        = orthonormalize(tangents.first, z);
  auto y        = normalize(cross(z, x));
  return {{x, y, z}, dot(y, tangents.second) < 0};
}

// Shape value interpolated using barycentric coordinates
template <typename T>
T eval_shape_elem(const yocto_shape& shape,
    const vector<vec4i>& facevarying_quads, const vector<T>& vals, int element,
    const vec2f& uv) {
  if (vals.empty()) return {};
  if (!shape.triangles.empty()) {
    auto t = shape.triangles[element];
    return interpolate_triangle(vals[t.x], vals[t.y], vals[t.z], uv);
  } else if (!shape.quads.empty()) {
    auto q = shape.quads[element];
    if (q.w == q.z)
      return interpolate_triangle(vals[q.x], vals[q.y], vals[q.z], uv);
    return interpolate_quad(vals[q.x], vals[q.y], vals[q.z], vals[q.w], uv);
  } else if (!shape.lines.empty()) {
    auto l = shape.lines[element];
    return interpolate_line(vals[l.x], vals[l.y], uv.x);
  } else if (!shape.points.empty()) {
    return vals[shape.points[element]];
  } else if (!shape.quadspos.empty()) {
    auto q = facevarying_quads[element];
    if (q.w == q.z)
      return interpolate_triangle(vals[q.x], vals[q.y], vals[q.z], uv);
    return interpolate_quad(vals[q.x], vals[q.y], vals[q.z], vals[q.w], uv);
  } else {
    return {};
  }
}

// Shape values interpolated using barycentric coordinates
vec3f eval_position(const yocto_shape& shape, int element, const vec2f& uv) {
  return eval_shape_elem(shape, shape.quadspos, shape.positions, element, uv);
}
vec3f eval_normal(const yocto_shape& shape, int element, const vec2f& uv) {
  if (shape.normals.empty()) return eval_element_normal(shape, element);
  return normalize(
      eval_shape_elem(shape, shape.quadsnorm, shape.normals, element, uv));
}
vec2f eval_texcoord(const yocto_shape& shape, int element, const vec2f& uv) {
  if (shape.texcoords.empty()) return uv;
  return eval_shape_elem(
      shape, shape.quadstexcoord, shape.texcoords, element, uv);
}
vec4f eval_color(const yocto_shape& shape, int element, const vec2f& uv) {
  if (shape.colors.empty()) return {1, 1, 1, 1};
  return eval_shape_elem(shape, {}, shape.colors, element, uv);
}
float eval_radius(const yocto_shape& shape, int element, const vec2f& uv) {
  if (shape.radius.empty()) return 0.001f;
  return eval_shape_elem(shape, {}, shape.radius, element, uv);
}
vec4f eval_tangent_space(
    const yocto_shape& shape, int element, const vec2f& uv) {
  if (shape.tangents.empty()) return zero4f;
  return eval_shape_elem(shape, {}, shape.tangents, element, uv);
}
pair<mat3f, bool> eval_tangent_basis(
    const yocto_shape& shape, int element, const vec2f& uv) {
  auto z = eval_normal(shape, element, uv);
  if (shape.tangents.empty()) {
    auto tangents = eval_element_tangents(shape, element, uv);
    auto x        = orthonormalize(tangents.first, z);
    auto y        = normalize(cross(z, x));
    return {{x, y, z}, dot(y, tangents.second) < 0};
  } else {
    auto tangsp = eval_shape_elem(shape, {}, shape.tangents, element, uv);
    auto x      = orthonormalize(xyz(tangsp), z);
    auto y      = normalize(cross(z, x));
    return {{x, y, z}, tangsp.w < 0};
  }
}

// Instance values interpolated using barycentric coordinates.
vec3f eval_position(const yocto_scene& scene, const yocto_instance& instance,
    int element, const vec2f& uv) {
  return transform_point(
      instance.frame, eval_position(scene.shapes[instance.shape], element, uv));
}
vec3f eval_normal(const yocto_scene& scene, const yocto_instance& instance,
    int element, const vec2f& uv, bool non_rigid_frame) {
  auto normal = eval_normal(scene.shapes[instance.shape], element, uv);
  return transform_normal(instance.frame, normal, non_rigid_frame);
}
vec3f eval_shading_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element, const vec2f& uv,
    const vec3f& direction, bool non_rigid_frame) {
  auto& shape    = scene.shapes[instance.shape];
  auto& material = scene.materials[instance.material];
  if (!shape.points.empty()) {
    return -direction;
  } else if (!shape.lines.empty()) {
    auto normal = eval_normal(scene, instance, element, uv, non_rigid_frame);
    return orthonormalize(-direction, normal);
  } else if (material.normal_tex < 0) {
    return eval_normal(scene, instance, element, uv, non_rigid_frame);
  } else {
    auto& normal_tex = scene.textures[material.normal_tex];
    auto  normalmap  = -1 + 2 * xyz(eval_texture(normal_tex,
                                  eval_texcoord(shape, element, uv), true));
    auto  basis      = eval_tangent_basis(shape, element, uv);
    normalmap.y *= basis.second ? 1 : -1;  // flip vertical axis
    auto normal = normalize(basis.first * normalmap);
    return transform_normal(instance.frame, normal, non_rigid_frame);
  }
}
// Instance element values.
vec3f eval_element_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element, bool non_rigid_frame) {
  auto normal = eval_element_normal(scene.shapes[instance.shape], element);
  return transform_normal(instance.frame, normal, non_rigid_frame);
}
// Instance material
material_point eval_material(const yocto_scene& scene,
    const yocto_instance& instance, int element, const vec2f& uv) {
  auto& shape     = scene.shapes[instance.shape];
  auto& material  = scene.materials[instance.material];
  auto  texcoords = eval_texcoord(shape, element, uv);
  auto  color     = eval_color(shape, element, uv);
  return eval_material(scene, material, texcoords, color);
}

// Environment texture coordinates from the direction.
vec2f eval_texcoord(
    const yocto_environment& environment, const vec3f& direction) {
  auto wl = transform_direction(inverse(environment.frame), direction);
  auto environment_uv = vec2f{
      atan2(wl.z, wl.x) / (2 * pif), acos(clamp(wl.y, -1.0f, 1.0f)) / pif};
  if (environment_uv.x < 0) environment_uv.x += 1;
  return environment_uv;
}
// Evaluate the environment direction.
vec3f eval_direction(
    const yocto_environment& environment, const vec2f& environment_uv) {
  return transform_direction(environment.frame,
      {cos(environment_uv.x * 2 * pif) * sin(environment_uv.y * pif),
          cos(environment_uv.y * pif),
          sin(environment_uv.x * 2 * pif) * sin(environment_uv.y * pif)});
}
// Evaluate the environment color.
vec3f eval_environment(const yocto_scene& scene,
    const yocto_environment& environment, const vec3f& direction) {
  auto emission = environment.emission;
  if (environment.emission_tex >= 0) {
    auto& emission_tex = scene.textures[environment.emission_tex];
    emission *= xyz(
        eval_texture(emission_tex, eval_texcoord(environment, direction)));
  }
  return emission;
}
// Evaluate all environment color.
vec3f eval_environment(const yocto_scene& scene, const vec3f& direction) {
  auto emission = zero3f;
  for (auto& environment : scene.environments)
    emission += eval_environment(scene, environment, direction);
  return emission;
}

// Check texture size
vec2i texture_size(const yocto_texture& texture) {
  if (!texture.hdr.empty()) {
    return texture.hdr.size();
  } else if (!texture.ldr.empty()) {
    return texture.ldr.size();
  } else {
    return zero2i;
  }
}

// Lookup a texture value
vec4f lookup_texture(
    const yocto_texture& texture, int i, int j, bool ldr_as_linear) {
  if (!texture.hdr.empty()) {
    return texture.hdr[{i, j}];
  } else if (!texture.ldr.empty() && !ldr_as_linear) {
    return srgb_to_rgb(byte_to_float(texture.ldr[{i, j}]));
  } else if (!texture.ldr.empty() && ldr_as_linear) {
    return byte_to_float(texture.ldr[{i, j}]);
  } else {
    return zero4f;
  }
}

// Evaluate a texture
vec4f eval_texture(const yocto_texture& texture, const vec2f& texcoord,
    bool ldr_as_linear, bool no_interpolation, bool clamp_to_edge) {
  if (texture.hdr.empty() && texture.ldr.empty()) return {1, 1, 1, 1};

  // get image width/height
  auto size  = texture_size(texture);
  auto width = size.x, height = size.y;

  // get coordinates normalized for tiling
  auto s = 0.0f, t = 0.0f;
  if (clamp_to_edge) {
    s = clamp(texcoord.x, 0.0f, 1.0f) * width;
    t = clamp(texcoord.y, 0.0f, 1.0f) * height;
  } else {
    s = fmod(texcoord.x, 1.0f) * width;
    if (s < 0) s += width;
    t = fmod(texcoord.y, 1.0f) * height;
    if (t < 0) t += height;
  }

  // get image coordinates and residuals
  auto i = clamp((int)s, 0, width - 1), j = clamp((int)t, 0, height - 1);
  auto ii = (i + 1) % width, jj = (j + 1) % height;
  auto u = s - i, v = t - j;

  if (no_interpolation) return lookup_texture(texture, i, j, ldr_as_linear);

  // handle interpolation
  return lookup_texture(texture, i, j, ldr_as_linear) * (1 - u) * (1 - v) +
         lookup_texture(texture, i, jj, ldr_as_linear) * (1 - u) * v +
         lookup_texture(texture, ii, j, ldr_as_linear) * u * (1 - v) +
         lookup_texture(texture, ii, jj, ldr_as_linear) * u * v;
}

// Lookup a texture value
float lookup_voltexture(
    const yocto_voltexture& texture, int i, int j, int k, bool ldr_as_linear) {
  if (!texture.vol.empty()) {
    return texture.vol[{i, j, k}];
  } else {
    return 0;
  }
}

// Evaluate a volume texture
float eval_voltexture(const yocto_voltexture& texture, const vec3f& texcoord,
    bool ldr_as_linear, bool no_interpolation, bool clamp_to_edge) {
  if (texture.vol.empty()) return 1;

  // get image width/height
  auto width  = texture.vol.size().x;
  auto height = texture.vol.size().y;
  auto depth  = texture.vol.size().z;

  // get coordinates normalized for tiling
  auto s = clamp((texcoord.x + 1.0f) * 0.5f, 0.0f, 1.0f) * width;
  auto t = clamp((texcoord.y + 1.0f) * 0.5f, 0.0f, 1.0f) * height;
  auto r = clamp((texcoord.z + 1.0f) * 0.5f, 0.0f, 1.0f) * depth;

  // get image coordinates and residuals
  auto i  = clamp((int)s, 0, width - 1);
  auto j  = clamp((int)t, 0, height - 1);
  auto k  = clamp((int)r, 0, depth - 1);
  auto ii = (i + 1) % width, jj = (j + 1) % height, kk = (k + 1) % depth;
  auto u = s - i, v = t - j, w = r - k;

  // nearest-neighbor interpolation
  if (no_interpolation) {
    i = u < 0.5 ? i : min(i + 1, width - 1);
    j = v < 0.5 ? j : min(j + 1, height - 1);
    k = w < 0.5 ? k : min(k + 1, depth - 1);
    return lookup_voltexture(texture, i, j, k, ldr_as_linear);
  }

  // trilinear interpolation
  return lookup_voltexture(texture, i, j, k, ldr_as_linear) * (1 - u) *
             (1 - v) * (1 - w) +
         lookup_voltexture(texture, ii, j, k, ldr_as_linear) * u * (1 - v) *
             (1 - w) +
         lookup_voltexture(texture, i, jj, k, ldr_as_linear) * (1 - u) * v *
             (1 - w) +
         lookup_voltexture(texture, i, j, kk, ldr_as_linear) * (1 - u) *
             (1 - v) * w +
         lookup_voltexture(texture, i, jj, kk, ldr_as_linear) * (1 - u) * v *
             w +
         lookup_voltexture(texture, ii, j, kk, ldr_as_linear) * u * (1 - v) *
             w +
         lookup_voltexture(texture, ii, jj, k, ldr_as_linear) * u * v *
             (1 - w) +
         lookup_voltexture(texture, ii, jj, kk, ldr_as_linear) * u * v * w;
}

// Set and evaluate camera parameters. Setters take zeros as default values.
vec2f camera_fov(const yocto_camera& camera) {
  assert(!camera.orthographic);
  return {2 * atan(camera.film.x / (2 * camera.lens)),
      2 * atan(camera.film.y / (2 * camera.lens))};
}
float camera_yfov(const yocto_camera& camera) {
  assert(!camera.orthographic);
  return 2 * atan(camera.film.y / (2 * camera.lens));
}
float camera_aspect(const yocto_camera& camera) {
  return camera.film.x / camera.film.y;
}
vec2i camera_resolution(const yocto_camera& camera, int resolution) {
  if (camera.film.x > camera.film.y) {
    return {resolution, (int)round(resolution * camera.film.y / camera.film.x)};
  } else {
    return {(int)round(resolution * camera.film.x / camera.film.y), resolution};
  }
}
void set_yperspective(
    yocto_camera& camera, float fov, float aspect, float focus, float film) {
  camera.orthographic = false;
  camera.film         = {film, film / aspect};
  camera.focus        = focus;
  auto distance       = camera.film.y / (2 * tan(fov / 2));
  if (focus < flt_max) {
    camera.lens = camera.focus * distance / (camera.focus + distance);
  } else {
    camera.lens = distance;
  }
}

// add missing camera
void set_view(
    yocto_camera& camera, const bbox3f& bbox, const vec3f& view_direction) {
  camera.orthographic = false;
  auto center         = (bbox.max + bbox.min) / 2;
  auto bbox_radius    = length(bbox.max - bbox.min) / 2;
  auto camera_dir     = (view_direction == zero3f) ? camera.frame.o - center
                                               : view_direction;
  if (camera_dir == zero3f) camera_dir = {0, 0, 1};
  auto fov = min(camera_fov(camera));
  if (fov == 0) fov = 45 * pif / 180;
  auto camera_dist = bbox_radius / sin(fov / 2);
  auto from        = camera_dir * (camera_dist * 1) + center;
  auto to          = center;
  auto up          = vec3f{0, 1, 0};
  camera.frame     = lookat_frame(from, to, up);
  camera.focus     = length(from - to);
  camera.aperture  = 0;
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f eval_perspective_camera(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv) {
  auto distance = camera.lens;
  if (camera.focus < flt_max) {
    distance = camera.lens * camera.focus / (camera.focus - camera.lens);
  }
  if (camera.aperture) {
    auto e = vec3f{(lens_uv.x - 0.5f) * camera.aperture,
        (lens_uv.y - 0.5f) * camera.aperture, 0};
    auto q = vec3f{camera.film.x * (0.5f - image_uv.x),
        camera.film.y * (image_uv.y - 0.5f), distance};
    // distance of the image of the point
    auto distance1 = camera.lens * distance / (distance - camera.lens);
    auto q1        = -q * distance1 / distance;
    auto d         = normalize(q1 - e);
    // auto q1 = - normalize(q) * camera.focus / normalize(q).z;
    auto ray = ray3f{
        transform_point(camera.frame, e), transform_direction(camera.frame, d)};
    return ray;
  } else {
    auto e   = zero3f;
    auto q   = vec3f{camera.film.x * (0.5f - image_uv.x),
        camera.film.y * (image_uv.y - 0.5f), distance};
    auto q1  = -q;
    auto d   = normalize(q1 - e);
    auto ray = ray3f{
        transform_point(camera.frame, e), transform_direction(camera.frame, d)};
    return ray;
  }
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f eval_orthographic_camera(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv) {
  if (camera.aperture) {
    auto scale = 1 / camera.lens;
    auto q     = vec3f{camera.film.x * (0.5f - image_uv.x) * scale,
        camera.film.y * (image_uv.y - 0.5f) * scale, scale};
    auto q1    = vec3f{-q.x, -q.y, -camera.focus};
    auto e = vec3f{-q.x, -q.y, 0} + vec3f{(lens_uv.x - 0.5f) * camera.aperture,
                                        (lens_uv.y - 0.5f) * camera.aperture,
                                        0};
    auto d = normalize(q1 - e);
    auto ray = ray3f{
        transform_point(camera.frame, e), transform_direction(camera.frame, d)};
    return ray;
  } else {
    auto scale = 1 / camera.lens;
    auto q     = vec3f{camera.film.x * (0.5f - image_uv.x) * scale,
        camera.film.y * (image_uv.y - 0.5f) * scale, scale};
    auto q1    = -q;
    auto e     = vec3f{-q.x, -q.y, 0};
    auto d     = normalize(q1 - e);
    auto ray   = ray3f{
        transform_point(camera.frame, e), transform_direction(camera.frame, d)};
    return ray;
  }
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f eval_camera(
    const yocto_camera& camera, const vec2f& uv, const vec2f& luv) {
  if (camera.orthographic)
    return eval_orthographic_camera(camera, uv, luv);
  else
    return eval_perspective_camera(camera, uv, luv);
}

// Generates a ray from a camera.
ray3f eval_camera(const yocto_camera& camera, const vec2i& image_ij,
    const vec2i& image_size, const vec2f& pixel_uv, const vec2f& lens_uv) {
  auto image_uv = vec2f{(image_ij.x + pixel_uv.x) / image_size.x,
      (image_ij.y + pixel_uv.y) / image_size.y};
  return eval_camera(camera, image_uv, lens_uv);
}

// Generates a ray from a camera.
ray3f eval_camera(const yocto_camera& camera, int idx, const vec2i& image_size,
    const vec2f& pixel_uv, const vec2f& lens_uv) {
  auto image_ij = vec2i{idx % image_size.x, idx / image_size.x};
  auto image_uv = vec2f{(image_ij.x + pixel_uv.x) / image_size.x,
      (image_ij.y + pixel_uv.y) / image_size.y};
  return eval_camera(camera, image_uv, lens_uv);
}

// Evaluates the microfacet_brdf at a location.
material_point eval_material(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texcoord,
    const vec4f& shape_color) {
  auto point = material_point{};
  // factors
  point.emission       = material.emission * xyz(shape_color);
  point.diffuse        = material.diffuse * xyz(shape_color);
  point.specular       = material.specular;
  auto metallic        = material.metallic;
  point.roughness      = material.roughness;
  point.coat           = material.coat;
  point.transmission   = material.transmission;
  auto voltransmission = material.voltransmission;
  auto volmeanfreepath = material.volmeanfreepath;
  point.volemission    = material.volemission;
  point.volscatter     = material.volscatter;
  point.volanisotropy  = material.volanisotropy;
  auto volscale        = material.volscale;
  point.opacity        = material.opacity * shape_color.w;
  point.thin           = material.thin;

  // textures
  if (material.emission_tex >= 0) {
    auto& emission_tex = scene.textures[material.emission_tex];
    point.emission *= xyz(eval_texture(emission_tex, texcoord));
  }
  if (material.diffuse_tex >= 0) {
    auto& diffuse_tex = scene.textures[material.diffuse_tex];
    auto  base_txt    = eval_texture(diffuse_tex, texcoord);
    point.diffuse *= xyz(base_txt);
    point.opacity *= base_txt.w;
  }
  if (material.metallic_tex >= 0) {
    auto& metallic_tex = scene.textures[material.metallic_tex];
    auto  metallic_txt = eval_texture(metallic_tex, texcoord);
    metallic *= metallic_txt.z;
    if (material.gltf_textures) {
      point.roughness *= metallic_txt.x;
    }
  }
  if (material.specular_tex >= 0) {
    auto& specular_tex = scene.textures[material.specular_tex];
    auto  specular_txt = eval_texture(specular_tex, texcoord);
    point.specular *= xyz(specular_txt);
    if (material.gltf_textures) {
      auto glossiness = 1 - point.roughness;
      glossiness *= specular_txt.w;
      point.roughness = 1 - glossiness;
    }
  }
  if (material.roughness_tex >= 0) {
    auto& roughness_tex = scene.textures[material.roughness_tex];
    point.roughness *= eval_texture(roughness_tex, texcoord).x;
  }
  if (material.transmission_tex >= 0) {
    auto& transmission_tex = scene.textures[material.transmission_tex];
    point.transmission *= xyz(eval_texture(transmission_tex, texcoord));
  }
  if (material.subsurface_tex >= 0) {
    auto& subsurface_tex = scene.textures[material.subsurface_tex];
    point.volscatter *= xyz(eval_texture(subsurface_tex, texcoord));
  }
  if (material.opacity_tex >= 0) {
    auto& opacity_tex = scene.textures[material.opacity_tex];
    point.opacity *= mean(xyz(eval_texture(opacity_tex, texcoord)));
  }
  if (material.coat_tex >= 0) {
    auto& coat_tex = scene.textures[material.coat_tex];
    point.coat *= xyz(eval_texture(coat_tex, texcoord));
  }
  if (metallic) {
    point.specular = point.specular * (1 - metallic) + metallic * point.diffuse;
    point.diffuse  = metallic * point.diffuse * (1 - metallic);
  }
  if (point.diffuse != zero3f || point.roughness) {
    point.roughness = point.roughness * point.roughness;
    point.roughness = clamp(point.roughness, 0.03f * 0.03f, 1.0f);
  }
  if (point.opacity > 0.999f) point.opacity = 1;
  if (voltransmission != zero3f || volmeanfreepath != zero3f) {
    if (voltransmission != zero3f) {
      point.voldensity = -log(clamp(voltransmission, 0.0001f, 1.0f)) / volscale;
    } else {
      point.voldensity = 1 / (volmeanfreepath * volscale);
    }
  } else {
    point.voldensity = zero3f;
  }
  return point;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

void merge_scene(yocto_scene& scene, const yocto_scene& merge) {
  auto offset_cameras      = scene.cameras.size();
  auto offset_textures     = scene.textures.size();
  auto offset_voltextures  = scene.voltextures.size();
  auto offset_materials    = scene.materials.size();
  auto offset_shapes       = scene.shapes.size();
  auto offset_subdivs      = scene.subdivs.size();
  auto offset_instances    = scene.instances.size();
  auto offset_environments = scene.environments.size();
  auto offset_nodes        = scene.nodes.size();
  auto offset_animations   = scene.animations.size();
  scene.cameras.insert(
      scene.cameras.end(), merge.cameras.begin(), merge.cameras.end());
  scene.textures.insert(
      scene.textures.end(), merge.textures.begin(), merge.textures.end());
  scene.voltextures.insert(scene.voltextures.end(), merge.voltextures.begin(),
      merge.voltextures.end());
  scene.materials.insert(
      scene.materials.end(), merge.materials.begin(), merge.materials.end());
  scene.shapes.insert(
      scene.shapes.end(), merge.shapes.begin(), merge.shapes.end());
  scene.subdivs.insert(
      scene.subdivs.end(), merge.subdivs.begin(), merge.subdivs.end());
  scene.instances.insert(
      scene.instances.end(), merge.instances.begin(), merge.instances.end());
  scene.environments.insert(scene.environments.end(),
      merge.environments.begin(), merge.environments.end());
  scene.nodes.insert(scene.nodes.end(), merge.nodes.begin(), merge.nodes.end());
  scene.animations.insert(
      scene.animations.end(), merge.animations.begin(), merge.animations.end());
  for (auto material_id = offset_materials;
       material_id < scene.materials.size(); material_id++) {
    auto& material = scene.materials[material_id];
    if (material.emission_tex >= 0) material.emission_tex += offset_textures;
    if (material.diffuse_tex >= 0) material.diffuse_tex += offset_textures;
    if (material.metallic_tex >= 0) material.metallic_tex += offset_textures;
    if (material.specular_tex >= 0) material.specular_tex += offset_textures;
    if (material.transmission_tex >= 0)
      material.transmission_tex += offset_textures;
    if (material.roughness_tex >= 0) material.roughness_tex += offset_textures;
    if (material.normal_tex >= 0) material.normal_tex += offset_textures;
    if (material.voldensity_tex >= 0)
      material.voldensity_tex += offset_voltextures;
  }
  for (auto subdiv_id = offset_subdivs; subdiv_id < scene.subdivs.size();
       subdiv_id++) {
    auto& subdiv = scene.subdivs[subdiv_id];
    if (subdiv.shape >= 0) subdiv.shape += offset_shapes;
    if (subdiv.displacement_tex >= 0)
      subdiv.displacement_tex += offset_textures;
  }
  for (auto instance_id = offset_instances;
       instance_id < scene.instances.size(); instance_id++) {
    auto& instance = scene.instances[instance_id];
    if (instance.shape >= 0) instance.shape += offset_shapes;
    if (instance.material >= 0) instance.material += offset_materials;
  }
  for (auto environment_id = offset_environments;
       environment_id < scene.environments.size(); environment_id++) {
    auto& environment = scene.environments[environment_id];
    if (environment.emission_tex >= 0)
      environment.emission_tex += offset_textures;
  }
  for (auto node_id = offset_nodes; node_id < scene.nodes.size(); node_id++) {
    auto& node = scene.nodes[node_id];
    if (node.parent >= 0) node.parent += offset_nodes;
    if (node.camera >= 0) node.camera += offset_cameras;
    if (node.instance >= 0) node.instance += offset_instances;
    if (node.environment >= 0) node.environment += offset_environments;
  }
  for (auto animation_id = offset_animations;
       animation_id < scene.animations.size(); animation_id++) {
    auto& animation = scene.animations[animation_id];
    for (auto& target : animation.targets)
      if (target >= 0) target += offset_nodes;
  }
}

string format_stats(
    const yocto_scene& scene, const string& prefix, bool verbose) {
  auto accumulate = [](const auto& values, const auto& func) -> size_t {
    auto sum = (size_t)0;
    for (auto& value : values) sum += func(value);
    return sum;
  };
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

  auto bbox = compute_bounds(scene);

  auto stats = ""s;
  stats += prefix + "cameras:      " + format(scene.cameras.size()) + "\n";
  stats += prefix + "shapes:       " + format(scene.shapes.size()) + "\n";
  stats += prefix + "subdivs:      " + format(scene.subdivs.size()) + "\n";
  stats += prefix + "instances:    " + format(scene.instances.size()) + "\n";
  stats += prefix + "environments: " + format(scene.environments.size()) + "\n";
  stats += prefix + "textures:     " + format(scene.textures.size()) + "\n";
  stats += prefix + "voltextures:  " + format(scene.voltextures.size()) + "\n";
  stats += prefix + "materials:    " + format(scene.materials.size()) + "\n";
  stats += prefix + "nodes:        " + format(scene.nodes.size()) + "\n";
  stats += prefix + "animations:   " + format(scene.animations.size()) + "\n";
  stats += prefix + "points:       " +
           format(accumulate(
               scene.shapes, [](auto& shape) { return shape.points.size(); })) +
           "\n";
  stats += prefix + "lines:        " +
           format(accumulate(
               scene.shapes, [](auto& shape) { return shape.lines.size(); })) +
           "\n";
  stats += prefix + "triangles:    " +
           format(accumulate(scene.shapes,
               [](auto& shape) { return shape.triangles.size(); })) +
           "\n";
  stats += prefix + "quads:        " +
           format(accumulate(
               scene.shapes, [](auto& shape) { return shape.quads.size(); })) +
           "\n";
  stats += prefix + "fvquads:      " +
           format(accumulate(scene.shapes,
               [](auto& shape) { return shape.quadspos.size(); })) +
           "\n";
  stats += prefix + "texels4b:     " +
           format(accumulate(scene.textures,
               [](auto& texture) {
                 return (size_t)texture.ldr.size().x *
                        (size_t)texture.ldr.size().x;
               })) +
           "\n";
  stats += prefix + "texels4f:     " +
           format(accumulate(scene.textures,
               [](auto& texture) {
                 return (size_t)texture.hdr.size().x *
                        (size_t)texture.hdr.size().y;
               })) +
           "\n";
  stats += prefix + "volxels1f:    " +
           format(accumulate(scene.voltextures,
               [](auto& texture) {
                 return (size_t)texture.vol.size().x *
                        (size_t)texture.vol.size().y *
                        (size_t)texture.vol.size().z;
               })) +
           "\n";
  stats += prefix + "center:       " + format3(center(bbox)) + "\n";
  stats += prefix + "size:         " + format3(size(bbox)) + "\n";

  return stats;
}

// Add missing names and resolve duplicated names.
void normalize_uris(yocto_scene& scene) {
  auto normalize = [](string& name, const string& base, const string& ext,
                       int num) {
    for (auto& c : name) {
      if (c == ':' || c == ' ') c = '_';
    }
    if (name.empty()) name = base + "_" + std::to_string(num);
    if (fs::path(name).parent_path().empty()) name = base + "s/" + name;
    if (fs::path(name).extension().empty()) name = name + "." + ext;
  };
  for (auto id = 0; id < scene.cameras.size(); id++)
    normalize(scene.cameras[id].uri, "camera", "yaml", id);
  for (auto id = 0; id < scene.textures.size(); id++)
    normalize(scene.textures[id].uri, "texture", "png", id);
  for (auto id = 0; id < scene.voltextures.size(); id++)
    normalize(scene.voltextures[id].uri, "volume", "yvol", id);
  for (auto id = 0; id < scene.materials.size(); id++)
    normalize(scene.materials[id].uri, "material", "yaml", id);
  for (auto id = 0; id < scene.shapes.size(); id++)
    normalize(scene.shapes[id].uri, "shape", "ply", id);
  for (auto id = 0; id < scene.instances.size(); id++)
    normalize(scene.instances[id].uri, "instance", "yaml", id);
  for (auto id = 0; id < scene.animations.size(); id++)
    normalize(scene.animations[id].uri, "animation", "yaml", id);
  for (auto id = 0; id < scene.nodes.size(); id++)
    normalize(scene.nodes[id].uri, "node", "yaml", id);
}
void rename_instances(yocto_scene& scene) {
  auto shape_names = vector<string>(scene.shapes.size());
  for (auto sid = 0; sid < scene.shapes.size(); sid++) {
    shape_names[sid] = fs::path(scene.shapes[sid].uri).stem();
  }
  auto shape_count = vector<vec2i>(scene.shapes.size(), vec2i{0, 0});
  for (auto& instance : scene.instances) shape_count[instance.shape].y += 1;
  for (auto& instance : scene.instances) {
    if (shape_count[instance.shape].y == 1) {
      instance.uri = "instances/" + shape_names[instance.shape] + ".yaml";
    } else {
      auto num = std::to_string(shape_count[instance.shape].x++);
      while (num.size() < (int)ceil(log10(shape_count[instance.shape].y)))
        num = '0' + num;
      instance.uri = "instances/" + shape_names[instance.shape] + "-" + num +
                     ".yaml";
    }
  }
}

// Normalized a scaled color in a material
void normalize_scaled_color(float& scale, vec3f& color) {
  auto scaled = scale * color;
  if (max(scaled) == 0) {
    scale = 0;
    color = {1, 1, 1};
  } else {
    scale = max(scaled);
    color = scaled / max(scaled);
  }
}

// Add missing tangent space if needed.
void add_tangent_spaces(yocto_scene& scene) {
  for (auto& instance : scene.instances) {
    auto& material = scene.materials[instance.material];
    if (material.normal_tex < 0) continue;
    auto& shape = scene.shapes[instance.shape];
    if (!shape.tangents.empty() || shape.texcoords.empty()) continue;
    if (!shape.triangles.empty()) {
      if (shape.normals.empty()) {
        shape.normals.resize(shape.positions.size());
        compute_normals(shape.normals, shape.triangles, shape.positions);
      }
      shape.tangents.resize(shape.positions.size());
      compute_tangent_spaces(shape.tangents, shape.triangles, shape.positions,
          shape.normals, shape.texcoords);
    } else {
      throw std::runtime_error("type not supported");
    }
  }
}

// Add missing materials.
void add_materials(yocto_scene& scene) {
  auto material_id = -1;
  for (auto& instance : scene.instances) {
    if (instance.material >= 0) continue;
    if (material_id < 0) {
      auto material    = yocto_material{};
      material.uri     = "materails/default.yaml";
      material.diffuse = {0.2f, 0.2f, 0.2f};
      scene.materials.push_back(material);
      material_id = (int)scene.materials.size() - 1;
    }
    instance.material = material_id;
  }
}

// Add missing radius.
void add_radius(yocto_scene& scene, float radius) {
  for (auto& shape : scene.shapes) {
    if (shape.points.empty() && shape.lines.empty()) continue;
    if (!shape.radius.empty()) continue;
    shape.radius.assign(shape.positions.size(), radius);
  }
}

// Add missing cameras.
void add_cameras(yocto_scene& scene) {
  if (scene.cameras.empty()) {
    auto camera = yocto_camera{};
    camera.uri  = "cameras/default.yaml";
    set_view(camera, compute_bounds(scene), {0, 0, 1});
    scene.cameras.push_back(camera);
  }
}

// Add a sky environment
void add_sky(yocto_scene& scene, float sun_angle) {
  auto texture = yocto_texture{};
  texture.uri  = "textures/sky.hdr";
  make_sunsky(texture.hdr, {1024, 512}, sun_angle);
  scene.textures.push_back(texture);
  auto environment         = yocto_environment{};
  environment.uri          = "environments/default.yaml";
  environment.emission     = {1, 1, 1};
  environment.emission_tex = (int)scene.textures.size() - 1;
  scene.environments.push_back(environment);
}

// Reduce memory usage
void trim_memory(yocto_scene& scene) {
  for (auto& shape : scene.shapes) {
    shape.points.shrink_to_fit();
    shape.lines.shrink_to_fit();
    shape.triangles.shrink_to_fit();
    shape.quads.shrink_to_fit();
    shape.quadspos.shrink_to_fit();
    shape.quadsnorm.shrink_to_fit();
    shape.quadstexcoord.shrink_to_fit();
    shape.positions.shrink_to_fit();
    shape.normals.shrink_to_fit();
    shape.texcoords.shrink_to_fit();
    shape.colors.shrink_to_fit();
    shape.radius.shrink_to_fit();
    shape.tangents.shrink_to_fit();
  }
  for (auto& texture : scene.textures) {
    texture.ldr.shrink_to_fit();
    texture.hdr.shrink_to_fit();
  }
  scene.cameras.shrink_to_fit();
  scene.shapes.shrink_to_fit();
  scene.instances.shrink_to_fit();
  scene.materials.shrink_to_fit();
  scene.textures.shrink_to_fit();
  scene.environments.shrink_to_fit();
  scene.voltextures.shrink_to_fit();
  scene.nodes.shrink_to_fit();
  scene.animations.shrink_to_fit();
}

// Checks for validity of the scene.
vector<string> validate_scene(const yocto_scene& scene, bool notextures) {
  auto errs        = vector<string>();
  auto check_names = [&errs](const auto& vals, const string& base) {
    auto used = unordered_map<string, int>();
    used.reserve(vals.size());
    for (auto& value : vals) used[value.uri] += 1;
    for (auto& [name, used] : used) {
      if (name == "") {
        errs.push_back("empty " + base + " name");
      } else if (used > 1) {
        errs.push_back("duplicated " + base + " name " + name);
      }
    }
  };
  auto check_empty_textures = [&errs](const vector<yocto_texture>& vals) {
    for (auto& value : vals) {
      if (value.hdr.empty() && value.ldr.empty()) {
        errs.push_back("empty texture " + value.uri);
      }
    }
  };

  check_names(scene.cameras, "camera");
  check_names(scene.shapes, "shape");
  check_names(scene.textures, "texture");
  check_names(scene.voltextures, "voltexture");
  check_names(scene.materials, "material");
  check_names(scene.instances, "instance");
  check_names(scene.environments, "environment");
  check_names(scene.nodes, "node");
  check_names(scene.animations, "animation");
  if (!notextures) check_empty_textures(scene.textures);

  return errs;
}

// Logs validations errors
void print_validation(const yocto_scene& scene, bool notextures) {
  for (auto err : validate_scene(scene, notextures))
    printf("%s [validation]\n", err.c_str());
}

}  // namespace yocto
