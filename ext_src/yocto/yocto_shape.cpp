//
// Implementation for Yocto/Shape.
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

#include "yocto_shape.h"
#include "yocto_random.h"
#include "yocto_utils.h"

#include <cassert>

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF COMPUTATION OF PER_VERTEX PROPETIES
// -----------------------------------------------------------------------------
namespace yocto {

// Compute per-vertex tangents for lines.
void compute_vertex_tangents(vector<vec3f>& tangents,
    const vector<vec2i>& lines, const vector<vec3f>& positions) {
    if (tangents.size() != positions.size()) {
        throw std::out_of_range("array should be the same length");
    }
    for (auto& tangent : tangents) tangent = zero3f;
    for (auto& l : lines) {
        auto tangent = line_tangent(positions[l.x], positions[l.y]);
        auto length  = line_length(positions[l.x], positions[l.y]);
        tangents[l.x] += tangent * length;
        tangents[l.y] += tangent * length;
    }
    for (auto& tangent : tangents) tangent = normalize(tangent);
}

// Compute per-vertex normals for triangles.
void compute_vertex_normals(vector<vec3f>& normals,
    const vector<vec3i>& triangles, const vector<vec3f>& positions) {
    if (normals.size() != positions.size()) {
        throw std::out_of_range("array should be the same length");
    }
    for (auto& normal : normals) normal = zero3f;
    for (auto& t : triangles) {
        auto normal = triangle_normal(
            positions[t.x], positions[t.y], positions[t.z]);
        auto area = triangle_area(
            positions[t.x], positions[t.y], positions[t.z]);
        normals[t.x] += normal * area;
        normals[t.y] += normal * area;
        normals[t.z] += normal * area;
    }
    for (auto& normal : normals) normal = normalize(normal);
}

// Compute per-vertex normals for quads.
void compute_vertex_normals(vector<vec3f>& normals, const vector<vec4i>& quads,
    const vector<vec3f>& positions) {
    if (normals.size() != positions.size()) {
        throw std::out_of_range("array should be the same length");
    }
    for (auto& normal : normals) normal = zero3f;
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
void compute_tangent_spaces(vector<vec4f>& tangent_spaces,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords) {
    auto tangu = vector<vec3f>(positions.size(), zero3f);
    auto tangv = vector<vec3f>(positions.size(), zero3f);
    for (auto t : triangles) {
        auto [tu, tv] = triangle_tangents_fromuv(positions[t.x], positions[t.y],
            positions[t.z], texturecoords[t.x], texturecoords[t.y],
            texturecoords[t.z]);
        tu            = normalize(tu);
        tv            = normalize(tv);
        for (auto vid : {t.x, t.y, t.z}) tangu[vid] += tu;
        for (auto vid : {t.x, t.y, t.z}) tangv[vid] += tv;
    }
    for (auto& t : tangu) t = normalize(t);
    for (auto& t : tangv) t = normalize(t);
    for (auto& tangent : tangent_spaces) tangent = zero4f;
    for (auto i = 0; i < positions.size(); i++) {
        tangu[i] = orthonormalize(tangu[i], normals[i]);
        auto s   = (dot(cross(normals[i], tangu[i]), tangv[i]) < 0) ? -1.0f
                                                                  : 1.0f;
        tangent_spaces[i] = {tangu[i].x, tangu[i].y, tangu[i].z, s};
    }
}

// Apply skinning
void compute_skinning(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<frame3f>& xforms) {
    if (skinned_positions.size() != positions.size() ||
        skinned_normals.size() != normals.size()) {
        throw std::out_of_range("arrays should be the same size");
    }
    for (auto i = 0; i < positions.size(); i++) {
        skinned_positions[i] =
            transform_point(xforms[joints[i].x], positions[i]) * weights[i].x +
            transform_point(xforms[joints[i].y], positions[i]) * weights[i].y +
            transform_point(xforms[joints[i].z], positions[i]) * weights[i].z +
            transform_point(xforms[joints[i].w], positions[i]) * weights[i].w;
    }
    for (auto i = 0; i < normals.size(); i++) {
        skinned_normals[i] = normalize(
            transform_direction(xforms[joints[i].x], normals[i]) *
                weights[i].x +
            transform_direction(xforms[joints[i].y], normals[i]) *
                weights[i].y +
            transform_direction(xforms[joints[i].z], normals[i]) *
                weights[i].z +
            transform_direction(xforms[joints[i].w], normals[i]) *
                weights[i].w);
    }
}

// Apply skinning as specified in Khronos glTF
void compute_matrix_skinning(vector<vec3f>& skinned_positions,
    vector<vec3f>& skinned_normals, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec4f>& weights,
    const vector<vec4i>& joints, const vector<mat4f>& xforms) {
    if (skinned_positions.size() != positions.size() ||
        skinned_normals.size() != normals.size()) {
        throw std::out_of_range("arrays should be the same size");
    }
    for (auto i = 0; i < positions.size(); i++) {
        auto xform = xforms[joints[i].x] * weights[i].x +
                     xforms[joints[i].y] * weights[i].y +
                     xforms[joints[i].z] * weights[i].z +
                     xforms[joints[i].w] * weights[i].w;
        skinned_positions[i] = transform_point(xform, positions[i]);
        skinned_normals[i] = normalize(transform_direction(xform, normals[i]));
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF EDGE AND GRID DATA STRUCTURES
// -----------------------------------------------------------------------------
namespace yocto {

// Create key entry for edge_map
inline vec2i make_edgemap_edge(const vec2i& e) {
    return e.x < e.y ? e : vec2i{e.y, e.x};
}

// Initialize an edge map with elements.
void insert_edges(edge_map& emap, const vector<vec3i>& triangles) {
    for (int i = 0; i < triangles.size(); i++) {
        auto& t = triangles[i];
        insert_edge(emap, {t.x, t.y});
        insert_edge(emap, {t.y, t.z});
        insert_edge(emap, {t.z, t.x});
    }
}
void insert_edges(edge_map& emap, const vector<vec4i>& quads) {
    for (int i = 0; i < quads.size(); i++) {
        auto& q = quads[i];
        insert_edge(emap, {q.x, q.y});
        insert_edge(emap, {q.y, q.z});
        if (q.z != q.w) insert_edge(emap, {q.z, q.w});
        insert_edge(emap, {q.w, q.x});
    }
}
// Insert an edge and return its index
int insert_edge(edge_map& emap, const vec2i& edge) {
    auto es = make_edgemap_edge(edge);
    auto it = emap.edge_index.find(es);
    if (it == emap.edge_index.end()) {
        auto idx = (int)emap.edges.size();
        emap.edge_index.insert(it, {es, idx});
        emap.edges.push_back(es);
        emap.num_faces.push_back(1);
        return idx;
    } else {
        auto idx = it->second;
        emap.num_faces[idx] += 1;
        return idx;
    }
}
// Get number of edges
int get_num_edges(const edge_map& emap) { return emap.edges.size(); }
// Get the edge index
int get_edge_index(const edge_map& emap, const vec2i& edge) {
    auto iterator = emap.edge_index.find(make_edgemap_edge(edge));
    if (iterator == emap.edge_index.end()) return -1;
    return iterator->second;
}
// Get a list of edges, boundary edges, boundary vertices
void get_edges(const edge_map& emap, vector<vec2i>& edges) {
    edges = emap.edges;
}
void get_boundary(const edge_map& emap, vector<vec2i>& boundary) {
    boundary.clear();
    for (auto edge_index = 0; edge_index < emap.edges.size(); edge_index++) {
        if (emap.num_faces[edge_index] < 2)
            boundary.push_back(emap.edges[edge_index]);
    }
}
void get_edges(const vector<vec3i>& triangles, vector<vec2i>& edges) {
    auto emap = edge_map{};
    insert_edges(emap, triangles);
    get_edges(emap, edges);
}
void get_edges(const vector<vec4i>& quads, vector<vec2i>& edges) {
    auto emap = edge_map{};
    insert_edges(emap, quads);
    get_edges(emap, edges);
}

// Gets the cell index
vec3i get_cell_index(const hash_grid& grid, const vec3f& position) {
    auto scaledpos = position * grid.cell_inv_size;
    return vec3i{(int)scaledpos.x, (int)scaledpos.y, (int)scaledpos.z};
}

// Create a hash_grid
void init_hash_grid(hash_grid& grid, float cell_size) {
    grid               = hash_grid{};
    grid.cell_size     = cell_size;
    grid.cell_inv_size = 1 / cell_size;
}
void init_hash_grid(
    hash_grid& grid, const vector<vec3f>& positions, float cell_size) {
    grid               = hash_grid{};
    grid.cell_size     = cell_size;
    grid.cell_inv_size = 1 / cell_size;
    for (auto& position : positions) insert_vertex(grid, position);
}
// Inserts a point into the grid
int insert_vertex(hash_grid& grid, const vec3f& position) {
    auto vertex_id = (int)grid.positions.size();
    auto cell      = get_cell_index(grid, position);
    grid.cells[cell].push_back(vertex_id);
    grid.positions.push_back(position);
    return vertex_id;
}
// Finds the nearest neighboors within a given radius
void find_nearest_neightbors(const hash_grid& grid, vector<int>& neighboors,
    const vec3f& position, float max_radius, int skip_id) {
    auto cell        = get_cell_index(grid, position);
    auto cell_radius = (int)(max_radius * grid.cell_inv_size) + 1;
    neighboors.clear();
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
                    neighboors.push_back(vertex_id);
                }
            }
        }
    }
}
void find_nearest_neightbors(const hash_grid& grid, vector<int>& neighboors,
    const vec3f& position, float max_radius) {
    find_nearest_neightbors(grid, neighboors, position, max_radius, -1);
}
void find_nearest_neightbors(const hash_grid& grid, vector<int>& neighboors,
    int vertex_id, float max_radius) {
    find_nearest_neightbors(
        grid, neighboors, grid.positions[vertex_id], max_radius, vertex_id);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE ELEMENT CONVERSION AND GROUPING
// -----------------------------------------------------------------------------
namespace yocto {

// Convert quads to triangles
void convert_quads_to_triangles(
    vector<vec3i>& triangles, const vector<vec4i>& quads) {
    triangles.clear();
    triangles.reserve(quads.size() * 2);
    for (auto& q : quads) {
        triangles.push_back({q.x, q.y, q.w});
        if (q.z != q.w) triangles.push_back({q.z, q.w, q.y});
    }
}

// Convert quads to triangles with a diamond-like topology.
// Quads have to be consecutive one row after another.
void convert_quads_to_triangles(
    vector<vec3i>& triangles, const vector<vec4i>& quads, int row_length) {
    triangles.clear();
    triangles.reserve(quads.size() * 2);
    for (auto& q : quads) {
        triangles.push_back({q.x, q.y, q.w});
        if (q.z != q.w) triangles.push_back({q.z, q.w, q.y});
    }
    // triangles.resize(usteps * vsteps * 2);
    // for (auto j = 0; j < vsteps; j++) {
    //     for (auto i = 0; i < usteps; i++) {
    //         auto f1 = triangles[(j * usteps + i) * 2 + 0];
    //         auto f2 = triangles[(j * usteps + i) * 2 + 1];
    //         if ((i + j) % 2) {
    //             f1 = {vid(i, j), vid(i + 1, j), vid(i + 1, j + 1)};
    //             f2 = {vid(i + 1, j + 1), vid(i, j + 1), vid(i, j)};
    //         } else {
    //             f1 = {vid(i, j), vid(i + 1, j), vid(i, j + 1)};
    //             f2 = {vid(i + 1, j + 1), vid(i, j + 1), vid(i + 1, j)};
    //         }
    //     }
    // }
}

// Convert triangles to quads by creating degenerate quads
void convert_triangles_to_quads(
    vector<vec4i>& quads, const vector<vec3i>& triangles) {
    quads.clear();
    quads.reserve(triangles.size());
    for (auto& t : triangles) quads.push_back({t.x, t.y, t.z, t.z});
}

// Convert beziers to lines using 3 lines for each bezier.
void convert_bezier_to_lines(
    vector<vec2i>& lines, const vector<vec4i>& beziers) {
    lines.clear();
    lines.reserve(beziers.size() * 3);
    for (auto b : beziers) {
        lines.push_back({b.x, b.y});
        lines.push_back({b.y, b.z});
        lines.push_back({b.z, b.w});
    }
}

// Convert face varying data to single primitives. Returns the quads indices
// and filled vectors for pos, norm and texcoord.
void convert_facevarying(vector<vec4i>& split_quads,
    vector<vec3f>& split_positions, vector<vec3f>& split_normals,
    vector<vec2f>& split_texturecoords, const vector<vec4i>& quads_positions,
    const vector<vec4i>& quads_normals,
    const vector<vec4i>& quads_texturecoords, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords) {
    // make faces unique
    unordered_map<vec3i, int> vert_map;
    split_quads.resize(quads_positions.size());
    for (auto fid = 0; fid < quads_positions.size(); fid++) {
        for (auto c = 0; c < 4; c++) {
            auto v = vec3i{
                (&quads_positions[fid].x)[c],
                (!quads_normals.empty()) ? (&quads_normals[fid].x)[c] : -1,
                (!quads_texturecoords.empty())
                    ? (&quads_texturecoords[fid].x)[c]
                    : -1,
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
    split_texturecoords.clear();
    if (!texturecoords.empty()) {
        split_texturecoords.resize(vert_map.size());
        for (auto& [vert, index] : vert_map) {
            split_texturecoords[index] = texturecoords[vert.z];
        }
    }
}

// Split primitives per id
template <typename T>
void ungroup_elems(vector<vector<T>>& split_elems, const vector<T>& elems,
    const vector<int>& ids) {
    auto max_id = *max_element(ids.begin(), ids.end());
    split_elems.resize(max_id + 1);
    for (auto elem_id = 0; elem_id < elems.size(); elem_id++) {
        split_elems[ids[elem_id]].push_back(elems[elem_id]);
    }
}
void ungroup_lines(vector<vector<vec2i>>& split_lines,
    const vector<vec2i>& lines, const vector<int>& ids) {
    ungroup_elems(split_lines, lines, ids);
}
void ungroup_triangles(vector<vector<vec3i>>& split_triangles,
    const vector<vec3i>& triangles, const vector<int>& ids) {
    ungroup_elems(split_triangles, triangles, ids);
}
void ungroup_quads(vector<vector<vec4i>>& split_quads,
    const vector<vec4i>& quads, const vector<int>& ids) {
    ungroup_elems(split_quads, quads, ids);
}

// Weld vertices within a threshold.
void weld_vertices(
    vector<vec3f>& positions, vector<int>& indices, float threshold) {
    indices.resize(positions.size());
    auto welded_positions = vector<vec3f>{};
    auto grid             = hash_grid{};
    init_hash_grid(grid, threshold);
    auto neighboors = vector<int>{};
    for (auto vertex_id = 0; vertex_id < positions.size(); vertex_id++) {
        auto& position = positions[vertex_id];
        find_nearest_neightbors(grid, neighboors, position, threshold);
        if (neighboors.empty()) {
            welded_positions.push_back(position);
            indices[vertex_id] = (int)welded_positions.size() - 1;
            insert_vertex(grid, position);
        } else {
            indices[vertex_id] = neighboors.front();
        }
    }
    swap(welded_positions, positions);
    // for (auto i = 0; i < positions.size(); i++) {
    //     welded_indices[i] = (int)welded_positions.size();
    //     for (auto j = 0; j < welded_positions.size(); j++) {
    //         if (length(positions[i] - welded_positions[j]) < threshold) {
    //             welded_indices[i] = j;
    //             break;
    //         }
    //     }
    //     if (welded_indices[i] == (int)welded_positions.size())
    //         welded_positions.push_back(positions[i]);
    // }
}
void weld_triangles(
    vector<vec3i>& triangles, vector<vec3f>& positions, float threshold) {
    auto indices = vector<int>{};
    weld_vertices(positions, indices, threshold);
    for (auto& t : triangles) t = {indices[t.x], indices[t.y], indices[t.z]};
}
void weld_quads(
    vector<vec4i>& quads, vector<vec3f>& positions, float threshold) {
    auto indices = vector<int>{};
    weld_vertices(positions, indices, threshold);
    auto welded_quads = vector<vec4i>{};
    for (auto& q : quads)
        q = {
            indices[q.x],
            indices[q.y],
            indices[q.z],
            indices[q.w],
        };
}

// Merge shape elements
void merge_lines(
    vector<vec2i>& lines, const vector<vec2i>& merge_lines, int num_verts) {
    for (auto& l : merge_lines)
        lines.push_back({l.x + num_verts, l.y + num_verts});
}
void merge_triangles(vector<vec3i>& triangles,
    const vector<vec3i>& merge_triangles, int num_verts) {
    for (auto& t : merge_triangles)
        triangles.push_back(
            {t.x + num_verts, t.y + num_verts, t.z + num_verts});
}
void merge_quads(
    vector<vec4i>& quads, const vector<vec4i>& merge_quads, int num_verts) {
    for (auto& q : merge_quads)
        quads.push_back({q.x + num_verts, q.y + num_verts, q.z + num_verts,
            q.w + num_verts});
}
void merge_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& tangents, vector<vec2f>& texturecoords,
    vector<float>& radius, const vector<vec2i>& merge_lines,
    const vector<vec3f>& merge_positions, const vector<vec3f>& merge_tangents,
    const vector<vec2f>& merge_texturecoords,
    const vector<float>& merge_radius) {
    auto merge_verts = (int)positions.size();
    for (auto& l : merge_lines)
        lines.push_back({l.x + merge_verts, l.y + merge_verts});
    positions.insert(
        positions.end(), merge_positions.begin(), merge_positions.end());
    tangents.insert(
        tangents.end(), merge_tangents.begin(), merge_tangents.end());
    texturecoords.insert(texturecoords.end(), merge_texturecoords.begin(),
        merge_texturecoords.end());
    radius.insert(radius.end(), merge_radius.begin(), merge_radius.end());
}
void merge_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
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
    texturecoords.insert(texturecoords.end(), merge_texturecoords.begin(),
        merge_texturecoords.end());
}
void merge_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
    const vector<vec4i>& merge_quads, const vector<vec3f>& merge_positions,
    const vector<vec3f>& merge_normals,
    const vector<vec2f>& merge_texturecoords) {
    auto merge_verts = (int)positions.size();
    for (auto& q : merge_quads)
        quads.push_back({q.x + merge_verts, q.y + merge_verts,
            q.z + merge_verts, q.w + merge_verts});
    positions.insert(
        positions.end(), merge_positions.begin(), merge_positions.end());
    normals.insert(normals.end(), merge_normals.begin(), merge_normals.end());
    texturecoords.insert(texturecoords.end(), merge_texturecoords.begin(),
        merge_texturecoords.end());
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE SUBDIVISION
// -----------------------------------------------------------------------------
namespace yocto {

// Subdivide lines.
template <typename T>
void subdivide_lines_impl(vector<vec2i>& lines, vector<T>& vert) {
    // early exit
    if (lines.empty() || vert.empty()) return;
    // sizes
    auto nverts = (int)vert.size();
    auto nlines = (int)lines.size();
    // create vertices
    auto tvert = vector<T>(nverts + nlines);
    for (auto i = 0; i < nverts; i++) tvert[i] = vert[i];
    for (auto i = 0; i < nlines; i++) {
        auto l            = lines[i];
        tvert[nverts + i] = (vert[l.x] + vert[l.y]) / 2;
    }
    // create lines
    auto tlines = vector<vec2i>(nlines * 2);
    for (auto i = 0; i < nlines; i++) {
        auto l            = lines[i];
        tlines[i * 2 + 0] = {l.x, nverts + i};
        tlines[i * 2 + 0] = {nverts + i, l.y};
    }
    swap(tlines, lines);
    swap(tvert, vert);
}

void subdivide_lines(vector<vec2i>& lines, vector<float>& vert) {
    return subdivide_lines_impl(lines, vert);
}
void subdivide_lines(vector<vec2i>& lines, vector<vec2f>& vert) {
    return subdivide_lines_impl(lines, vert);
}
void subdivide_lines(vector<vec2i>& lines, vector<vec3f>& vert) {
    return subdivide_lines_impl(lines, vert);
}
void subdivide_lines(vector<vec2i>& lines, vector<vec4f>& vert) {
    return subdivide_lines_impl(lines, vert);
}

// Subdivide triangle.
template <typename T>
void subdivide_triangles_impl(vector<vec3i>& triangles, vector<T>& vert) {
    // early exit
    if (triangles.empty() || vert.empty()) return;
    // get edges
    auto emap = edge_map{};
    insert_edges(emap, triangles);
    auto edges = vector<vec2i>{};
    get_edges(emap, edges);
    // number of elements
    auto nverts = (int)vert.size();
    auto nedges = (int)edges.size();
    auto nfaces = (int)triangles.size();
    // create vertices
    auto tvert = vector<T>(nverts + nedges);
    for (auto i = 0; i < nverts; i++) tvert[i] = vert[i];
    for (auto i = 0; i < nedges; i++) {
        auto e            = edges[i];
        tvert[nverts + i] = (vert[e.x] + vert[e.y]) / 2;
    }
    // create triangles
    auto ttriangles = vector<vec3i>(nfaces * 4);
    for (auto i = 0; i < nfaces; i++) {
        auto t                = triangles[i];
        ttriangles[i * 4 + 0] = {t.x, nverts + get_edge_index(emap, {t.x, t.y}),
            nverts + get_edge_index(emap, {t.z, t.x})};
        ttriangles[i * 4 + 1] = {t.y, nverts + get_edge_index(emap, {t.y, t.z}),
            nverts + get_edge_index(emap, {t.x, t.y})};
        ttriangles[i * 4 + 2] = {t.z, nverts + get_edge_index(emap, {t.z, t.x}),
            nverts + get_edge_index(emap, {t.y, t.z})};
        ttriangles[i * 4 + 3] = {nverts + get_edge_index(emap, {t.x, t.y}),
            nverts + get_edge_index(emap, {t.y, t.z}),
            nverts + get_edge_index(emap, {t.z, t.x})};
    }
    swap(ttriangles, triangles);
    swap(tvert, vert);
}

void subdivide_triangles(vector<vec3i>& triangles, vector<float>& vert) {
    subdivide_triangles_impl(triangles, vert);
}
void subdivide_triangles(vector<vec3i>& triangles, vector<vec2f>& vert) {
    subdivide_triangles_impl(triangles, vert);
}
void subdivide_triangles(vector<vec3i>& triangles, vector<vec3f>& vert) {
    subdivide_triangles_impl(triangles, vert);
}
void subdivide_triangles(vector<vec3i>& triangles, vector<vec4f>& vert) {
    subdivide_triangles_impl(triangles, vert);
}

// Subdivide quads.
template <typename T>
void subdivide_quads_impl(vector<vec4i>& quads, vector<T>& vert) {
    // early exit
    if (quads.empty() || vert.empty()) return;
    // get edges
    auto emap = edge_map{};
    insert_edges(emap, quads);
    auto edges = vector<vec2i>{};
    get_edges(emap, edges);
    // number of elements
    auto nverts = (int)vert.size();
    auto nedges = (int)edges.size();
    auto nfaces = (int)quads.size();
    // create vertices
    auto tvert = vector<T>(nverts + nedges + nfaces);
    for (auto i = 0; i < nverts; i++) tvert[i] = vert[i];
    for (auto i = 0; i < nedges; i++) {
        auto e            = edges[i];
        tvert[nverts + i] = (vert[e.x] + vert[e.y]) / 2;
    }
    for (auto i = 0; i < nfaces; i++) {
        auto q = quads[i];
        if (q.z != q.w) {
            tvert[nverts + nedges + i] =
                (vert[q.x] + vert[q.y] + vert[q.z] + vert[q.w]) / 4;
        } else {
            tvert[nverts + nedges + i] = (vert[q.x] + vert[q.y] + vert[q.y]) /
                                         3;
        }
    }
    // create quads
    auto tquads = vector<vec4i>(nfaces * 4);  // conservative allocation
    auto qi     = 0;
    for (auto i = 0; i < nfaces; i++) {
        auto q = quads[i];
        if (q.z != q.w) {
            tquads[qi++] = {q.x, nverts + get_edge_index(emap, {q.x, q.y}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.w, q.x})};
            tquads[qi++] = {q.y, nverts + get_edge_index(emap, {q.y, q.z}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.x, q.y})};
            tquads[qi++] = {q.z, nverts + get_edge_index(emap, {q.z, q.w}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.y, q.z})};
            tquads[qi++] = {q.w, nverts + get_edge_index(emap, {q.w, q.x}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.z, q.w})};
        } else {
            tquads[qi++] = {q.x, nverts + get_edge_index(emap, {q.x, q.y}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.z, q.x})};
            tquads[qi++] = {q.y, nverts + get_edge_index(emap, {q.y, q.z}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.x, q.y})};
            tquads[qi++] = {q.z, nverts + get_edge_index(emap, {q.z, q.x}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.y, q.z})};
        }
    }
    tquads.resize(qi);
    // done
    swap(tquads, quads);
    swap(tvert, vert);
}

void subdivide_quads(vector<vec4i>& quads, vector<float>& vert) {
    subdivide_quads_impl(quads, vert);
}
void subdivide_quads(vector<vec4i>& quads, vector<vec2f>& vert) {
    subdivide_quads_impl(quads, vert);
}
void subdivide_quads(vector<vec4i>& quads, vector<vec3f>& vert) {
    subdivide_quads_impl(quads, vert);
}
void subdivide_quads(vector<vec4i>& quads, vector<vec4f>& vert) {
    subdivide_quads_impl(quads, vert);
}

// Subdivide beziers.
template <typename T>
void subdivide_beziers_impl(vector<vec4i>& beziers, vector<T>& vert) {
    // early exit
    if (beziers.empty() || vert.empty()) return;
    // get edges
    auto vmap     = unordered_map<int, int>();
    auto tvert    = vector<T>();
    auto tbeziers = vector<vec4i>();
    for (auto b : beziers) {
        if (vmap.find(b.x) == vmap.end()) {
            vmap[b.x] = (int)tvert.size();
            tvert.push_back(vert[b.x]);
        }
        if (vmap.find(b.w) == vmap.end()) {
            vmap[b.w] = (int)tvert.size();
            tvert.push_back(vert[b.w]);
        }
        auto bo = (int)tvert.size();
        tbeziers.push_back({vmap.at(b.x), bo + 0, bo + 1, bo + 2});
        tbeziers.push_back({bo + 2, bo + 3, bo + 4, vmap.at(b.w)});
        tvert.push_back(vert[b.x] / 2 + vert[b.y] / 2);
        tvert.push_back(vert[b.x] / 4 + vert[b.y] / 2 + vert[b.z] / 4);
        tvert.push_back(vert[b.x] / 8 + 3 * vert[b.y] / 8 + 3 * vert[b.z] / 8 +
                        vert[b.w] / 8);
        tvert.push_back(vert[b.y] / 4 + vert[b.z] / 2 + vert[b.w] / 4);
        tvert.push_back(vert[b.z] / 2 + vert[b.w] / 2);
    }

    // done
    swap(tbeziers, beziers);
    swap(tvert, vert);
}

void subdivide_beziers(vector<vec4i>& beziers, vector<float>& vert) {
    subdivide_beziers_impl(beziers, vert);
}
void subdivide_beziers(vector<vec4i>& beziers, vector<vec2f>& vert) {
    subdivide_beziers_impl(beziers, vert);
}
void subdivide_beziers(vector<vec4i>& beziers, vector<vec3f>& vert) {
    subdivide_beziers_impl(beziers, vert);
}
void subdivide_beziers(vector<vec4i>& beziers, vector<vec4f>& vert) {
    subdivide_beziers_impl(beziers, vert);
}

// Subdivide catmullclark.
template <typename T>
void subdivide_catmullclark_impl(
    vector<vec4i>& quads, vector<T>& vert, bool lock_boundary) {
    // early exit
    if (quads.empty() || vert.empty()) return;
    // get edges
    auto emap = edge_map{};
    insert_edges(emap, quads);
    auto edges = vector<vec2i>{}, boundary = vector<vec2i>{};
    get_edges(emap, edges);
    get_boundary(emap, boundary);
    // number of elements
    auto nverts    = (int)vert.size();
    auto nedges    = (int)edges.size();
    auto nboundary = (int)boundary.size();
    auto nfaces    = (int)quads.size();

    // split elements ------------------------------------
    // create vertices
    auto tvert = vector<T>(nverts + nedges + nfaces);
    for (auto i = 0; i < nverts; i++) tvert[i] = vert[i];
    for (auto i = 0; i < nedges; i++) {
        auto e            = edges[i];
        tvert[nverts + i] = (vert[e.x] + vert[e.y]) / 2;
    }
    for (auto i = 0; i < nfaces; i++) {
        auto q = quads[i];
        if (q.z != q.w) {
            tvert[nverts + nedges + i] =
                (vert[q.x] + vert[q.y] + vert[q.z] + vert[q.w]) / 4;
        } else {
            tvert[nverts + nedges + i] = (vert[q.x] + vert[q.y] + vert[q.y]) /
                                         3;
        }
    }
    // create quads
    auto tquads = vector<vec4i>(nfaces * 4);  // conservative allocation
    auto qi     = 0;
    for (auto i = 0; i < nfaces; i++) {
        auto q = quads[i];
        if (q.z != q.w) {
            tquads[qi++] = {q.x, nverts + get_edge_index(emap, {q.x, q.y}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.w, q.x})};
            tquads[qi++] = {q.y, nverts + get_edge_index(emap, {q.y, q.z}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.x, q.y})};
            tquads[qi++] = {q.z, nverts + get_edge_index(emap, {q.z, q.w}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.y, q.z})};
            tquads[qi++] = {q.w, nverts + get_edge_index(emap, {q.w, q.x}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.z, q.w})};
        } else {
            tquads[qi++] = {q.x, nverts + get_edge_index(emap, {q.x, q.y}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.z, q.x})};
            tquads[qi++] = {q.y, nverts + get_edge_index(emap, {q.y, q.z}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.x, q.y})};
            tquads[qi++] = {q.z, nverts + get_edge_index(emap, {q.z, q.x}),
                nverts + nedges + i, nverts + get_edge_index(emap, {q.y, q.z})};
        }
    }
    tquads.resize(qi);

    // split boundary
    auto tboundary = vector<vec2i>(nboundary * 2);
    for (auto i = 0; i < nboundary; i++) {
        auto e = boundary[i];
        tboundary.push_back({e.x, nverts + get_edge_index(emap, e)});
        tboundary.push_back({nverts + get_edge_index(emap, e), e.y});
    }

    // setup creases -----------------------------------
    auto tcrease_edges = vector<vec2i>();
    auto tcrease_verts = vector<int>();
    if (lock_boundary) {
        for (auto& b : tboundary) {
            tcrease_verts.push_back(b.x);
            tcrease_verts.push_back(b.y);
        }
    } else {
        for (auto& b : tboundary) tcrease_edges.push_back(b);
    }

    // define vertex valence ---------------------------
    auto tvert_val = vector<int>(tvert.size(), 2);
    for (auto& e : tboundary) {
        tvert_val[e.x] = (lock_boundary) ? 0 : 1;
        tvert_val[e.y] = (lock_boundary) ? 0 : 1;
    }

    // averaging pass ----------------------------------
    auto avert  = vector<T>(tvert.size(), T());
    auto acount = vector<int>(tvert.size(), 0);
    for (auto p : tcrease_verts) {
        if (tvert_val[p] != 0) continue;
        avert[p] += tvert[p];
        acount[p] += 1;
    }
    for (auto& e : tcrease_edges) {
        auto c = (tvert[e.x] + tvert[e.y]) / 2.0f;
        for (auto vid : {e.x, e.y}) {
            if (tvert_val[vid] != 1) continue;
            avert[vid] += c;
            acount[vid] += 1;
        }
    }
    for (auto& q : tquads) {
        auto c = (tvert[q.x] + tvert[q.y] + tvert[q.z] + tvert[q.w]) / 4.0f;
        for (auto vid : {q.x, q.y, q.z, q.w}) {
            if (tvert_val[vid] != 2) continue;
            avert[vid] += c;
            acount[vid] += 1;
        }
    }
    for (auto i = 0; i < tvert.size(); i++) avert[i] /= (float)acount[i];

    // correction pass ----------------------------------
    // p = p + (avg_p - p) * (4/avg_count)
    for (auto i = 0; i < tvert.size(); i++) {
        if (tvert_val[i] != 2) continue;
        avert[i] = tvert[i] + (avert[i] - tvert[i]) * (4.0f / acount[i]);
    }
    tvert = avert;

    // done
    swap(tquads, quads);
    swap(tvert, vert);
}

void subdivide_catmullclark(
    vector<vec4i>& quads, vector<float>& vert, bool lock_boundary) {
    subdivide_catmullclark_impl(quads, vert, lock_boundary);
}
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec2f>& vert, bool lock_boundary) {
    subdivide_catmullclark_impl(quads, vert, lock_boundary);
}
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec3f>& vert, bool lock_boundary) {
    subdivide_catmullclark_impl(quads, vert, lock_boundary);
}
void subdivide_catmullclark(
    vector<vec4i>& quads, vector<vec4f>& vert, bool lock_boundary) {
    subdivide_catmullclark_impl(quads, vert, lock_boundary);
}

template <typename T, typename SubdivideFunc>
void subdivide_elems_impl(vector<T>& elems, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius, const SubdivideFunc&& subdivied_func) {
    if (!normals.empty()) {
        auto elems_ = elems;
        subdivied_func(elems_, normals);
    }
    if (!texturecoords.empty()) {
        auto elems_ = elems;
        subdivied_func(elems_, texturecoords);
    }
    if (!colors.empty()) {
        auto elems_ = elems;
        subdivied_func(elems_, colors);
    }
    if (!radius.empty()) {
        auto elems_ = elems;
        subdivied_func(elems_, radius);
    }
    subdivied_func(elems, positions);
}

void subdivide_lines(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius) {
    subdivide_elems_impl(lines, positions, normals, texturecoords, colors,
        radius, [](auto& lines, auto& vert) { subdivide_lines(lines, vert); });
}
void subdivide_triangles(vector<vec3i>& triangles, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius) {
    subdivide_elems_impl(triangles, positions, normals, texturecoords, colors,
        radius, [](auto& triangles, auto& vert) {
            subdivide_triangles(triangles, vert);
        });
}
void subdivide_quads(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius) {
    subdivide_elems_impl(quads, positions, normals, texturecoords, colors,
        radius, [](auto& quads, auto& vert) { subdivide_quads(quads, vert); });
}
void subdivide_beziers(vector<vec4i>& beziers, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius) {
    subdivide_elems_impl(beziers, positions, normals, texturecoords, colors,
        radius,
        [](auto& beziers, auto& vert) { subdivide_beziers(beziers, vert); });
}
void subdivide_catmullclark(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<vec4f>& colors,
    vector<float>& radius) {
    subdivide_elems_impl(quads, positions, normals, texturecoords, colors,
        radius,
        [](auto& quads, auto& vert) { subdivide_catmullclark(quads, vert); });
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE SAMPLING
// -----------------------------------------------------------------------------
namespace yocto {

// Pick a point in a point set uniformly.
int sample_points_element(int npoints, float re) {
    return sample_uniform_index(npoints, re);
}
void sample_points_element_cdf(vector<float>& cdf, int npoints) {
    cdf.resize(npoints);
    for (auto i = 0; i < cdf.size(); i++) cdf[i] = 1 + (i ? cdf[i - 1] : 0);
}
int sample_points_element(const vector<float>& cdf, float re) {
    return sample_discrete_distribution(cdf, re);
}

// Pick a point on lines uniformly.
void sample_lines_element_cdf(vector<float>& cdf, const vector<vec2i>& lines,
    const vector<vec3f>& positions) {
    cdf.resize(lines.size());
    for (auto i = 0; i < cdf.size(); i++) {
        auto l = lines[i];
        auto w = line_length(positions[l.x], positions[l.y]);
        cdf[i] = w + (i ? cdf[i - 1] : 0);
    }
}
pair<int, float> sample_lines_element(
    const vector<float>& cdf, float re, float ru) {
    return {sample_discrete_distribution(cdf, re), ru};
}

// Pick a point on a triangle mesh uniformly.
void sample_triangles_element_cdf(vector<float>& cdf,
    const vector<vec3i>& triangles, const vector<vec3f>& positions) {
    cdf.resize(triangles.size());
    for (auto i = 0; i < cdf.size(); i++) {
        auto t = triangles[i];
        auto w = triangle_area(positions[t.x], positions[t.y], positions[t.z]);
        cdf[i] = w + (i ? cdf[i - 1] : 0);
    }
}
pair<int, vec2f> sample_triangles_element(
    const vector<float>& cdf, float re, const vec2f& ruv) {
    return {sample_discrete_distribution(cdf, re),
        sample_triangle_coordinates(ruv)};
}

// Pick a point on a quad mesh uniformly.
void sample_quads_element_cdf(vector<float>& cdf, const vector<vec4i>& quads,
    const vector<vec3f>& positions) {
    cdf.resize(quads.size());
    for (auto i = 0; i < cdf.size(); i++) {
        auto q = quads[i];
        auto w = quad_area(
            positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
        cdf[i] = w + (i ? cdf[i - 1] : 0);
    }
}
pair<int, vec2f> sample_quads_element(
    const vector<float>& cdf, float re, const vec2f& ruv) {
    return {sample_discrete_distribution(cdf, re), ruv};
}
pair<int, vec2f> sample_quads_element(const vector<vec4i>& quads,
    const vector<float>& cdf, float re, const vec2f& ruv) {
    auto element_id = sample_discrete_distribution(cdf, re);
    if (quads[element_id].z == quads[element_id].w) {
        return {element_id, sample_triangle_coordinates(ruv)};
    } else {
        return {element_id, ruv};
    }
}

// Samples a set of points over a triangle mesh uniformly. The rng function
// takes the point index and returns vec3f numbers uniform directibuted in
// [0,1]^3. unorm and texcoord are optional.
void sample_triangles_points(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texturecoords,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    int npoints, int seed) {
    sampled_positions.resize(npoints);
    sampled_normals.resize(npoints);
    sampled_texturecoords.resize(npoints);
    auto cdf = vector<float>{};
    sample_triangles_element_cdf(cdf, triangles, positions);
    auto rng = make_rng(seed);
    for (auto i = 0; i < npoints; i++) {
        auto [triangle_id, triangle_uv] = sample_triangles_element(cdf,
            get_random_float(rng),
            {get_random_float(rng), get_random_float(rng)});
        auto t                          = triangles[triangle_id];
        sampled_positions[i]            = interpolate_triangle(
            positions[t.x], positions[t.y], positions[t.z], triangle_uv);
        if (!sampled_normals.empty()) {
            sampled_normals[i] = normalize(interpolate_triangle(
                normals[t.x], normals[t.y], normals[t.z], triangle_uv));
        } else {
            sampled_normals[i] = triangle_normal(
                positions[t.x], positions[t.y], positions[t.z]);
        }
        if (!sampled_texturecoords.empty()) {
            sampled_texturecoords[i] = interpolate_triangle(texturecoords[t.x],
                texturecoords[t.y], texturecoords[t.z], triangle_uv);
        } else {
            sampled_texturecoords[i] = zero2f;
        }
    }
}

// Samples a set of points over a triangle mesh uniformly. The rng function
// takes the point index and returns vec3f numbers uniform directibuted in
// [0,1]^3. unorm and texcoord are optional.
void sample_quads_points(vector<vec3f>& sampled_positions,
    vector<vec3f>& sampled_normals, vector<vec2f>& sampled_texturecoords,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    int npoints, int seed) {
    sampled_positions.resize(npoints);
    sampled_normals.resize(npoints);
    sampled_texturecoords.resize(npoints);
    auto cdf = vector<float>{};
    sample_quads_element_cdf(cdf, quads, positions);
    auto rng = make_rng(seed);
    for (auto i = 0; i < npoints; i++) {
        auto [quad_id, quad_uv] = sample_quads_element(cdf,
            get_random_float(rng),
            {get_random_float(rng), get_random_float(rng)});
        auto q                  = quads[quad_id];
        sampled_positions[i] = interpolate_quad(positions[q.x], positions[q.y],
            positions[q.z], positions[q.w], quad_uv);
        if (!sampled_normals.empty()) {
            sampled_normals[i] = normalize(interpolate_quad(normals[q.x],
                normals[q.y], normals[q.z], normals[q.w], quad_uv));
        } else {
            sampled_normals[i] = quad_normal(
                positions[q.x], positions[q.y], positions[q.z], positions[q.w]);
        }
        if (!sampled_texturecoords.empty()) {
            sampled_texturecoords[i] = interpolate_quad(texturecoords[q.x],
                texturecoords[q.y], texturecoords[q.z], texturecoords[q.w],
                quad_uv);
        } else {
            sampled_texturecoords[i] = zero2f;
        }
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// SHAPE GEODESICS
// -----------------------------------------------------------------------------
namespace yocto {

void add_node(geodesic_solver& solver, const vec3f& position) {
    solver.positions.push_back(position);
    solver.graph.push_back({});
    solver.graph.back().reserve(8);
}

inline void add_directed_arc(geodesic_solver& solver, int from, int to) {
    assert(from >= 0 and from < solver.graph.size());
    assert(to >= 0 and to < solver.graph.size());

    float len = length(solver.positions[from] - solver.positions[to]);
    solver.graph[from].push_back({to, len});
}

inline void add_undirected_arc(geodesic_solver& solver, int na, int nb) {
    add_directed_arc(solver, na, nb);
    add_directed_arc(solver, nb, na);
}

void make_edge_solver_slow(geodesic_solver& solver,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    bool use_steiner_points) {
    solver.graph.reserve(positions.size());

    for (int i = 0; i < positions.size(); i++) add_node(solver, positions[i]);

    auto emap = edge_map{};
    insert_edges(emap, triangles);
    auto edges = vector<vec2i>{};
    get_edges(emap, edges);
    for (auto& edge : edges) {
        add_undirected_arc(solver, edge.x, edge.y);
    }

    if (!use_steiner_points) return;

    solver.graph.reserve(size(positions) + size(edges));
    auto steiner_per_edge = vector<int>(get_num_edges(emap));

    // On each edge, connect the mid vertex with the vertices on th same edge.
    for (auto edge_index = 0; edge_index < size(edges); edge_index++) {
        auto& edge                   = edges[edge_index];
        auto  steiner_idx            = solver.graph.size();
        steiner_per_edge[edge_index] = steiner_idx;
        add_node(solver, (positions[edge.x] + positions[edge.y]) * 0.5f);
        add_directed_arc(solver, steiner_idx, edge.x);
        add_directed_arc(solver, steiner_idx, edge.y);
    }

    // Make connection for each face
    for (int face = 0; face < triangles.size(); ++face) {
        int steiner_idx[3];
        for (int k : {0, 1, 2}) {
            int a          = triangles[face][k];
            int b          = triangles[face][(k + 1) % 3];
            steiner_idx[k] = steiner_per_edge[get_edge_index(emap, {a, b})];
        }

        // Connect each mid-vertex to the opposite mesh vertex in the triangle
        int opp[3] = {triangles[face].z, triangles[face].x, triangles[face].y};
        add_undirected_arc(solver, steiner_idx[0], opp[0]);
        add_undirected_arc(solver, steiner_idx[1], opp[1]);
        add_undirected_arc(solver, steiner_idx[2], opp[2]);

        // Connect mid-verts of the face between them
        add_undirected_arc(solver, steiner_idx[0], steiner_idx[1]);
        add_undirected_arc(solver, steiner_idx[0], steiner_idx[2]);
        add_undirected_arc(solver, steiner_idx[1], steiner_idx[2]);
    }
}

inline void add_half_edge(geodesic_solver& solver, const vec2i& edge) {
    // check if edge exists already
    for (auto [vert, _] : solver.graph[edge.x]) {
        if (vert == edge.y) return;
    }
    auto len = length(solver.positions[edge.x] - solver.positions[edge.y]);
    auto edge_index = (int)solver.edges.size();
    solver.graph[edge.x].push_back({edge.y, len});
    solver.edge_index[edge.x].push_back({edge.y, edge_index});
    solver.edges.push_back(edge);
}

inline void add_edge(geodesic_solver& solver, const vec2i& edge) {
    // check if edge exists already
    for (auto [vert, _] : solver.graph[edge.x]) {
        if (vert == edge.y) return;
    }
    auto len = length(solver.positions[edge.x] - solver.positions[edge.y]);
    solver.graph[edge.x].push_back({edge.y, len});
    solver.graph[edge.y].push_back({edge.x, len});
    auto edge_index = (int)solver.edges.size();
    solver.edge_index[edge.x].push_back({edge.y, edge_index});
    solver.edge_index[edge.y].push_back({edge.x, edge_index});
    solver.edges.push_back(edge);
}

inline int get_edge_index(const geodesic_solver& solver, const vec2i& edge) {
    for (auto [node, index] : solver.edge_index[edge.x])
        if (edge.y == node) return index;
    return -1;
}

void make_edge_solver_fast(geodesic_solver& solver,
    const vector<vec3i>& triangles, const vector<vec3f>& positions,
    bool use_steiner_points) {
    solver.positions = positions;
    solver.graph.resize(size(positions));
    solver.edge_index.resize(size(positions));

    // fast construction assuming edges are not repeated
    for (auto t : triangles) {
        add_edge(solver, {t.x, t.y});
        add_edge(solver, {t.y, t.z});
        add_edge(solver, {t.z, t.x});
    }

    if (!use_steiner_points) return;

    auto edges = solver.edges;
    solver.graph.resize(size(positions) + size(edges));
    solver.edge_index.resize(size(positions) + size(edges));
    solver.positions.resize(size(positions) + size(edges));
    auto steiner_per_edge = vector<int>(size(edges));

    // On each edge, connect the mid vertex with the vertices on th same edge.
    auto edge_offset = (int)positions.size();
    for (auto edge_index = 0; edge_index < size(edges); edge_index++) {
        auto& edge                   = edges[edge_index];
        auto  steiner_idx            = edge_offset + edge_index;
        steiner_per_edge[edge_index] = steiner_idx;
        solver.positions[steiner_idx] =
            (positions[edge.x] + positions[edge.y]) * 0.5f;
        add_half_edge(solver, {steiner_idx, edge.x});
        add_half_edge(solver, {steiner_idx, edge.y});
    }

    // Make connection for each face
    for (int face = 0; face < triangles.size(); ++face) {
        int steiner_idx[3];
        for (int k : {0, 1, 2}) {
            int a          = triangles[face][k];
            int b          = triangles[face][(k + 1) % 3];
            steiner_idx[k] = steiner_per_edge[get_edge_index(solver, {a, b})];
        }

        // Connect each mid-vertex to the opposite mesh vertex in the triangle
        int opp[3] = {triangles[face].z, triangles[face].x, triangles[face].y};
        add_edge(solver, {steiner_idx[0], opp[0]});
        add_edge(solver, {steiner_idx[1], opp[1]});
        add_edge(solver, {steiner_idx[2], opp[2]});

        // Connect mid-verts of the face between them
        add_edge(solver, {steiner_idx[0], steiner_idx[1]});
        add_edge(solver, {steiner_idx[1], steiner_idx[2]});
        add_edge(solver, {steiner_idx[2], steiner_idx[0]});
    }
}

void log_geodesic_solver_stats(const geodesic_solver& solver) {
    // stats
    auto num_edges     = 0;
    auto min_adjacents = int_max, max_adjacents = int_min;
    auto min_length = float_max, max_length = float_min;
    auto avg_adjacents = 0.0, avg_length = 0.0;
    for (auto& adj : solver.graph) {
        num_edges += (int)adj.size();
        min_adjacents = min(min_adjacents, (int)adj.size());
        max_adjacents = max(max_adjacents, (int)adj.size());
        avg_adjacents += adj.size() / (double)solver.graph.size();
        for (auto& edge : adj) {
            min_length = min(min_length, edge.length);
            max_length = max(max_length, edge.length);
            avg_length += edge.length;
        }
    }
    avg_length /= num_edges;
}

void update_edge_distances(geodesic_solver& solver) {
    for (auto node = 0; node < solver.graph.size(); node++) {
        for (auto& edge : solver.graph[node])
            edge.length = length(
                solver.positions[node] - solver.positions[edge.node]);
    }
}

void init_geodesic_solver(geodesic_solver& solver,
    const vector<vec3i>& triangles, const vector<vec3f>& positions) {
    make_edge_solver_fast(solver, triangles, positions, true);
    // auto solver = make_edge_solver_slow(triangles, positions, true);
    log_geodesic_solver_stats(solver);
}

void compute_geodesic_distances(geodesic_solver& graph,
    vector<float>& distances, const vector<int>& sources) {
    // preallocated
    distances.resize(graph.positions.size());
    for (auto& d : distances) d = float_max;

    // Small Label Fisrt + Large Label Last
    // https://en.wikipedia.org/wiki/Shortest_Path_Faster_Algorithm
    auto num_nodes = (int)graph.graph.size();
    assert(distances.size() == num_nodes);
    auto visited = vector<bool>(num_nodes, false);

    // setup queue
    auto queue = deque<int>{};
    for (auto source : sources) {
        distances[source] = 0.0f;
        visited[source]   = true;
        queue.push_back(source);
    }

    // Cumulative weights of elements in queue.
    auto cumulative_weight = 0.0f;
    while (!queue.empty()) {
        auto node           = queue.front();
        auto average_weight = (double)cumulative_weight / queue.size();

        // Large Label Last: until front node weights more than the average, put
        // it on back. Sometimes average_weight is less than envery value due to
        // floating point errors  (doesn't happen with double precision).
        for (auto tries = 0; tries < queue.size() + 1; tries++) {
            if (distances[node] <= average_weight) break;
            queue.pop_front();
            queue.push_back(node);
            node = queue.front();
        }
        queue.pop_front();
        visited[node] = false;                 // out of queue
        cumulative_weight -= distances[node];  // update average

        const auto offset_distance = distances[node];
        const auto num_neighbors   = (int)graph.graph[node].size();

        for (int neighbor_idx = 0; neighbor_idx < num_neighbors;
             neighbor_idx++) {
            // distance and id to neightbor through this node
            auto new_distance = offset_distance +
                                graph.graph[node][neighbor_idx].length;
            auto neighbor = graph.graph[node][neighbor_idx].node;

            auto old_distance = distances[neighbor];
            if (new_distance >= old_distance) continue;

            if (visited[neighbor]) {
                // if neighbor already in queue, update cumulative weights.
                cumulative_weight = cumulative_weight - old_distance +
                                    new_distance;
            } else {
                // if neighbor not in queue, Small Label first.
                if (queue.empty() || (new_distance < distances[queue.front()]))
                    queue.push_front(neighbor);
                else
                    queue.push_back(neighbor);

                visited[neighbor] = true;
                cumulative_weight = cumulative_weight + new_distance;
            }

            distances[neighbor] = new_distance;
        }
    }
}

void convert_distance_to_color(
    vector<vec4f>& colors, const vector<float>& distances) {
    colors.resize(distances.size());
    for (auto idx = 0; idx < distances.size(); idx++) {
        auto distance = fmod(distances[idx] * 10, 1.0f);
        colors[idx]   = {distance, distance, distance, 1};
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF SHAPE EXAMPLES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a quad.
void make_quad_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize) {
    positions.resize((steps.x + 1) * (steps.y + 1));
    normals.resize((steps.x + 1) * (steps.y + 1));
    texturecoords.resize((steps.x + 1) * (steps.y + 1));
    for (auto j = 0; j <= steps.y; j++) {
        for (auto i = 0; i <= steps.x; i++) {
            auto uv = vec2f{i / (float)steps.x, j / (float)steps.y};
            positions[j * (steps.x + 1) + i] = {
                (uv.x - 0.5f) * size.x, (uv.y - 0.5f) * size.y, 0};
            normals[j * (steps.x + 1) + i]       = {0, 0, 1};
            texturecoords[j * (steps.x + 1) + i] = uv * uvsize;
        }
    }

    for (auto& uv : texturecoords) uv.y = 1 - uv.y;

    quads.resize(steps.x * steps.y);
    for (auto j = 0; j < steps.y; j++) {
        for (auto i = 0; i < steps.x; i++) {
            quads[j * steps.x + i] = {j * (steps.x + 1) + i,
                j * (steps.x + 1) + i + 1, (j + 1) * (steps.x + 1) + i + 1,
                (j + 1) * (steps.x + 1) + i};
        }
    }
    // shape.triangles.resize(steps.x * steps.y * 2);
    // for (auto j = 0; j < steps.y; j++) {
    //     for (auto i = 0; i < steps.x; i++) {
    //         shape.triangles[(j * steps.x + i) * 2 + 0] = {
    //             j * (steps.x + 1) + i, j * (steps.x + 1) + i + 1,
    //             (j + 1) * (steps.x + 1) + i + 1};
    //         shape.triangles[(j * steps.x + i) * 2 + 1] = {
    //             j * (steps.x + 1) + i, (j + 1) * (steps.x + 1) + i + 1,
    //             (j + 1) * (steps.x + 1) + i};
    //     }
    // }
}

void make_floor_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize) {
    make_quad_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    for (auto& p : positions) p = {p.x, p.z, p.y};
    for (auto& normal : normals) normal = {normal.x, normal.z, normal.y};
}

// Make a rounded cube.
void make_floor_bent_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize, float radius) {
    make_floor_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    auto start = (size.y / 2 - radius) / 2;
    auto end   = start + radius;
    for (auto i = 0; i < positions.size(); i++) {
        if (positions[i].z < -end) {
            positions[i] = {
                positions[i].x, -positions[i].z - end + radius, -end};
            normals[i] = {0, 0, 1};
        } else if (positions[i].z < -start && positions[i].z >= -end) {
            auto phi     = (pif / 2) * (-positions[i].z - start) / radius;
            positions[i] = {positions[i].x, -cos(phi) * radius + radius,
                -sin(phi) * radius - start};
            normals[i]   = {0, cos(phi), sin(phi)};
        } else {
        }
    }
}

// Make a stack of quads
void make_quad_stack_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec2f& uvsize) {
    quads.clear();
    positions.clear();
    normals.clear();
    texturecoords.clear();
    auto qquads         = vector<vec4i>{};
    auto qpositions     = vector<vec3f>{};
    auto qnormals       = vector<vec3f>{};
    auto qtexturecoords = vector<vec2f>{};
    for (auto i = 0; i <= steps.z; i++) {
        make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
            {steps.x, steps.y}, {size.x, size.y}, uvsize);
        for (auto& p : qpositions) p.z = (-0.5f + (float)i / steps.z) * size.z;
        merge_quads(quads, positions, normals, texturecoords, qquads,
            qpositions, qnormals, qtexturecoords);
    }
}

// Make a cube.
void make_cube_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvsize) {
    quads.clear();
    positions.clear();
    normals.clear();
    texturecoords.clear();
    auto qquads         = vector<vec4i>{};
    auto qpositions     = vector<vec3f>{};
    auto qnormals       = vector<vec3f>{};
    auto qtexturecoords = vector<vec2f>{};
    // + z
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.y}, {size.x, size.y}, {uvsize.x, uvsize.y});
    for (auto& p : qpositions) p = {p.x, p.y, size.z / 2};
    for (auto& n : qnormals) n = {0, 0, 1};
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // - z
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.y}, {size.x, size.y}, {uvsize.x, uvsize.y});
    for (auto& p : qpositions) p = {-p.x, p.y, -size.z / 2};
    for (auto& n : qnormals) n = {0, 0, -1};
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // + x
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.z, steps.y}, {size.z, size.y}, {uvsize.z, uvsize.y});
    for (auto& p : qpositions) p = {size.x / 2, p.y, -p.x};
    for (auto& n : qnormals) n = {1, 0, 0};
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // - x
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.z, steps.y}, {size.z, size.y}, {uvsize.z, uvsize.y});
    for (auto& p : qpositions) p = {-size.x / 2, p.y, p.x};
    for (auto& n : qnormals) n = {-1, 0, 0};
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // + y
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.z}, {size.x, size.z}, {uvsize.x, uvsize.z});
    for (auto i = 0; i < qpositions.size(); i++) {
        qpositions[i] = {qpositions[i].x, size.y / 2, -qpositions[i].y};
        qnormals[i]   = {0, 1, 0};
    }
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // - y
    make_quad_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.z}, {size.x, size.z}, {uvsize.x, uvsize.z});
    for (auto i = 0; i < qpositions.size(); i++) {
        qpositions[i] = {qpositions[i].x, -size.y / 2, qpositions[i].y};
        qnormals[i]   = {0, -1, 0};
    }
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
}

// Make a rounded cube.
void make_cube_rounded_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec3f& size, const vec3f& uvsize, float radius) {
    make_cube_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    auto c = size / 2 - vec3f{radius, radius, radius};
    for (auto i = 0; i < positions.size(); i++) {
        auto pc = vec3f{
            fabs(positions[i].x), fabs(positions[i].y), fabs(positions[i].z)};
        auto ps = vec3f{positions[i].x < 0 ? -1.0f : 1.0f,
            positions[i].y < 0 ? -1.0f : 1.0f,
            positions[i].z < 0 ? -1.0f : 1.0f};
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

// Make a sphere.
void make_uvsphere_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize) {
    make_quad_shape(
        quads, positions, normals, texturecoords, steps, {1, 1}, {1, 1});
    for (auto i = 0; i < positions.size(); i++) {
        auto uv = texturecoords[i];
        auto a  = vec2f{2 * pif * uv.x, pif * (1 - uv.y)};
        auto p  = vec3f{cos(a.x) * sin(a.y), sin(a.x) * sin(a.y), cos(a.y)};
        positions[i]     = p * (size / 2);
        normals[i]       = normalize(p);
        texturecoords[i] = uv * uvsize;
    }
}

// Make a spherecube.
void make_sphere_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize) {
    make_cube_shape(quads, positions, normals, texturecoords,
        {steps, steps, steps}, {1, 1, 1}, {uvsize, uvsize, uvsize});
    for (auto i = 0; i < positions.size(); i++) {
        auto p       = positions[i];
        positions[i] = normalize(p) * (size / 2);
        normals[i]   = normalize(p);
    }
}

// Make a flipped sphere. This is not watertight.
void make_uvsphere_flipcap_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize, const vec2f& zflip) {
    make_uvsphere_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    for (auto i = 0; i < positions.size(); i++) {
        if (positions[i].z > zflip.y) {
            positions[i].z = 2 * zflip.y - positions[i].z;
            normals[i].x   = -normals[i].x;
            normals[i].y   = -normals[i].y;
        } else if (positions[i].z < zflip.x) {
            positions[i].z = 2 * zflip.x - positions[i].z;
            normals[i].x   = -normals[i].x;
            normals[i].y   = -normals[i].y;
        }
    }
}

// Make a disk.
void make_uvdisk_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    float size, const vec2f& uvsize) {
    make_quad_shape(
        quads, positions, normals, texturecoords, steps, {1, 1}, {1, 1});
    for (auto i = 0; i < positions.size(); i++) {
        auto uv      = texturecoords[i];
        auto phi     = 2 * pif * uv.x;
        positions[i] = {
            cos(phi) * uv.y * size / 2, sin(phi) * uv.y * size / 2, 0};
        normals[i]       = {0, 0, 1};
        texturecoords[i] = uv * uvsize;
    }
}

// Make a disk from a quad.
void make_disk_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize) {
    make_quad_shape(quads, positions, normals, texturecoords, {steps, steps},
        {2, 2}, {uvsize, uvsize});
    for (auto i = 0; i < positions.size(); i++) {
        // Analytical Methods for Squaring the Disc, by C. Fong
        // https://arxiv.org/abs/1509.06344
        auto xy = vec2f{positions[i].x, positions[i].y};
        auto uv = vec2f{
            xy.x * sqrt(1 - xy.y * xy.y / 2), xy.y * sqrt(1 - xy.x * xy.x / 2)};
        positions[i] = {uv.x * size / 2, uv.y * size / 2, 0};
    }
}

// Make a bulged disk from a quad.
void make_disk_bulged_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize, float height) {
    make_disk_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    if (height == 0) return;
    auto radius = (size * size / 4 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i = 0; i < positions.size(); i++) {
        auto pn      = normalize(positions[i] - center);
        positions[i] = center + pn * radius;
        normals[i]   = pn;
    }
}

// Make a bulged quad.
void make_quad_bulged_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, int steps, float size,
    float uvsize, float height) {
    make_quad_shape(quads, positions, normals, texturecoords, {steps, steps},
        {size, size}, {uvsize, uvsize});
    if (height == 0) return;
    auto radius = (size * size / 4 + height * height) / (2 * height);
    auto center = vec3f{0, 0, -radius + height};
    for (auto i = 0; i < positions.size(); i++) {
        auto pn      = normalize(positions[i] - center);
        positions[i] = center + pn * radius;
        normals[i]   = pn;
    }
}

// Make a cylinder (side-only).
void make_cylinder_side_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec2i& steps,
    const vec2f& size, const vec2f& uvsize) {
    make_quad_shape(
        quads, positions, normals, texturecoords, steps, {1, 1}, {1, 1});
    for (auto i = 0; i < positions.size(); i++) {
        auto uv          = texturecoords[i];
        auto phi         = 2 * pif * uv.x;
        positions[i]     = {cos(phi) * size.x / 2, sin(phi) * size.x / 2,
            (uv.y - 0.5f) * size.y};
        normals[i]       = {cos(phi), sin(phi), 0};
        texturecoords[i] = uv * uvsize;
    }
}

// Make a cylinder.
void make_uvcylinder_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, const vec3i& steps,
    const vec2f& size, const vec3f& uvsize) {
    quads.clear();
    positions.clear();
    normals.clear();
    texturecoords.clear();
    auto qquads         = vector<vec4i>{};
    auto qpositions     = vector<vec3f>{};
    auto qnormals       = vector<vec3f>{};
    auto qtexturecoords = vector<vec2f>{};
    // side
    make_cylinder_side_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.y}, {size.x, size.y}, {uvsize.x, uvsize.y});
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // top
    make_uvdisk_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.z}, size.x, {uvsize.x, uvsize.z});
    for (auto i = 0; i < qpositions.size(); i++) {
        qpositions[i].z = size.y / 2;
    }
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
    // bottom
    make_uvdisk_shape(qquads, qpositions, qnormals, qtexturecoords,
        {steps.x, steps.z}, size.x, {uvsize.x, uvsize.z});
    for (auto i = 0; i < qpositions.size(); i++) {
        qpositions[i].z = -size.y / 2;
        qnormals[i]     = -qnormals[i];
    }
    for (auto i = 0; i < qquads.size(); i++) swap(qquads[i].x, qquads[i].z);
    merge_quads(quads, positions, normals, texturecoords, qquads, qpositions,
        qnormals, qtexturecoords);
}

// Make a rounded cylinder.
void make_uvcylinder_rounded_shape(vector<vec4i>& quads,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, const vec3i& steps, const vec2f& size,
    const vec3f& uvsize, float radius) {
    make_uvcylinder_shape(
        quads, positions, normals, texturecoords, steps, size, uvsize);
    auto c = size / 2 - vec2f{radius, radius};
    for (auto i = 0; i < positions.size(); i++) {
        auto phi = atan2(positions[i].y, positions[i].x);
        auto r   = length(vec2f{positions[i].x, positions[i].y});
        auto z   = positions[i].z;
        auto pc  = vec2f{r, fabs(z)};
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

// Make a geodesic sphere.
void make_geodesic_sphere_shape(vector<vec3i>& triangles,
    vector<vec3f>& positions, vector<vec3f>& normals, int tesselation,
    float size) {
    // https://stackoverflow.com/questions/17705621/algorithm-for-a-geodesic-sphere
    const float X                = 0.525731112119133606f;
    const float Z                = 0.850650808352039932f;
    static auto sphere_pos       = vector<vec3f>{{-X, 0.0, Z}, {X, 0.0, Z},
        {-X, 0.0, -Z}, {X, 0.0, -Z}, {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X},
        {0.0, -Z, -X}, {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}};
    static auto sphere_triangles = vector<vec3i>{{0, 1, 4}, {0, 4, 9},
        {9, 4, 5}, {4, 8, 5}, {4, 1, 8}, {8, 1, 10}, {8, 10, 3}, {5, 8, 3},
        {5, 3, 2}, {2, 3, 7}, {7, 3, 10}, {7, 10, 6}, {7, 6, 11}, {11, 6, 0},
        {0, 6, 1}, {6, 10, 1}, {9, 11, 0}, {9, 2, 11}, {9, 5, 2}, {7, 11, 2}};
    positions                    = sphere_pos;
    triangles                    = sphere_triangles;
    for (auto l = 0; l < max(0, tesselation - 2); l++) {
        subdivide_triangles(triangles, positions);
    }
    for (auto& p : positions) p = normalize(p) * size / 2;
    normals = positions;
}

// Make a facevarying cube.
void make_cube_fvshape(vector<vec4i>& quads_positions,
    vector<vec4i>& quads_normals, vector<vec4i>& quads_texturecoords,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, const vec3i& steps, const vec3f& size,
    const vec3f& uvsize) {
    make_cube_shape(quads_positions, positions, normals, texturecoords, steps,
        size, uvsize);
    quads_normals       = quads_positions;
    quads_texturecoords = quads_positions;
    auto positions_     = positions;
    weld_quads(quads_positions, positions,
        min(0.1f * size /
            vec3f{(float)steps.x, (float)steps.y, (float)steps.z}));
}

// Make a faceavrying spherecube.
void make_sphere_fvshape(vector<vec4i>& quads_positions,
    vector<vec4i>& quads_normals, vector<vec4i>& quads_texturecoords,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, int steps, float size, float uvsize) {
    make_cube_fvshape(quads_positions, quads_normals, quads_texturecoords,
        positions, normals, texturecoords, {steps, steps, steps}, {1, 1, 1},
        {uvsize, uvsize, uvsize});
    quads_normals = quads_positions;
    normals       = positions;
    for (auto i = 0; i < positions.size(); i++) {
        auto p       = positions[i];
        positions[i] = normalize(p) * (size / 2);
        normals[i]   = normalize(p);
    }
}

// Make a suzanne monkey model for testing. Note that some quads are
// degenerate.
void make_suzanne_shape(
    vector<vec4i>& quads, vector<vec3f>& positions, float size) {
    static auto suzanne_pos       = vector<vec3f>{{0.4375, 0.1640625, 0.765625},
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
        {0.4296875, -0.1953125, -0.2109375},
        {-0.4296875, -0.1953125, -0.2109375}, {0.890625, 0.40625, -0.234375},
        {-0.890625, 0.40625, -0.234375}, {0.7734375, -0.140625, -0.125},
        {-0.7734375, -0.140625, -0.125}, {1.0390625, -0.1015625, -0.328125},
        {-1.0390625, -0.1015625, -0.328125}, {1.28125, 0.0546875, -0.4296875},
        {-1.28125, 0.0546875, -0.4296875}, {1.3515625, 0.3203125, -0.421875},
        {-1.3515625, 0.3203125, -0.421875}, {1.234375, 0.5078125, -0.421875},
        {-1.234375, 0.5078125, -0.421875}, {1.0234375, 0.4765625, -0.3125},
        {-1.0234375, 0.4765625, -0.3125}, {1.015625, 0.4140625, -0.2890625},
        {-1.015625, 0.4140625, -0.2890625}, {1.1875, 0.4375, -0.390625},
        {-1.1875, 0.4375, -0.390625}, {1.265625, 0.2890625, -0.40625},
        {-1.265625, 0.2890625, -0.40625}, {1.2109375, 0.078125, -0.40625},
        {-1.2109375, 0.078125, -0.40625}, {1.03125, -0.0390625, -0.3046875},
        {-1.03125, -0.0390625, -0.3046875}, {0.828125, -0.0703125, -0.1328125},
        {-0.828125, -0.0703125, -0.1328125}, {0.921875, 0.359375, -0.21875},
        {-0.921875, 0.359375, -0.21875}, {0.9453125, 0.3046875, -0.2890625},
        {-0.9453125, 0.3046875, -0.2890625},
        {0.8828125, -0.0234375, -0.2109375},
        {-0.8828125, -0.0234375, -0.2109375}, {1.0390625, 0.0, -0.3671875},
        {-1.0390625, 0.0, -0.3671875}, {1.1875, 0.09375, -0.4453125},
        {-1.1875, 0.09375, -0.4453125}, {1.234375, 0.25, -0.4453125},
        {-1.234375, 0.25, -0.4453125}, {1.171875, 0.359375, -0.4375},
        {-1.171875, 0.359375, -0.4375}, {1.0234375, 0.34375, -0.359375},
        {-1.0234375, 0.34375, -0.359375}, {0.84375, 0.2890625, -0.2109375},
        {-0.84375, 0.2890625, -0.2109375}, {0.8359375, 0.171875, -0.2734375},
        {-0.8359375, 0.171875, -0.2734375}, {0.7578125, 0.09375, -0.2734375},
        {-0.7578125, 0.09375, -0.2734375}, {0.8203125, 0.0859375, -0.2734375},
        {-0.8203125, 0.0859375, -0.2734375}, {0.84375, 0.015625, -0.2734375},
        {-0.84375, 0.015625, -0.2734375}, {0.8125, -0.015625, -0.2734375},
        {-0.8125, -0.015625, -0.2734375}, {0.7265625, 0.0, -0.0703125},
        {-0.7265625, 0.0, -0.0703125}, {0.71875, -0.0234375, -0.171875},
        {-0.71875, -0.0234375, -0.171875}, {0.71875, 0.0390625, -0.1875},
        {-0.71875, 0.0390625, -0.1875}, {0.796875, 0.203125, -0.2109375},
        {-0.796875, 0.203125, -0.2109375}, {0.890625, 0.2421875, -0.265625},
        {-0.890625, 0.2421875, -0.265625}, {0.890625, 0.234375, -0.3203125},
        {-0.890625, 0.234375, -0.3203125}, {0.8125, -0.015625, -0.3203125},
        {-0.8125, -0.015625, -0.3203125}, {0.8515625, 0.015625, -0.3203125},
        {-0.8515625, 0.015625, -0.3203125}, {0.828125, 0.078125, -0.3203125},
        {-0.828125, 0.078125, -0.3203125}, {0.765625, 0.09375, -0.3203125},
        {-0.765625, 0.09375, -0.3203125}, {0.84375, 0.171875, -0.3203125},
        {-0.84375, 0.171875, -0.3203125}, {1.0390625, 0.328125, -0.4140625},
        {-1.0390625, 0.328125, -0.4140625}, {1.1875, 0.34375, -0.484375},
        {-1.1875, 0.34375, -0.484375}, {1.2578125, 0.2421875, -0.4921875},
        {-1.2578125, 0.2421875, -0.4921875}, {1.2109375, 0.0859375, -0.484375},
        {-1.2109375, 0.0859375, -0.484375}, {1.046875, 0.0, -0.421875},
        {-1.046875, 0.0, -0.421875}, {0.8828125, -0.015625, -0.265625},
        {-0.8828125, -0.015625, -0.265625}, {0.953125, 0.2890625, -0.34375},
        {-0.953125, 0.2890625, -0.34375}, {0.890625, 0.109375, -0.328125},
        {-0.890625, 0.109375, -0.328125}, {0.9375, 0.0625, -0.3359375},
        {-0.9375, 0.0625, -0.3359375}, {1.0, 0.125, -0.3671875},
        {-1.0, 0.125, -0.3671875}, {0.9609375, 0.171875, -0.3515625},
        {-0.9609375, 0.171875, -0.3515625}, {1.015625, 0.234375, -0.375},
        {-1.015625, 0.234375, -0.375}, {1.0546875, 0.1875, -0.3828125},
        {-1.0546875, 0.1875, -0.3828125}, {1.109375, 0.2109375, -0.390625},
        {-1.109375, 0.2109375, -0.390625}, {1.0859375, 0.2734375, -0.390625},
        {-1.0859375, 0.2734375, -0.390625}, {1.0234375, 0.4375, -0.484375},
        {-1.0234375, 0.4375, -0.484375}, {1.25, 0.46875, -0.546875},
        {-1.25, 0.46875, -0.546875}, {1.3671875, 0.296875, -0.5},
        {-1.3671875, 0.296875, -0.5}, {1.3125, 0.0546875, -0.53125},
        {-1.3125, 0.0546875, -0.53125}, {1.0390625, -0.0859375, -0.4921875},
        {-1.0390625, -0.0859375, -0.4921875}, {0.7890625, -0.125, -0.328125},
        {-0.7890625, -0.125, -0.328125}, {0.859375, 0.3828125, -0.3828125},
        {-0.859375, 0.3828125, -0.3828125}};
    static auto suzanne_triangles = vector<vec3i>{{60, 64, 48}, {49, 65, 61},
        {62, 64, 60}, {61, 65, 63}, {60, 58, 62}, {63, 59, 61}, {60, 56, 58},
        {59, 57, 61}, {60, 54, 56}, {57, 55, 61}, {60, 52, 54}, {55, 53, 61},
        {60, 50, 52}, {53, 51, 61}, {60, 48, 50}, {51, 49, 61}, {224, 228, 226},
        {227, 229, 225}, {72, 283, 73}, {73, 284, 72}, {341, 347, 383},
        {384, 348, 342}, {299, 345, 343}, {344, 346, 300}, {323, 379, 351},
        {352, 380, 324}, {441, 443, 445}, {446, 444, 442}, {463, 491, 465},
        {466, 492, 464}, {495, 497, 499}, {500, 498, 496}};
    static auto suzanne_quads = vector<vec4i>{{46, 0, 2, 44}, {3, 1, 47, 45},
        {44, 2, 4, 42}, {5, 3, 45, 43}, {2, 8, 6, 4}, {7, 9, 3, 5},
        {0, 10, 8, 2}, {9, 11, 1, 3}, {10, 12, 14, 8}, {15, 13, 11, 9},
        {8, 14, 16, 6}, {17, 15, 9, 7}, {14, 20, 18, 16}, {19, 21, 15, 17},
        {12, 22, 20, 14}, {21, 23, 13, 15}, {22, 24, 26, 20}, {27, 25, 23, 21},
        {20, 26, 28, 18}, {29, 27, 21, 19}, {26, 32, 30, 28}, {31, 33, 27, 29},
        {24, 34, 32, 26}, {33, 35, 25, 27}, {34, 36, 38, 32}, {39, 37, 35, 33},
        {32, 38, 40, 30}, {41, 39, 33, 31}, {38, 44, 42, 40}, {43, 45, 39, 41},
        {36, 46, 44, 38}, {45, 47, 37, 39}, {46, 36, 50, 48}, {51, 37, 47, 49},
        {36, 34, 52, 50}, {53, 35, 37, 51}, {34, 24, 54, 52}, {55, 25, 35, 53},
        {24, 22, 56, 54}, {57, 23, 25, 55}, {22, 12, 58, 56}, {59, 13, 23, 57},
        {12, 10, 62, 58}, {63, 11, 13, 59}, {10, 0, 64, 62}, {65, 1, 11, 63},
        {0, 46, 48, 64}, {49, 47, 1, 65}, {88, 173, 175, 90},
        {175, 174, 89, 90}, {86, 171, 173, 88}, {174, 172, 87, 89},
        {84, 169, 171, 86}, {172, 170, 85, 87}, {82, 167, 169, 84},
        {170, 168, 83, 85}, {80, 165, 167, 82}, {168, 166, 81, 83},
        {78, 91, 145, 163}, {146, 92, 79, 164}, {91, 93, 147, 145},
        {148, 94, 92, 146}, {93, 95, 149, 147}, {150, 96, 94, 148},
        {95, 97, 151, 149}, {152, 98, 96, 150}, {97, 99, 153, 151},
        {154, 100, 98, 152}, {99, 101, 155, 153}, {156, 102, 100, 154},
        {101, 103, 157, 155}, {158, 104, 102, 156}, {103, 105, 159, 157},
        {160, 106, 104, 158}, {105, 107, 161, 159}, {162, 108, 106, 160},
        {107, 66, 67, 161}, {67, 66, 108, 162}, {109, 127, 159, 161},
        {160, 128, 110, 162}, {127, 178, 157, 159}, {158, 179, 128, 160},
        {125, 155, 157, 178}, {158, 156, 126, 179}, {123, 153, 155, 125},
        {156, 154, 124, 126}, {121, 151, 153, 123}, {154, 152, 122, 124},
        {119, 149, 151, 121}, {152, 150, 120, 122}, {117, 147, 149, 119},
        {150, 148, 118, 120}, {115, 145, 147, 117}, {148, 146, 116, 118},
        {113, 163, 145, 115}, {146, 164, 114, 116}, {113, 180, 176, 163},
        {176, 181, 114, 164}, {109, 161, 67, 111}, {67, 162, 110, 112},
        {111, 67, 177, 182}, {177, 67, 112, 183}, {176, 180, 182, 177},
        {183, 181, 176, 177}, {134, 136, 175, 173}, {175, 136, 135, 174},
        {132, 134, 173, 171}, {174, 135, 133, 172}, {130, 132, 171, 169},
        {172, 133, 131, 170}, {165, 186, 184, 167}, {185, 187, 166, 168},
        {130, 169, 167, 184}, {168, 170, 131, 185}, {143, 189, 188, 186},
        {188, 189, 144, 187}, {184, 186, 188, 68}, {188, 187, 185, 68},
        {129, 130, 184, 68}, {185, 131, 129, 68}, {141, 192, 190, 143},
        {191, 193, 142, 144}, {139, 194, 192, 141}, {193, 195, 140, 142},
        {138, 196, 194, 139}, {195, 197, 138, 140}, {137, 70, 196, 138},
        {197, 70, 137, 138}, {189, 143, 190, 69}, {191, 144, 189, 69},
        {69, 190, 205, 207}, {206, 191, 69, 207}, {70, 198, 199, 196},
        {200, 198, 70, 197}, {196, 199, 201, 194}, {202, 200, 197, 195},
        {194, 201, 203, 192}, {204, 202, 195, 193}, {192, 203, 205, 190},
        {206, 204, 193, 191}, {198, 203, 201, 199}, {202, 204, 198, 200},
        {198, 207, 205, 203}, {206, 207, 198, 204}, {138, 139, 163, 176},
        {164, 140, 138, 176}, {139, 141, 210, 163}, {211, 142, 140, 164},
        {141, 143, 212, 210}, {213, 144, 142, 211}, {143, 186, 165, 212},
        {166, 187, 144, 213}, {80, 208, 212, 165}, {213, 209, 81, 166},
        {208, 214, 210, 212}, {211, 215, 209, 213}, {78, 163, 210, 214},
        {211, 164, 79, 215}, {130, 129, 71, 221}, {71, 129, 131, 222},
        {132, 130, 221, 219}, {222, 131, 133, 220}, {134, 132, 219, 217},
        {220, 133, 135, 218}, {136, 134, 217, 216}, {218, 135, 136, 216},
        {216, 217, 228, 230}, {229, 218, 216, 230}, {217, 219, 226, 228},
        {227, 220, 218, 229}, {219, 221, 224, 226}, {225, 222, 220, 227},
        {221, 71, 223, 224}, {223, 71, 222, 225}, {223, 230, 228, 224},
        {229, 230, 223, 225}, {182, 180, 233, 231}, {234, 181, 183, 232},
        {111, 182, 231, 253}, {232, 183, 112, 254}, {109, 111, 253, 255},
        {254, 112, 110, 256}, {180, 113, 251, 233}, {252, 114, 181, 234},
        {113, 115, 249, 251}, {250, 116, 114, 252}, {115, 117, 247, 249},
        {248, 118, 116, 250}, {117, 119, 245, 247}, {246, 120, 118, 248},
        {119, 121, 243, 245}, {244, 122, 120, 246}, {121, 123, 241, 243},
        {242, 124, 122, 244}, {123, 125, 239, 241}, {240, 126, 124, 242},
        {125, 178, 235, 239}, {236, 179, 126, 240}, {178, 127, 237, 235},
        {238, 128, 179, 236}, {127, 109, 255, 237}, {256, 110, 128, 238},
        {237, 255, 257, 275}, {258, 256, 238, 276}, {235, 237, 275, 277},
        {276, 238, 236, 278}, {239, 235, 277, 273}, {278, 236, 240, 274},
        {241, 239, 273, 271}, {274, 240, 242, 272}, {243, 241, 271, 269},
        {272, 242, 244, 270}, {245, 243, 269, 267}, {270, 244, 246, 268},
        {247, 245, 267, 265}, {268, 246, 248, 266}, {249, 247, 265, 263},
        {266, 248, 250, 264}, {251, 249, 263, 261}, {264, 250, 252, 262},
        {233, 251, 261, 279}, {262, 252, 234, 280}, {255, 253, 259, 257},
        {260, 254, 256, 258}, {253, 231, 281, 259}, {282, 232, 254, 260},
        {231, 233, 279, 281}, {280, 234, 232, 282}, {66, 107, 283, 72},
        {284, 108, 66, 72}, {107, 105, 285, 283}, {286, 106, 108, 284},
        {105, 103, 287, 285}, {288, 104, 106, 286}, {103, 101, 289, 287},
        {290, 102, 104, 288}, {101, 99, 291, 289}, {292, 100, 102, 290},
        {99, 97, 293, 291}, {294, 98, 100, 292}, {97, 95, 295, 293},
        {296, 96, 98, 294}, {95, 93, 297, 295}, {298, 94, 96, 296},
        {93, 91, 299, 297}, {300, 92, 94, 298}, {307, 308, 327, 337},
        {328, 308, 307, 338}, {306, 307, 337, 335}, {338, 307, 306, 336},
        {305, 306, 335, 339}, {336, 306, 305, 340}, {88, 90, 305, 339},
        {305, 90, 89, 340}, {86, 88, 339, 333}, {340, 89, 87, 334},
        {84, 86, 333, 329}, {334, 87, 85, 330}, {82, 84, 329, 331},
        {330, 85, 83, 332}, {329, 335, 337, 331}, {338, 336, 330, 332},
        {329, 333, 339, 335}, {340, 334, 330, 336}, {325, 331, 337, 327},
        {338, 332, 326, 328}, {80, 82, 331, 325}, {332, 83, 81, 326},
        {208, 341, 343, 214}, {344, 342, 209, 215}, {80, 325, 341, 208},
        {342, 326, 81, 209}, {78, 214, 343, 345}, {344, 215, 79, 346},
        {78, 345, 299, 91}, {300, 346, 79, 92}, {76, 323, 351, 303},
        {352, 324, 76, 303}, {303, 351, 349, 77}, {350, 352, 303, 77},
        {77, 349, 347, 304}, {348, 350, 77, 304}, {304, 347, 327, 308},
        {328, 348, 304, 308}, {325, 327, 347, 341}, {348, 328, 326, 342},
        {295, 297, 317, 309}, {318, 298, 296, 310}, {75, 315, 323, 76},
        {324, 316, 75, 76}, {301, 357, 355, 302}, {356, 358, 301, 302},
        {302, 355, 353, 74}, {354, 356, 302, 74}, {74, 353, 315, 75},
        {316, 354, 74, 75}, {291, 293, 361, 363}, {362, 294, 292, 364},
        {363, 361, 367, 365}, {368, 362, 364, 366}, {365, 367, 369, 371},
        {370, 368, 366, 372}, {371, 369, 375, 373}, {376, 370, 372, 374},
        {313, 377, 373, 375}, {374, 378, 314, 376}, {315, 353, 373, 377},
        {374, 354, 316, 378}, {353, 355, 371, 373}, {372, 356, 354, 374},
        {355, 357, 365, 371}, {366, 358, 356, 372}, {357, 359, 363, 365},
        {364, 360, 358, 366}, {289, 291, 363, 359}, {364, 292, 290, 360},
        {73, 359, 357, 301}, {358, 360, 73, 301}, {283, 285, 287, 289},
        {288, 286, 284, 290}, {283, 289, 359, 73}, {360, 290, 284, 73},
        {293, 295, 309, 361}, {310, 296, 294, 362}, {309, 311, 367, 361},
        {368, 312, 310, 362}, {311, 381, 369, 367}, {370, 382, 312, 368},
        {313, 375, 369, 381}, {370, 376, 314, 382}, {347, 349, 385, 383},
        {386, 350, 348, 384}, {317, 383, 385, 319}, {386, 384, 318, 320},
        {297, 299, 383, 317}, {384, 300, 298, 318}, {299, 343, 341, 383},
        {342, 344, 300, 384}, {313, 321, 379, 377}, {380, 322, 314, 378},
        {315, 377, 379, 323}, {380, 378, 316, 324}, {319, 385, 379, 321},
        {380, 386, 320, 322}, {349, 351, 379, 385}, {380, 352, 350, 386},
        {399, 387, 413, 401}, {414, 388, 400, 402}, {399, 401, 403, 397},
        {404, 402, 400, 398}, {397, 403, 405, 395}, {406, 404, 398, 396},
        {395, 405, 407, 393}, {408, 406, 396, 394}, {393, 407, 409, 391},
        {410, 408, 394, 392}, {391, 409, 411, 389}, {412, 410, 392, 390},
        {409, 419, 417, 411}, {418, 420, 410, 412}, {407, 421, 419, 409},
        {420, 422, 408, 410}, {405, 423, 421, 407}, {422, 424, 406, 408},
        {403, 425, 423, 405}, {424, 426, 404, 406}, {401, 427, 425, 403},
        {426, 428, 402, 404}, {401, 413, 415, 427}, {416, 414, 402, 428},
        {317, 319, 443, 441}, {444, 320, 318, 442}, {319, 389, 411, 443},
        {412, 390, 320, 444}, {309, 317, 441, 311}, {442, 318, 310, 312},
        {381, 429, 413, 387}, {414, 430, 382, 388}, {411, 417, 439, 443},
        {440, 418, 412, 444}, {437, 445, 443, 439}, {444, 446, 438, 440},
        {433, 445, 437, 435}, {438, 446, 434, 436}, {431, 447, 445, 433},
        {446, 448, 432, 434}, {429, 447, 431, 449}, {432, 448, 430, 450},
        {413, 429, 449, 415}, {450, 430, 414, 416}, {311, 447, 429, 381},
        {430, 448, 312, 382}, {311, 441, 445, 447}, {446, 442, 312, 448},
        {415, 449, 451, 475}, {452, 450, 416, 476}, {449, 431, 461, 451},
        {462, 432, 450, 452}, {431, 433, 459, 461}, {460, 434, 432, 462},
        {433, 435, 457, 459}, {458, 436, 434, 460}, {435, 437, 455, 457},
        {456, 438, 436, 458}, {437, 439, 453, 455}, {454, 440, 438, 456},
        {439, 417, 473, 453}, {474, 418, 440, 454}, {427, 415, 475, 463},
        {476, 416, 428, 464}, {425, 427, 463, 465}, {464, 428, 426, 466},
        {423, 425, 465, 467}, {466, 426, 424, 468}, {421, 423, 467, 469},
        {468, 424, 422, 470}, {419, 421, 469, 471}, {470, 422, 420, 472},
        {417, 419, 471, 473}, {472, 420, 418, 474}, {457, 455, 479, 477},
        {480, 456, 458, 478}, {477, 479, 481, 483}, {482, 480, 478, 484},
        {483, 481, 487, 485}, {488, 482, 484, 486}, {485, 487, 489, 491},
        {490, 488, 486, 492}, {463, 475, 485, 491}, {486, 476, 464, 492},
        {451, 483, 485, 475}, {486, 484, 452, 476}, {451, 461, 477, 483},
        {478, 462, 452, 484}, {457, 477, 461, 459}, {462, 478, 458, 460},
        {453, 473, 479, 455}, {480, 474, 454, 456}, {471, 481, 479, 473},
        {480, 482, 472, 474}, {469, 487, 481, 471}, {482, 488, 470, 472},
        {467, 489, 487, 469}, {488, 490, 468, 470}, {465, 491, 489, 467},
        {490, 492, 466, 468}, {391, 389, 503, 501}, {504, 390, 392, 502},
        {393, 391, 501, 499}, {502, 392, 394, 500}, {395, 393, 499, 497},
        {500, 394, 396, 498}, {397, 395, 497, 495}, {498, 396, 398, 496},
        {399, 397, 495, 493}, {496, 398, 400, 494}, {387, 399, 493, 505},
        {494, 400, 388, 506}, {493, 501, 503, 505}, {504, 502, 494, 506},
        {493, 495, 499, 501}, {500, 496, 494, 502}, {313, 381, 387, 505},
        {388, 382, 314, 506}, {313, 505, 503, 321}, {504, 506, 314, 322},
        {319, 321, 503, 389}, {504, 322, 320, 390}};

    positions = suzanne_pos;
    for (auto& p : positions) p *= size / 2;
    quads = suzanne_quads;
    for (auto& t : suzanne_triangles) {
        quads.push_back({t.x, t.y, t.z, t.z});
    }
}

// Watertight cube
void make_cube_shape(
    vector<vec4i>& quads, vector<vec3f>& positions, const vec3f& size) {
    static auto cube_pos     = vector<vec3f>{{-1, -1, -1}, {-1, +1, -1},
        {+1, +1, -1}, {+1, -1, -1}, {-1, -1, +1}, {-1, +1, +1}, {+1, +1, +1},
        {+1, -1, +1}};
    static auto cube_quads   = vector<vec4i>{{0, 1, 2, 3}, {7, 6, 5, 4},
        {4, 5, 1, 0}, {6, 7, 3, 2}, {2, 1, 5, 6}, {0, 3, 7, 4}};
    static auto cube_quad_uv = vector<vec2f>{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    positions                = cube_pos;
    for (auto& p : positions) p *= size / 2;
    quads = cube_quads;
}

// Generate lines set along a quad.
void make_lines_shape(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    const vec2i& steps, const vec2f& size, const vec2f& uvsize,
    const vec2f& line_radius) {
    auto nverts = (steps.x + 1) * steps.y;
    auto nlines = steps.x * steps.y;
    auto vid    = [steps](int i, int j) { return j * (steps.x + 1) + i; };
    auto fid    = [steps](int i, int j) { return j * steps.x + i; };

    positions.resize(nverts);
    normals.resize(nverts);
    texturecoords.resize(nverts);
    radius.resize(nverts);
    if (steps.y > 1) {
        for (auto j = 0; j < steps.y; j++) {
            for (auto i = 0; i <= steps.x; i++) {
                auto uv              = vec2f{i / (float)steps.x,
                    j / (float)(steps.y > 1 ? steps.y - 1 : 1)};
                positions[vid(i, j)] = {
                    (uv.x - 0.5f) * size.x, (uv.y - 0.5f) * size.y, 0};
                normals[vid(i, j)]       = {1, 0, 0};
                texturecoords[vid(i, j)] = uv * uvsize;
            }
        }
    } else {
        for (auto i = 0; i <= steps.x; i++) {
            auto uv                  = vec2f{i / (float)steps.x, 0};
            positions[vid(i, 0)]     = {(uv.x - 0.5f) * size.x, 0, 0};
            normals[vid(i, 0)]       = {1, 0, 0};
            texturecoords[vid(i, 0)] = uv * uvsize;
        }
    }

    lines.resize(nlines);
    for (int j = 0; j < steps.y; j++) {
        for (int i = 0; i < steps.x; i++) {
            lines[fid(i, j)] = {vid(i, j), vid(i + 1, j)};
        }
    }
}

// Generate a point set with points placed at the origin with texcoords
// varying along u.
void make_points_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    int num, float uvsize, float point_radius) {
    points.resize(num);
    for (auto i = 0; i < num; i++) points[i] = i;
    positions.assign(num, {0, 0, 0});
    normals.assign(num, {0, 0, 1});
    texturecoords.assign(num, {0, 0});
    radius.assign(num, point_radius);
    for (auto i = 0; i < texturecoords.size(); i++)
        texturecoords[i] = {(float)i / (float)num, 0};
}

// Generate a point set.
void make_random_points_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    int num, const vec3f& size, float uvsize, float point_radius,
    uint64_t seed) {
    make_points_shape(points, positions, normals, texturecoords, radius, num,
        uvsize, point_radius);
    auto rng = make_rng(seed);
    for (auto i = 0; i < positions.size(); i++) {
        positions[i] = (get_random_vec3f(rng) - vec3f{0.5f, 0.5f, 0.5f}) * size;
    }
}

// Make a point.
void make_point_shape(vector<int>& points, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    float point_radius) {
    points        = {0};
    positions     = {{0, 0, 0}};
    normals       = {{0, 0, 1}};
    texturecoords = {{0, 0}};
    radius        = {point_radius};
}

// Make a bezier circle. Returns bezier, pos.
void make_bezier_circle_shape(
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

// Make a hair ball around a shape
void make_hair_shape(vector<vec2i>& lines, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, vector<float>& radius,
    const vec2i& steps, const vector<vec3i>& striangles,
    const vector<vec4i>& squads, const vector<vec3f>& spos,
    const vector<vec3f>& snorm, const vector<vec2f>& stexcoord,
    const vec2f& len, const vec2f& rad, const vec2f& noise, const vec2f& clump,
    const vec2f& rotation, int seed) {
    auto alltriangles    = striangles;
    auto quads_triangles = vector<vec3i>{};
    convert_quads_to_triangles(quads_triangles, squads);
    alltriangles.insert(
        alltriangles.end(), quads_triangles.begin(), quads_triangles.end());
    auto bpos      = vector<vec3f>{};
    auto bnorm     = vector<vec3f>{};
    auto btexcoord = vector<vec2f>{};
    sample_triangles_points(bpos, bnorm, btexcoord, alltriangles, spos, snorm,
        stexcoord, steps.y, seed);

    auto rng  = make_rng(seed, 3);
    auto blen = vector<float>(bpos.size());
    for (auto& l : blen) l = lerp(len.x, len.y, get_random_float(rng));

    auto cidx = vector<int>();
    if (clump.x > 0) {
        for (auto bidx = 0; bidx < bpos.size(); bidx++) {
            cidx.push_back(0);
            auto cdist = float_max;
            for (auto c = 0; c < clump.y; c++) {
                auto d = length(bpos[bidx] - bpos[c]);
                if (d < cdist) {
                    cdist       = d;
                    cidx.back() = c;
                }
            }
        }
    }

    make_lines_shape(lines, positions, normals, texturecoords, radius, steps,
        {1, 1}, {1, 1});
    for (auto i = 0; i < positions.size(); i++) {
        auto u       = texturecoords[i].x;
        auto bidx    = i / (steps.x + 1);
        positions[i] = bpos[bidx] + bnorm[bidx] * u * blen[bidx];
        normals[i]   = bnorm[bidx];
        radius[i]    = lerp(rad.x, rad.y, u);
        if (clump.x > 0) {
            positions[i] = positions[i] +
                           (positions[i + (cidx[bidx] - bidx) * (steps.x + 1)] -
                               positions[i]) *
                               u * clump.x;
        }
        if (noise.x > 0) {
            auto nx = perlin_noise(positions[i] * noise.y + vec3f{0, 0, 0}) *
                      noise.x;
            auto ny = perlin_noise(positions[i] * noise.y + vec3f{3, 7, 11}) *
                      noise.x;
            auto nz = perlin_noise(positions[i] * noise.y + vec3f{13, 17, 19}) *
                      noise.x;
            positions[i] += {nx, ny, nz};
        }
    }

    if (clump.x > 0 || noise.x > 0 || rotation.x > 0) {
        compute_vertex_tangents(normals, lines, positions);
    }
}

// Thickens a shape by copy9ing the shape content, rescaling it and flipping its
// normals. Note that this is very much not robust and only useful for trivial
// cases.
void make_shell_shape(vector<vec4i>& quads, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords, float thickness) {
    auto bbox = invalid_bbox3f;
    for (auto p : positions) bbox += p;
    auto center              = bbox_center(bbox);
    auto inner_quads         = quads;
    auto inner_positions     = positions;
    auto inner_normals       = normals;
    auto inner_texturecoords = texturecoords;
    for (auto& p : inner_positions) p = (1 - thickness) * (p - center) + center;
    for (auto& n : inner_normals) n = -n;
    merge_quads(quads, positions, normals, texturecoords, inner_quads,
        inner_positions, inner_normals, inner_texturecoords);
}

}  // namespace yocto
