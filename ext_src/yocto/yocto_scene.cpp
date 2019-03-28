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
#include "yocto_utils.h"

#include <cassert>
#include <unordered_map>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {
using std::unordered_map;
}

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Computes a shape bounding box.
bbox3f compute_shape_bounds(const yocto_shape& shape) {
    auto bbox = invalid_bbox3f;
    for (auto p : shape.positions) bbox += p;
    return bbox;
}

// Updates the scene and scene's instances bounding boxes
bbox3f compute_scene_bounds(const yocto_scene& scene) {
    auto shape_bbox = vector<bbox3f>(scene.shapes.size());
    for (auto shape_id = 0; shape_id < scene.shapes.size(); shape_id++)
        shape_bbox[shape_id] = compute_shape_bounds(scene.shapes[shape_id]);
    auto bbox = invalid_bbox3f;
    for (auto& instance : scene.instances) {
        bbox += transform_bbox(instance.frame, shape_bbox[instance.shape]);
    }
    return bbox;
}

// Compute vertex normals
void compute_shape_normals(const yocto_shape& shape, vector<vec3f>& normals) {
    normals.assign(shape.positions.size(), {0, 0, 1});
    if (!shape.points.empty()) {
    } else if (!shape.lines.empty()) {
        compute_vertex_tangents(normals, shape.lines, shape.positions);
    } else if (!shape.triangles.empty()) {
        compute_vertex_normals(normals, shape.triangles, shape.positions);
    } else if (!shape.quads.empty()) {
        compute_vertex_normals(normals, shape.quads, shape.positions);
    } else if (!shape.quads_positions.empty()) {
        compute_vertex_normals(normals, shape.quads_positions, shape.positions);
    } else {
        throw std::runtime_error("unknown element type");
    }
}

// Apply subdivision and displacement rules.
void subdivide_shape(yocto_shape& shape) {
    if (!shape.subdivision_level) return;
    if (!shape.points.empty()) {
        throw runtime_error("point subdivision not supported");
    } else if (!shape.lines.empty()) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_lines(shape.lines, shape.positions, shape.normals,
                shape.texturecoords, shape.colors, shape.radius);
        }
    } else if (!shape.triangles.empty()) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_triangles(shape.triangles, shape.positions, shape.normals,
                shape.texturecoords, shape.colors, shape.radius);
        }
    } else if (!shape.quads.empty() && !shape.catmull_clark) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_quads(shape.quads, shape.positions, shape.normals,
                shape.texturecoords, shape.colors, shape.radius);
        }
    } else if (!shape.quads.empty() && shape.catmull_clark) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_catmullclark(shape.quads, shape.positions, shape.normals,
                shape.texturecoords, shape.colors, shape.radius);
        }
    } else if (!shape.quads_positions.empty() && !shape.catmull_clark) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_quads(shape.quads_positions, shape.positions);
            subdivide_quads(shape.quads_normals, shape.normals);
            subdivide_quads(shape.quads_texturecoords, shape.texturecoords);
        }
    } else if (!shape.quads_positions.empty() && shape.catmull_clark) {
        for (auto l = 0; l < shape.subdivision_level; l++) {
            subdivide_catmullclark(shape.quads_positions, shape.positions);
            subdivide_catmullclark(
                shape.quads_texturecoords, shape.texturecoords, true);
        }
    } else {
        throw runtime_error("empty shape");
    }

    if (shape.compute_normals) {
        if (!shape.quads_positions.empty()) {
            shape.quads_normals = shape.quads_positions;
        }
        compute_shape_normals(shape, shape.normals);
    }

    shape.subdivision_level = 0;
}
// Apply displacement to a shape
void displace_shape(yocto_shape& shape, const yocto_texture& displacement) {
    if (shape.texturecoords.empty()) {
        throw runtime_error("missing texture coordinates");
        return;
    }

    // simple case
    if (shape.quads_positions.empty()) {
        auto normals = shape.normals;
        if (shape.normals.empty()) compute_shape_normals(shape, normals);
        for (auto vid = 0; vid < shape.positions.size(); vid++) {
            shape.positions[vid] += normals[vid] * displacement.height_scale *
                                    mean(xyz(evaluate_texture(displacement,
                                        shape.texturecoords[vid])));
        }
        if (shape.compute_normals || !shape.normals.empty()) {
            compute_shape_normals(shape, shape.normals);
        }
    } else {
        // facevarying case
        auto offset = vector<float>(shape.positions.size(), 0);
        auto count  = vector<int>(shape.positions.size(), 0);
        for (auto fid = 0; fid < shape.quads_positions.size(); fid++) {
            auto qpos = shape.quads_positions[fid];
            auto qtxt = shape.quads_texturecoords[fid];
            for (auto i = 0; i < 4; i++) {
                offset[qpos[i]] += displacement.height_scale *
                                   mean(xyz(evaluate_texture(displacement,
                                       shape.texturecoords[qtxt[i]])));
                count[qpos[i]] += 1;
            }
        }
        auto normals = vector<vec3f>{shape.positions.size()};
        compute_vertex_normals(normals, shape.quads_positions, shape.positions);
        for (auto vid = 0; vid < shape.positions.size(); vid++) {
            shape.positions[vid] += normals[vid] * offset[vid] / count[vid];
        }
        if (shape.compute_normals || !shape.normals.empty()) {
            shape.quads_normals = shape.quads_positions;
            compute_shape_normals(shape, shape.normals);
        }
    }
}

// Updates tesselation.
void tesselate_shapes(yocto_scene& scene) {
    for (auto& shape : scene.shapes) {
        auto& material = scene.materials[shape.material];
        if (!shape.subdivision_level && material.displacement_texture < 0)
            continue;
        if (shape.subdivision_level) {
            subdivide_shape(shape);
        }
        if (material.displacement_texture >= 0) {
            displace_shape(
                shape, scene.textures[material.displacement_texture]);
        }
    }
}

// Update animation transforms
void update_transforms(yocto_scene& scene, yocto_animation& animation,
    float time, const string& anim_group) {
    if (anim_group != "" && anim_group != animation.animation_group) return;

    if (!animation.translation_keyframes.empty()) {
        auto value = vec3f{0, 0, 0};
        switch (animation.interpolation_type) {
            case yocto_interpolation_type::step:
                value = evaluate_keyframed_step(animation.keyframes_times,
                    animation.translation_keyframes, time);
                break;
            case yocto_interpolation_type::linear:
                value = evaluate_keyframed_linear(animation.keyframes_times,
                    animation.translation_keyframes, time);
                break;
            case yocto_interpolation_type::bezier:
                value = evaluate_keyframed_bezier(animation.keyframes_times,
                    animation.translation_keyframes, time);
                break;
            default: throw runtime_error("should not have been here");
        }
        for (auto target : animation.node_targets)
            scene.nodes[target].translation = value;
    }
    if (!animation.rotation_keyframes.empty()) {
        auto value = vec4f{0, 0, 0, 1};
        switch (animation.interpolation_type) {
            case yocto_interpolation_type::step:
                value = evaluate_keyframed_step(animation.keyframes_times,
                    animation.rotation_keyframes, time);
                break;
            case yocto_interpolation_type::linear:
                value = evaluate_keyframed_linear(animation.keyframes_times,
                    animation.rotation_keyframes, time);
                break;
            case yocto_interpolation_type::bezier:
                value = evaluate_keyframed_bezier(animation.keyframes_times,
                    animation.rotation_keyframes, time);
                break;
        }
        for (auto target : animation.node_targets)
            scene.nodes[target].rotation = value;
    }
    if (!animation.scale_keyframes.empty()) {
        auto value = vec3f{1, 1, 1};
        switch (animation.interpolation_type) {
            case yocto_interpolation_type::step:
                value = evaluate_keyframed_step(
                    animation.keyframes_times, animation.scale_keyframes, time);
                break;
            case yocto_interpolation_type::linear:
                value = evaluate_keyframed_linear(
                    animation.keyframes_times, animation.scale_keyframes, time);
                break;
            case yocto_interpolation_type::bezier:
                value = evaluate_keyframed_bezier(
                    animation.keyframes_times, animation.scale_keyframes, time);
                break;
        }
        for (auto target : animation.node_targets)
            scene.nodes[target].scale = value;
    }
}

// Update node transforms
void update_transforms(yocto_scene& scene, yocto_scene_node& node,
    const frame3f& parent = identity_frame3f) {
    auto frame =
        parent * node.local * make_translation_frame(node.translation) *
        make_rotation_frame(node.rotation) * make_scaling_frame(node.scale);
    if (node.instance >= 0) scene.instances[node.instance].frame = frame;
    if (node.camera >= 0) scene.cameras[node.camera].frame = frame;
    if (node.environment >= 0)
        scene.environments[node.environment].frame = frame;
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
        if (node.parent >= 0)
            scene.nodes[node.parent].children.push_back(node_id);
    }
    for (auto& node : scene.nodes)
        if (node.parent < 0) update_transforms(scene, node);
}

// Compute animation range
vec2f compute_animation_range(
    const yocto_scene& scene, const string& anim_group) {
    if (scene.animations.empty()) return zero2f;
    auto range = vec2f{+float_max, -float_max};
    for (auto& animation : scene.animations) {
        if (anim_group != "" && animation.animation_group != anim_group)
            continue;
        range.x = min(range.x, animation.keyframes_times.front());
        range.y = max(range.y, animation.keyframes_times.back());
    }
    if (range.y < range.x) return zero2f;
    return range;
}

// Generate a distribution for sampling a shape uniformly based on area/length.
void compute_shape_elements_cdf(const yocto_shape& shape, vector<float>& cdf) {
    cdf.clear();
    if (!shape.triangles.empty()) {
        sample_triangles_element_cdf(cdf, shape.triangles, shape.positions);
    } else if (!shape.quads.empty()) {
        sample_quads_element_cdf(cdf, shape.quads, shape.positions);
    } else if (!shape.lines.empty()) {
        sample_lines_element_cdf(cdf, shape.lines, shape.positions);
    } else if (!shape.points.empty()) {
        sample_points_element_cdf(cdf, shape.points.size());
    } else if (!shape.quads_positions.empty()) {
        sample_quads_element_cdf(cdf, shape.quads_positions, shape.positions);
    } else {
        throw runtime_error("empty shape");
    }
}

// Sample a shape based on a distribution.
pair<int, vec2f> sample_shape_element(const yocto_shape& shape,
    const vector<float>& elements_cdf, float re, const vec2f& ruv) {
    // TODO: implement sampling without cdf
    if (elements_cdf.empty()) return {};
    if (!shape.triangles.empty()) {
        return sample_triangles_element(elements_cdf, re, ruv);
    } else if (!shape.quads.empty()) {
        return sample_quads_element(elements_cdf, re, ruv);
    } else if (!shape.lines.empty()) {
        return {sample_lines_element(elements_cdf, re, ruv.x).first, ruv};
    } else if (!shape.points.empty()) {
        return {sample_points_element(elements_cdf, re), ruv};
    } else if (!shape.quads_positions.empty()) {
        return sample_quads_element(elements_cdf, re, ruv);
    } else {
        return {0, zero2f};
    }
}

float sample_shape_element_pdf(const yocto_shape& shape,
    const vector<float>& elements_cdf, int element_id,
    const vec2f& element_uv) {
    // prob triangle * area triangle = area triangle mesh
    return 1 / elements_cdf.back();
}

// Update environment CDF for sampling.
void compute_environment_texels_cdf(const yocto_scene& scene,
    const yocto_environment& environment, vector<float>& texels_cdf) {
    if (environment.emission_texture < 0) {
        texels_cdf.clear();
        return;
    }
    auto& texture = scene.textures[environment.emission_texture];
    auto  size    = evaluate_texture_size(texture);
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
        throw runtime_error("empty texture");
    }
}

// Sample an environment based on texels
vec3f sample_environment_direction(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    float re, const vec2f& ruv) {
    if (!texels_cdf.empty() && environment.emission_texture >= 0) {
        auto& texture = scene.textures[environment.emission_texture];
        auto  idx     = sample_discrete_distribution(texels_cdf, re);
        auto  size    = evaluate_texture_size(texture);
        auto  u       = (idx % size.x + 0.5f) / size.x;
        auto  v       = (idx / size.x + 0.5f) / size.y;
        return evaluate_environment_direction(environment, {u, v});
    } else {
        return sample_sphere_direction(ruv);
    }
}

// Sample an environment based on texels
float sample_environment_direction_pdf(const yocto_scene& scene,
    const yocto_environment& environment, const vector<float>& texels_cdf,
    const vec3f& direction) {
    if (!texels_cdf.empty() && environment.emission_texture >= 0) {
        auto& texture  = scene.textures[environment.emission_texture];
        auto  size     = evaluate_texture_size(texture);
        auto  texcoord = evaluate_environment_texturecoord(
            environment, direction);
        auto i    = (int)(texcoord.x * size.x);
        auto j    = (int)(texcoord.y * size.y);
        auto idx  = j * size.x + i;
        auto prob = sample_discrete_distribution_pdf(texels_cdf, idx) /
                    texels_cdf.back();
        auto angle = (2 * pif / size.x) * (pif / size.y) *
                     sin(pif * (j + 0.5f) / size.y);
        return prob / angle;
    } else {
        return sample_sphere_direction_pdf(direction);
    }
}

// Build a scene BVH
void build_scene_bvh(const yocto_scene& scene, bvh_scene& bvh,
    const build_bvh_options& options) {
    // shapes
    auto shape_bvhs = vector<bvh_shape>();
    for (auto& shape : scene.shapes) {
        // make bvh
        auto shape_bvh = bvh_shape{};
        if (!shape.points.empty()) {
            init_shape_bvh(
                shape_bvh, shape.points, shape.positions, shape.radius);
        } else if (!shape.lines.empty()) {
            init_shape_bvh(
                shape_bvh, shape.lines, shape.positions, shape.radius);
        } else if (!shape.triangles.empty()) {
            init_shape_bvh(shape_bvh, shape.triangles, shape.positions);
        } else if (!shape.quads.empty()) {
            init_shape_bvh(shape_bvh, shape.quads, shape.positions);
        } else if (!shape.quads_positions.empty()) {
            init_shape_bvh(shape_bvh, shape.quads_positions, shape.positions);
        } else {
            throw runtime_error("empty shape");
        }
        shape_bvhs.push_back(shape_bvh);
    }

    // instances
    auto bvh_instances = vector<bvh_instance>{};
    for (auto& instance : scene.instances) {
        bvh_instances.push_back(
            {instance.frame, inverse(instance.frame, false), instance.shape});
    }

    // build bvh
    bvh = {};
    init_scene_bvh(bvh, bvh_instances, shape_bvhs);
    build_scene_bvh(bvh, options);
}

// Refits a scene BVH
void refit_scene_bvh(const yocto_scene& scene, bvh_scene& bvh,
    const vector<int>& updated_instances, const vector<int>& updated_shapes) {
    for (auto shape_id : updated_shapes)
        update_shape_bvh(get_shape_bvh(bvh, shape_id),
            scene.shapes[shape_id].positions, scene.shapes[shape_id].radius);

    auto bvh_instances = vector<bvh_instance>{};
    for (auto& instance : scene.instances) {
        bvh_instances.push_back(
            {instance.frame, inverse(instance.frame), instance.shape});
    }
    update_scene_bvh(bvh, bvh_instances);

    refit_scene_bvh(bvh, updated_instances, updated_shapes);
}

// Add missing names and resolve duplicated names.
void add_missing_names(yocto_scene& scene) {
    auto fix_names = [](auto& vals, const string& base) {
        auto nmap = unordered_map<string, int>();
        for (auto& value : vals) {
            if (value.name == "") value.name = base;
            if (nmap.find(value.name) == nmap.end()) {
                nmap[value.name] = 0;
            } else {
                nmap[value.name] += 1;
                value.name = value.name + "[" +
                             std::to_string(nmap[value.name]) + "]";
            }
        }
    };
    fix_names(scene.cameras, "camera");
    fix_names(scene.shapes, "shape");
    fix_names(scene.textures, "texture");
    fix_names(scene.voltextures, "voltexture");
    fix_names(scene.materials, "material");
    fix_names(scene.instances, "instance");
    fix_names(scene.environments, "environment");
    fix_names(scene.nodes, "node");
    fix_names(scene.animations, "animation");
}

// Add missing tangent space if needed.
void add_missing_tangent_space(yocto_scene& scene) {
    for (auto& shape : scene.shapes) {
        auto& material = scene.materials[shape.material];
        if (!shape.tangentspaces.empty() || shape.texturecoords.empty())
            continue;
        if (material.normal_texture < 0) continue;
        if (!shape.triangles.empty()) {
            if (shape.normals.empty()) {
                shape.normals.resize(shape.positions.size());
                compute_vertex_normals(
                    shape.normals, shape.triangles, shape.positions);
            }
            shape.tangentspaces.resize(shape.positions.size());
            compute_tangent_spaces(shape.tangentspaces, shape.triangles,
                shape.positions, shape.normals, shape.texturecoords);
        } else {
            throw runtime_error("type not supported");
        }
    }
}

// Add missing materials.
void add_missing_materials(yocto_scene& scene) {
    auto material_id = -1;
    for (auto& shape : scene.shapes) {
        if (shape.material >= 0) continue;
        if (material_id < 0) {
            auto material    = yocto_material{};
            material.name    = "<default>";
            material.diffuse = {0.2f, 0.2f, 0.2f};
            scene.materials.push_back(material);
            material_id = (int)scene.materials.size() - 1;
        }
        shape.material = material_id;
    }
}

// Add missing cameras.
void add_missing_cameras(yocto_scene& scene) {
    if (scene.cameras.empty()) {
        auto camera = yocto_camera{};
        camera.name = "<view>";
        set_camera_view_from_bbox(
            camera, compute_scene_bounds(scene), {0, 0, 1});
        scene.cameras.push_back(camera);
    }
}

// Add a sky environment
void add_sky_environment(yocto_scene& scene, float sun_angle) {
    auto texture     = yocto_texture{};
    texture.name     = "<sky>";
    texture.filename = "textures/sky.hdr";
    texture.hdr_image.resize({1024, 512});
    make_sunsky_image(texture.hdr_image, sun_angle);
    scene.textures.push_back(texture);
    auto environment             = yocto_environment{};
    environment.name             = "<sky>";
    environment.emission         = {1, 1, 1};
    environment.emission_texture = (int)scene.textures.size() - 1;
    scene.environments.push_back(environment);
}

// Checks for validity of the scene.
vector<string> validate_scene(const yocto_scene& scene, bool skip_textures) {
    auto errs        = vector<string>();
    auto check_names = [&errs](const auto& vals, const string& base) {
        auto used = unordered_map<string, int>();
        for (auto& value : vals) used[value.name] += 1;
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
            if (value.hdr_image.empty() && value.ldr_image.empty()) {
                errs.push_back("empty texture " + value.name);
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
    if (!skip_textures) check_empty_textures(scene.textures);

    return errs;
}

// Logs validations errors
void print_validation_errors(const yocto_scene& scene, bool skip_textures) {
    for (auto err : validate_scene(scene, skip_textures))
        printf("%s [validation]\n", err.c_str());
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR EVAL AND SAMPLING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Shape element normal.
vec3f evaluate_shape_element_normal(const yocto_shape& shape, int element_id) {
    auto norm = zero3f;
    if (!shape.triangles.empty()) {
        auto t = shape.triangles[element_id];
        norm   = triangle_normal(
            shape.positions[t.x], shape.positions[t.y], shape.positions[t.z]);
    } else if (!shape.quads.empty()) {
        auto q = shape.quads[element_id];
        norm   = quad_normal(shape.positions[q.x], shape.positions[q.y],
            shape.positions[q.z], shape.positions[q.w]);
    } else if (!shape.lines.empty()) {
        auto l = shape.lines[element_id];
        norm   = line_tangent(shape.positions[l.x], shape.positions[l.y]);
    } else if (!shape.quads_positions.empty()) {
        auto q = shape.quads_positions[element_id];
        norm   = quad_normal(shape.positions[q.x], shape.positions[q.y],
            shape.positions[q.z], shape.positions[q.w]);
    } else {
        throw runtime_error("empty shape");
        norm = {0, 0, 1};
    }
    return norm;
}

// Shape element normal.
pair<vec3f, bool> evaluate_shape_element_tangentspace(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (!shape.triangles.empty()) {
        auto t    = shape.triangles[element_id];
        auto norm = triangle_normal(
            shape.positions[t.x], shape.positions[t.y], shape.positions[t.z]);
        auto txty = pair<vec3f, vec3f>();
        if (shape.texturecoords.empty()) {
            txty = triangle_tangents_fromuv(shape.positions[t.x],
                shape.positions[t.y], shape.positions[t.z], {0, 0}, {1, 0},
                {0, 1});
        } else {
            txty = triangle_tangents_fromuv(shape.positions[t.x],
                shape.positions[t.y], shape.positions[t.z],
                shape.texturecoords[t.x], shape.texturecoords[t.y],
                shape.texturecoords[t.z]);
        }
        auto tx = txty.first, ty = txty.second;
        tx     = orthonormalize(tx, norm);
        auto s = (dot(cross(norm, tx), ty) < 0) ? -1.0f : 1.0f;
        return {tx, s};
    } else if (!shape.quads.empty()) {
        auto q    = shape.quads[element_id];
        auto norm = quad_normal(shape.positions[q.x], shape.positions[q.y],
            shape.positions[q.z], shape.positions[q.w]);
        auto txty = pair<vec3f, vec3f>();
        if (shape.texturecoords.empty()) {
            txty = quad_tangents_fromuv(shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], {0, 0}, {1, 0}, {0, 1}, {1, 1},
                element_uv);
        } else {
            txty = quad_tangents_fromuv(shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], shape.texturecoords[q.x],
                shape.texturecoords[q.y], shape.texturecoords[q.z],
                shape.texturecoords[q.w], element_uv);
        }
        auto tx = txty.first, ty = txty.second;
        tx     = orthonormalize(tx, norm);
        auto s = (dot(cross(norm, tx), ty) < 0) ? -1.0f : 1.0f;
        return {tx, s};
    } else if (!shape.quads_positions.empty()) {
        auto q    = shape.quads_positions[element_id];
        auto norm = quad_normal(shape.positions[q.x], shape.positions[q.y],
            shape.positions[q.z], shape.positions[q.w]);
        auto txty = pair<vec3f, vec3f>();
        if (shape.texturecoords.empty()) {
            txty = quad_tangents_fromuv(shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], {0, 0}, {1, 0}, {0, 1}, {1, 1},
                element_uv);
        } else {
            auto qt = shape.quads_texturecoords[element_id];
            txty    = quad_tangents_fromuv(shape.positions[q.x],
                shape.positions[q.y], shape.positions[q.z],
                shape.positions[q.w], shape.texturecoords[qt.x],
                shape.texturecoords[qt.y], shape.texturecoords[qt.z],
                shape.texturecoords[qt.w], element_uv);
        }
        auto tx = txty.first, ty = txty.second;
        tx     = orthonormalize(tx, norm);
        auto s = (dot(cross(norm, tx), ty) < 0) ? -1.0f : 1.0f;
        return {tx, s};
    } else {
        return {zero3f, false};
    }
}

// Shape value interpolated using barycentric coordinates
template <typename T>
T evaluate_shape_elem(const yocto_shape& shape,
    const vector<vec4i>& facevarying_quads, const vector<T>& vals,
    int element_id, const vec2f& element_uv) {
    if (vals.empty()) return {};
    if (!shape.triangles.empty()) {
        auto t = shape.triangles[element_id];
        return interpolate_triangle(
            vals[t.x], vals[t.y], vals[t.z], element_uv);
    } else if (!shape.quads.empty()) {
        auto q = shape.quads[element_id];
        if (q.w == q.z)
            return interpolate_triangle(
                vals[q.x], vals[q.y], vals[q.z], element_uv);
        return interpolate_quad(
            vals[q.x], vals[q.y], vals[q.z], vals[q.w], element_uv);
    } else if (!shape.lines.empty()) {
        auto l = shape.lines[element_id];
        return interpolate_line(vals[l.x], vals[l.y], element_uv.x);
    } else if (!shape.points.empty()) {
        return vals[shape.points[element_id]];
    } else if (!shape.quads_positions.empty()) {
        auto q = facevarying_quads[element_id];
        if (q.w == q.z)
            return interpolate_triangle(
                vals[q.x], vals[q.y], vals[q.z], element_uv);
        return interpolate_quad(
            vals[q.x], vals[q.y], vals[q.z], vals[q.w], element_uv);
    } else {
        return {};
    }
}

// Shape values interpolated using barycentric coordinates
vec3f evaluate_shape_position(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    return evaluate_shape_elem(
        shape, shape.quads_positions, shape.positions, element_id, element_uv);
}
vec3f evaluate_shape_normal(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (shape.normals.empty())
        return evaluate_shape_element_normal(shape, element_id);
    return normalize(evaluate_shape_elem(
        shape, shape.quads_normals, shape.normals, element_id, element_uv));
}
vec2f evaluate_shape_texturecoord(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (shape.texturecoords.empty()) return element_uv;
    return evaluate_shape_elem(shape, shape.quads_texturecoords,
        shape.texturecoords, element_id, element_uv);
}
vec4f evaluate_shape_color(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (shape.colors.empty()) return {1, 1, 1, 1};
    return evaluate_shape_elem(shape, {}, shape.colors, element_id, element_uv);
}
float evaluate_shape_radius(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (shape.radius.empty()) return 0.001f;
    return evaluate_shape_elem(shape, {}, shape.radius, element_id, element_uv);
}
pair<vec3f, bool> evaluate_shape_tangentspace(
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    if (shape.tangentspaces.empty())
        return evaluate_shape_element_tangentspace(
            shape, element_id, element_uv);
    auto tangsp = evaluate_shape_elem(
        shape, {}, shape.tangentspaces, element_id, element_uv);
    return {xyz(tangsp), tangsp.w < 0};
}
// Shading normals including material perturbations.
vec3f evaluate_shape_perturbed_normal(const yocto_scene& scene,
    const yocto_shape& shape, int element_id, const vec2f& element_uv) {
    auto normal = evaluate_shape_normal(shape, element_id, element_uv);
    if (shape.triangles.empty() && shape.quads.empty()) return normal;
    auto& material = scene.materials[shape.material];
    if (scene.materials[shape.material].normal_texture >= 0) {
        auto normalmap         = evaluate_material_normalmap(scene, material,
            evaluate_shape_texturecoord(shape, element_id, element_uv));
        auto [tu, left_handed] = evaluate_shape_tangentspace(
            shape, element_id, element_uv);
        tu      = orthonormalize(tu, normal);
        auto tv = normalize(cross(normal, tu) * (left_handed ? -1.0f : 1.0f));
        normal  = normalize(
            normalmap.x * tu + normalmap.y * tv + normalmap.z * normal);
    }
    return normal;
}

// Instance values interpolated using barycentric coordinates.
vec3f evaluate_instance_position(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv) {
    return transform_point(
        instance.frame, evaluate_shape_position(scene.shapes[instance.shape],
                            element_id, element_uv));
}
vec3f evaluate_instance_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv) {
    return transform_direction(
        instance.frame, evaluate_shape_normal(scene.shapes[instance.shape],
                            element_id, element_uv));
}
vec3f evaluate_instance_perturbed_normal(const yocto_scene& scene,
    const yocto_instance& instance, int element_id, const vec2f& element_uv) {
    return transform_direction(instance.frame,
        evaluate_shape_perturbed_normal(
            scene, scene.shapes[instance.shape], element_id, element_uv));
}
// Instance element values.
vec3f evaluate_instance_element_normal(
    const yocto_scene& scene, const yocto_instance& instance, int element_id) {
    return transform_direction(
        instance.frame, evaluate_shape_element_normal(
                            scene.shapes[instance.shape], element_id));
}

// Environment texture coordinates from the direction.
vec2f evaluate_environment_texturecoord(
    const yocto_environment& environment, const vec3f& direction) {
    auto wl = transform_direction_inverse(environment.frame, direction);
    auto environment_uv = vec2f{
        atan2(wl.z, wl.x) / (2 * pif), acos(clamp(wl.y, -1.0f, 1.0f)) / pif};
    if (environment_uv.x < 0) environment_uv.x += 1;
    return environment_uv;
}
// Evaluate the environment direction.
vec3f evaluate_environment_direction(
    const yocto_environment& environment, const vec2f& environment_uv) {
    return transform_direction(environment.frame,
        {cos(environment_uv.x * 2 * pif) * sin(environment_uv.y * pif),
            cos(environment_uv.y * pif),
            sin(environment_uv.x * 2 * pif) * sin(environment_uv.y * pif)});
}
// Evaluate the environment color.
vec3f evaluate_environment_emission(const yocto_scene& scene,
    const yocto_environment& environment, const vec3f& direction) {
    auto ke = environment.emission;
    if (environment.emission_texture >= 0) {
        auto& emission_texture = scene.textures[environment.emission_texture];
        ke *= xyz(evaluate_texture(emission_texture,
            evaluate_environment_texturecoord(environment, direction)));
    }
    return ke;
}
// Evaluate all environment color.
vec3f evaluate_environment_emission(
    const yocto_scene& scene, const vec3f& direction) {
    auto ke = zero3f;
    for (auto& environment : scene.environments)
        ke += evaluate_environment_emission(scene, environment, direction);
    return ke;
}

// Check texture size
vec2i evaluate_texture_size(const yocto_texture& texture) {
    if (!texture.hdr_image.empty()) {
        return texture.hdr_image.size();
    } else if (!texture.ldr_image.empty()) {
        return texture.ldr_image.size();
    } else {
        return zero2i;
    }
}

// Lookup a texture value
vec4f lookup_texture(const yocto_texture& texture, int i, int j) {
    if (!texture.hdr_image.empty()) {
        return texture.hdr_image[{i, j}];
    } else if (!texture.ldr_image.empty() && !texture.ldr_as_linear) {
        return srgb_to_linear(byte_to_float(texture.ldr_image[{i, j}]));
    } else if (!texture.ldr_image.empty() && texture.ldr_as_linear) {
        return byte_to_float(texture.ldr_image[{i, j}]);
    } else {
        return zero4f;
    }
}

// Evaluate a texture
vec4f evaluate_texture(const yocto_texture& texture, const vec2f& texcoord) {
    if (texture.hdr_image.empty() && texture.ldr_image.empty())
        return {1, 1, 1, 1};

    // get image width/height
    auto size  = evaluate_texture_size(texture);
    auto width = size.x, height = size.y;

    // get coordinates normalized for tiling
    auto s = 0.0f, t = 0.0f;
    if (texture.clamp_to_edge) {
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

    // nearest-neighbor interpolation
    if (texture.no_interpolation) {
        i = u < 0.5 ? i : min(i + 1, width - 1);
        j = v < 0.5 ? j : min(j + 1, height - 1);
        return lookup_texture(texture, i, j);
    }

    // handle interpolation
    return lookup_texture(texture, i, j) * (1 - u) * (1 - v) +
           lookup_texture(texture, i, jj) * (1 - u) * v +
           lookup_texture(texture, ii, j) * u * (1 - v) +
           lookup_texture(texture, ii, jj) * u * v;
}

// Lookup a texture value
float lookup_voltexture(const yocto_voltexture& texture, int i, int j, int k) {
    if (!texture.volume_data.empty()) {
        return texture.volume_data[{i, j, k}];
    } else {
        return 0;
    }
}

// Evaluate a volume texture
float evaluate_voltexture(
    const yocto_voltexture& texture, const vec3f& texcoord) {
    if (texture.volume_data.empty()) return 1;

    // get image width/height
    auto width  = texture.volume_data.size().x;
    auto height = texture.volume_data.size().y;
    auto depth  = texture.volume_data.size().z;

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
    if (texture.no_interpolation) {
        i = u < 0.5 ? i : min(i + 1, width - 1);
        j = v < 0.5 ? j : min(j + 1, height - 1);
        k = w < 0.5 ? k : min(k + 1, depth - 1);
        return lookup_voltexture(texture, i, j, k);
    }

    // trilinear interpolation
    return lookup_voltexture(texture, i, j, k) * (1 - u) * (1 - v) * (1 - w) +
           lookup_voltexture(texture, ii, j, k) * u * (1 - v) * (1 - w) +
           lookup_voltexture(texture, i, jj, k) * (1 - u) * v * (1 - w) +
           lookup_voltexture(texture, i, j, kk) * (1 - u) * (1 - v) * w +
           lookup_voltexture(texture, i, jj, kk) * (1 - u) * v * w +
           lookup_voltexture(texture, ii, j, kk) * u * (1 - v) * w +
           lookup_voltexture(texture, ii, jj, k) * u * v * (1 - w) +
           lookup_voltexture(texture, ii, jj, kk) * u * v * w;
}

// Set and evaluate camera parameters. Setters take zeros as default values.
float get_camera_fovx(const yocto_camera& camera) {
    assert(!camera.orthographic);
    return 2 * atan(camera.film_width / (2 * camera.focal_length));
}
float get_camera_fovy(const yocto_camera& camera) {
    assert(!camera.orthographic);
    return 2 * atan(camera.film_height / (2 * camera.focal_length));
}
float get_camera_aspect(const yocto_camera& camera) {
    return camera.film_width / camera.film_height;
}
vec2i get_camera_image_size(const yocto_camera& camera, const vec2i& size_) {
    auto size = size_;
    if (size == zero2i) size = {1280, 720};
    if (size.x != 0 && size.y != 0) {
        if (size.x * camera.film_height / camera.film_width > size.y) {
            size.x = 0;
        } else {
            size.y = 0;
        }
    }
    if (size.x == 0) {
        size.x = (int)round(size.y * camera.film_width / camera.film_height);
    }
    if (size.y == 0) {
        size.y = (int)round(size.x * camera.film_height / camera.film_width);
    }
    return size;
}
void set_camera_perspective(
    yocto_camera& camera, float fovy, float aspect, float focus, float height) {
    camera.orthographic   = false;
    camera.film_width     = height * aspect;
    camera.film_height    = height;
    camera.focus_distance = focus;
    auto distance         = camera.film_height / (2 * tan(fovy / 2));
    if (focus < float_max) {
        camera.focal_length = camera.focus_distance * distance /
                              (camera.focus_distance + distance);
    } else {
        camera.focal_length = distance;
    }
}

// add missing camera
void set_camera_view_from_bbox(yocto_camera& camera, const bbox3f& bbox,
    const vec3f& view_direction, float width, float height, float focal) {
    camera.orthographic = false;
    if (width != 0) camera.film_width = width;
    if (height != 0) camera.film_height = height;
    if (focal != 0) camera.focal_length = focal;
    auto bbox_center = (bbox.max + bbox.min) / 2.0f;
    auto bbox_radius = length(bbox.max - bbox.min) / 2;
    auto camera_dir  = (view_direction == zero3f) ? camera.frame.o - bbox_center
                                                 : view_direction;
    if (camera_dir == zero3f) camera_dir = {0, 0, 1};
    auto camera_fov = min(get_camera_fovx(camera), get_camera_fovy(camera));
    if (camera_fov == 0) camera_fov = 45 * pif / 180;
    auto camera_dist      = bbox_radius / sin(camera_fov / 2);
    auto from             = camera_dir * (camera_dist * 1) + bbox_center;
    auto to               = bbox_center;
    auto up               = vec3f{0, 1, 0};
    camera.frame          = make_lookat_frame(from, to, up);
    camera.focus_distance = length(from - to);
    camera.lens_aperture  = 0;
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f evaluate_perspective_camera_ray(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv) {
    auto distance = camera.focal_length;
    if (camera.focus_distance < float_max) {
        distance = camera.focal_length * camera.focus_distance /
                   (camera.focus_distance - camera.focal_length);
    }
    if (camera.lens_aperture) {
        auto e = vec3f{(lens_uv.x - 0.5f) * camera.lens_aperture,
            (lens_uv.y - 0.5f) * camera.lens_aperture, 0};
        auto q = vec3f{camera.film_width * (0.5f - image_uv.x),
            camera.film_height * (image_uv.y - 0.5f), distance};
        // distance of the image of the point
        auto distance1 = camera.focal_length * distance /
                         (distance - camera.focal_length);
        auto q1 = -q * distance1 / distance;
        auto d  = normalize(q1 - e);
        // auto q1 = - normalize(q) * camera.focus_distance / normalize(q).z;
        auto ray = make_ray(transform_point(camera.frame, e),
            transform_direction(camera.frame, d));
        return ray;
    } else {
        auto e   = zero3f;
        auto q   = vec3f{camera.film_width * (0.5f - image_uv.x),
            camera.film_height * (image_uv.y - 0.5f), distance};
        auto q1  = -q;
        auto d   = normalize(q1 - e);
        auto ray = make_ray(transform_point(camera.frame, e),
            transform_direction(camera.frame, d));
        return ray;
    }
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f evaluate_orthographic_camera_ray(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv) {
    if (camera.lens_aperture) {
        auto scale = 1 / camera.focal_length;
        auto q     = vec3f{camera.film_width * (0.5f - image_uv.x) * scale,
            camera.film_height * (image_uv.y - 0.5f) * scale, scale};
        auto q1    = vec3f{-q.x, -q.y, -camera.focus_distance};
        auto e     = vec3f{-q.x, -q.y, 0} +
                 vec3f{(lens_uv.x - 0.5f) * camera.lens_aperture,
                     (lens_uv.y - 0.5f) * camera.lens_aperture, 0};
        auto d   = normalize(q1 - e);
        auto ray = make_ray(transform_point(camera.frame, e),
            transform_direction(camera.frame, d));
        return ray;
    } else {
        auto scale = 1 / camera.focal_length;
        auto q     = vec3f{camera.film_width * (0.5f - image_uv.x) * scale,
            camera.film_height * (image_uv.y - 0.5f) * scale, scale};
        auto q1    = -q;
        auto e     = vec3f{-q.x, -q.y, 0};
        auto d     = normalize(q1 - e);
        auto ray   = make_ray(transform_point(camera.frame, e),
            transform_direction(camera.frame, d));
        return ray;
    }
}

// Generates a ray from a camera for image plane coordinate uv and
// the lens coordinates luv.
ray3f evaluate_camera_ray(
    const yocto_camera& camera, const vec2f& image_uv, const vec2f& lens_uv) {
    if (camera.orthographic)
        return evaluate_orthographic_camera_ray(camera, image_uv, lens_uv);
    else
        return evaluate_perspective_camera_ray(camera, image_uv, lens_uv);
}

// Generates a ray from a camera.
ray3f evaluate_camera_ray(const yocto_camera& camera, const vec2i& image_ij,
    const vec2i& image_size, const vec2f& pixel_uv, const vec2f& lens_uv) {
    auto image_uv = vec2f{(image_ij.x + pixel_uv.x) / image_size.x,
        (image_ij.y + pixel_uv.y) / image_size.y};
    return evaluate_camera_ray(camera, image_uv, lens_uv);
}

// Generates a ray from a camera.
ray3f evaluate_camera_ray(const yocto_camera& camera, int idx,
    const vec2i& image_size, const vec2f& pixel_uv, const vec2f& lens_uv) {
    auto image_ij = vec2i{idx % image_size.x, idx / image_size.x};
    auto image_uv = vec2f{(image_ij.x + pixel_uv.x) / image_size.x,
        (image_ij.y + pixel_uv.y) / image_size.y};
    return evaluate_camera_ray(camera, image_uv, lens_uv);
}

// Evaluates material parameters.
vec3f evaluate_material_emission(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord) {
    auto emission = material.emission;
    if (material.emission_texture >= 0) {
        auto& emission_texture = scene.textures[material.emission_texture];
        emission *= xyz(evaluate_texture(emission_texture, texturecoord));
    }
    return emission;
}
vec3f evaluate_material_normalmap(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord) {
    if (material.normal_texture >= 0) {
        auto& normal_texture = scene.textures[material.normal_texture];
        auto  normalmap = xyz(evaluate_texture(normal_texture, texturecoord));
        normalmap       = normalmap * 2 - vec3f{1, 1, 1};
        normalmap.y = -normalmap.y;  // flip vertical axis to align green with
                                     // image up
        return normalmap;
    } else {
        return {0, 0, 1};
    }
}

// Evaluates the microfacet_brdf at a location.
microfacet_brdf evaluate_basemetallic_material_brdf(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord) {
    auto base    = material.diffuse;
    auto opacity = material.opacity;
    if (material.diffuse_texture >= 0) {
        auto& diffuse_texture = scene.textures[material.diffuse_texture];
        auto  diffuse_txt     = evaluate_texture(diffuse_texture, texturecoord);
        base *= xyz(diffuse_txt);
        opacity *= diffuse_txt.w;
    }
    auto metallic = material.specular;
    if (material.specular_texture >= 0) {
        auto& specular_texture = scene.textures[material.specular_texture];
        metallic *= evaluate_texture(specular_texture, texturecoord).z;
    }
    auto diffuse   = base * (1 - metallic);
    auto specular  = base * metallic + 0.04f * (1 - metallic);
    auto roughness = 1.0f;
    if (!material.gltf_textures) {
        roughness = material.roughness;
        if (material.roughness_texture >= 0) {
            auto& roughness_texture =
                scene.textures[material.roughness_texture];
            roughness *= evaluate_texture(roughness_texture, texturecoord).x;
        }
    } else {
        auto glossiness = 1 - material.roughness;
        if (material.roughness_texture >= 0) {
            auto& roughness_texture =
                scene.textures[material.roughness_texture];
            glossiness *= evaluate_texture(roughness_texture, texturecoord).w;
        }
        roughness = 1 - glossiness;
    }
    roughness         = roughness * roughness;
    auto transmission = material.transmission;
    if (material.transmission_texture >= 0) {
        auto& transmission_texture =
            scene.textures[material.transmission_texture];
        transmission *= xyz(
            evaluate_texture(transmission_texture, texturecoord));
    }
    auto fresnel = material.fresnel;
    auto refract = material.refract;
    auto brdf    = microfacet_brdf{
        diffuse, specular, transmission, roughness, opacity, fresnel, refract};
    if (brdf.diffuse != zero3f) {
        brdf.roughness = clamp(brdf.roughness, 0.03f * 0.03f, 1.0f);
    } else if (brdf.roughness <= 0.03f * 0.03f)
        brdf.roughness = 0;
    return brdf;
}

// Evaluates the microfacet_brdf at a location.
microfacet_brdf evaluate_material_brdf(const yocto_scene& scene,
    const yocto_material& material, const vec2f& texturecoord) {
    if (material.base_metallic)
        return evaluate_basemetallic_material_brdf(
            scene, material, texturecoord);
    auto diffuse = material.diffuse;
    auto opacity = material.opacity;
    if (material.diffuse_texture >= 0) {
        auto& diffuse_texture = scene.textures[material.diffuse_texture];
        auto  diffuse_txt     = evaluate_texture(diffuse_texture, texturecoord);
        diffuse *= xyz(diffuse_txt);
        opacity *= diffuse_txt.w;
    }
    auto specular = material.specular;
    if (material.specular_texture >= 0) {
        auto& specular_texture = scene.textures[material.specular_texture];
        specular *= xyz(evaluate_texture(specular_texture, texturecoord));
    }
    auto roughness = material.roughness;
    if (material.roughness_texture >= 0) {
        auto& roughness_texture = scene.textures[material.roughness_texture];
        roughness *= evaluate_texture(roughness_texture, texturecoord).x;
    }
    roughness         = roughness * roughness;
    auto transmission = material.transmission;
    if (material.transmission_texture >= 0) {
        auto& transmission_texture =
            scene.textures[material.transmission_texture];
        transmission *= xyz(
            evaluate_texture(transmission_texture, texturecoord));
    }
    auto fresnel = material.fresnel;
    auto refract = material.refract;
    auto brdf    = microfacet_brdf{
        diffuse, specular, transmission, roughness, opacity, fresnel, refract};
    if (brdf.diffuse != zero3f) {
        brdf.roughness = clamp(brdf.roughness, 0.03f * 0.03f, 1.0f);
    } else if (brdf.roughness <= 0.03f * 0.03f)
        brdf.roughness = 0;
    return brdf;
}

bool is_brdf_delta(const microfacet_brdf& brdf) {
    return brdf.roughness == 0 && brdf.diffuse == zero3f &&
           (brdf.specular != zero3f || brdf.transmission != zero3f);
}
bool is_brdf_zero(const microfacet_brdf& brdf) {
    return brdf.diffuse == zero3f && brdf.specular == zero3f &&
           brdf.transmission == zero3f;
}

bool is_material_volume_homogeneus(const yocto_material& material) {
    return material.volume_density_texture < 0;
}
bool is_material_volume_colored(const yocto_material& material) {
    return !(material.volume_density.x == material.volume_density.y &&
             material.volume_density.y == material.volume_density.z);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SCENE UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

void merge_scene_into(yocto_scene& scene, const yocto_scene& merge) {
    auto offset_cameras      = scene.cameras.size();
    auto offset_textures     = scene.textures.size();
    auto offset_voltextures  = scene.voltextures.size();
    auto offset_materials    = scene.materials.size();
    auto offset_shapes       = scene.shapes.size();
    auto offset_instances    = scene.instances.size();
    auto offset_environments = scene.environments.size();
    auto offset_nodes        = scene.nodes.size();
    auto offset_animations   = scene.animations.size();
    scene.cameras += merge.cameras;
    scene.textures += merge.textures;
    scene.voltextures += merge.voltextures;
    scene.materials += merge.materials;
    scene.shapes += merge.shapes;
    scene.instances += merge.instances;
    scene.environments += merge.environments;
    scene.nodes += merge.nodes;
    scene.animations += merge.animations;
    for (auto material_id = offset_materials;
         material_id < scene.materials.size(); material_id++) {
        auto& material = scene.materials[material_id];
        if (material.emission_texture >= 0)
            material.emission_texture += offset_textures;
        if (material.diffuse_texture >= 0)
            material.diffuse_texture += offset_textures;
        if (material.specular_texture >= 0)
            material.specular_texture += offset_textures;
        if (material.transmission_texture >= 0)
            material.transmission_texture += offset_textures;
        if (material.roughness_texture >= 0)
            material.roughness_texture += offset_textures;
        if (material.normal_texture >= 0)
            material.normal_texture += offset_textures;
        if (material.displacement_texture >= 0)
            material.displacement_texture += offset_textures;
        if (material.volume_density_texture >= 0)
            material.volume_density_texture += offset_voltextures;
    }
    for (auto shape_id = offset_shapes; shape_id < scene.shapes.size();
         shape_id++) {
        auto& shape = scene.shapes[shape_id];
        if (shape.material >= 0) shape.material += offset_materials;
    }
    for (auto instance_id = offset_instances;
         instance_id < scene.instances.size(); instance_id++) {
        auto& instance = scene.instances[instance_id];
        if (instance.shape >= 0) instance.shape += offset_shapes;
    }
    for (auto environment_id = offset_environments;
         environment_id < scene.environments.size(); environment_id++) {
        auto& environment = scene.environments[environment_id];
        if (environment.emission_texture >= 0)
            environment.emission_texture += offset_textures;
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
        for (auto& target : animation.node_targets)
            if (target >= 0) target += offset_nodes;
    }
}

string print_scene_stats(const yocto_scene& scene) {
    // using long long instead of uint64_t to avoid printf macros
    auto num_cameras      = (long long)0;
    auto num_shapes       = (long long)0;
    auto num_surfaces     = (long long)0;
    auto num_instances    = (long long)0;
    auto num_materials    = (long long)0;
    auto num_textures     = (long long)0;
    auto num_voltextures  = (long long)0;
    auto num_environments = (long long)0;
    auto num_nodes        = (long long)0;
    auto num_animations   = (long long)0;

    auto elem_points         = (long long)0;
    auto elem_lines          = (long long)0;
    auto elem_triangles      = (long long)0;
    auto elem_quads          = (long long)0;
    auto elem_quads_pos      = (long long)0;
    auto elem_quads_norm     = (long long)0;
    auto elem_quads_texcoord = (long long)0;
    auto vert_pos            = (long long)0;
    auto vert_norm           = (long long)0;
    auto vert_texcoord       = (long long)0;
    auto vert_color          = (long long)0;
    auto vert_radius         = (long long)0;
    auto vert_tangsp         = (long long)0;

    auto texel_hdr = (long long)0;
    auto texel_ldr = (long long)0;
    auto voxel_hdr = (long long)0;

    auto memory_imgs  = (long long)0;
    auto memory_vols  = (long long)0;
    auto memory_elems = (long long)0;
    auto memory_verts = (long long)0;

    auto bbox = compute_scene_bounds(scene);

    num_cameras      = scene.cameras.size();
    num_shapes       = scene.shapes.size();
    num_materials    = scene.materials.size();
    num_textures     = scene.textures.size();
    num_voltextures  = scene.voltextures.size();
    num_environments = scene.environments.size();
    num_instances    = scene.instances.size();
    num_nodes        = scene.nodes.size();
    num_animations   = scene.animations.size();

    for (auto& shape : scene.shapes) {
        elem_points += shape.points.size();
        elem_lines += shape.lines.size();
        elem_triangles += shape.triangles.size();
        elem_quads += shape.quads.size();
        elem_quads_pos += shape.quads_positions.size();
        elem_quads_norm += shape.quads_normals.size();
        elem_quads_texcoord += shape.quads_texturecoords.size();
        vert_pos += shape.positions.size();
        vert_norm += shape.normals.size();
        vert_texcoord += shape.texturecoords.size();
        vert_color += shape.colors.size();
        vert_radius += shape.radius.size();
        vert_tangsp += shape.tangentspaces.size();
    }

    memory_elems = elem_points * sizeof(int) + elem_lines * sizeof(vec2i) +
                   elem_triangles * sizeof(vec3i) + elem_quads * sizeof(vec4i) +
                   elem_quads_pos * sizeof(vec4i) +
                   elem_quads_norm * sizeof(vec4i) +
                   elem_quads_texcoord * sizeof(vec4i);
    memory_verts = vert_pos * sizeof(vec3f) + vert_norm * sizeof(vec3f) +
                   vert_texcoord * sizeof(vec2f) + vert_color * sizeof(vec4f) +
                   vert_tangsp * sizeof(vec4f) + vert_radius * sizeof(float);

    for (auto& texture : scene.textures) {
        texel_hdr += texture.hdr_image.size().x * texture.hdr_image.size().y;
        texel_ldr += texture.ldr_image.size().x * texture.ldr_image.size().y;
    }
    memory_imgs = texel_hdr * sizeof(vec4f) + texel_ldr * sizeof(vec4b);

    for (auto& voltexture : scene.voltextures) {
        voxel_hdr += voltexture.volume_data.size().x *
                     voltexture.volume_data.size().y *
                     voltexture.volume_data.size().z;
    }
    memory_vols = voxel_hdr * sizeof(float);

    auto str = ""s;

    str += "num_cameras: " + std::to_string(num_cameras) + "\n";
    str += "num_shapes: " + std::to_string(num_shapes) + "\n";
    str += "num_surface: " + std::to_string(num_surfaces) + "\n";
    str += "num_instances: " + std::to_string(num_instances) + "\n";
    str += "num_materials: " + std::to_string(num_materials) + "\n";
    str += "num_textures: " + std::to_string(num_textures) + "\n";
    str += "num_voltextures: " + std::to_string(num_voltextures) + "\n";
    str += "num_environments: " + std::to_string(num_environments) + "\n";
    str += "num_nodes: " + std::to_string(num_nodes) + "\n";
    str += "num_animations: " + std::to_string(num_animations) + "\n";

    str += "elem_points: " + std::to_string(elem_points) + "\n";
    str += "elem_lines: " + std::to_string(elem_lines) + "\n";
    str += "elem_triangles: " + std::to_string(elem_triangles) + "\n";
    str += "elem_quads: " + std::to_string(elem_quads) + "\n";
    str += "vert_pos: " + std::to_string(vert_pos) + "\n";
    str += "vert_norm: " + std::to_string(vert_norm) + "\n";
    str += "vert_texcoord: " + std::to_string(vert_texcoord) + "\n";
    str += "vert_color: " + std::to_string(vert_color) + "\n";
    str += "vert_radius: " + std::to_string(vert_radius) + "\n";
    str += "vert_tangsp: " + std::to_string(vert_tangsp) + "\n";

    str += "elem_points: " + std::to_string(elem_points) + "\n";
    str += "elem_lines: " + std::to_string(elem_lines) + "\n";
    str += "elem_triangles: " + std::to_string(elem_triangles) + "\n";
    str += "elem_quads: " + std::to_string(elem_quads) + "\n";
    str += "vert_pos: " + std::to_string(vert_pos) + "\n";
    str += "vert_norm: " + std::to_string(vert_norm) + "\n";
    str += "vert_texcoord: " + std::to_string(vert_texcoord) + "\n";

    str += "texel_hdr: " + std::to_string(texel_hdr) + "\n";
    str += "texel_ldr: " + std::to_string(texel_ldr) + "\n";

    str += "memory_imgs: " + std::to_string(memory_imgs) + "\n";
    str += "memory_vols: " + std::to_string(memory_vols) + "\n";
    str += "memory_elems: " + std::to_string(memory_elems) + "\n";
    str += "memory_verts: " + std::to_string(memory_verts) + "\n";

    str += "bbox min: " + std::to_string(bbox.min.x) + " " +
           std::to_string(bbox.min.y) + " " + std::to_string(bbox.min.z) + "\n";
    str += "bbox max: " + std::to_string(bbox.max.x) + " " +
           std::to_string(bbox.max.y) + " " + std::to_string(bbox.max.z) + "\n";

    return str;
}

}  // namespace yocto
