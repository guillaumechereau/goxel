//
// # Yocto/SceneIO: Tiny library for Yocto/Scene input and output
//
// Yocto/SceneIO provides loading and saving functionality for scenes
// in Yocto/GL. We support a simple to use JSON format, PLY, OBJ and glTF.
// The JSON serialization is a straight copy of the in-memory scene data.
// To speed up testing, we also support a binary format that is a dump of
// the current scene. This format should not be use for archival though.
//
// Error reporting is done through exceptions using the `io_error` exception.
//
// ## Scene Loading and Saving
//
// 1. load a scene with `load_json_scene()` and save it with `save_json_scene()`
// 2. load and save OBJs with `load_obj_scene()` and `save_obj_scene()`
// 3. load and save glTFs with `load_gltf_scene()` and `save_gltf_scene()`
// 4. load and save binary dumps with `load_ybin_scene()`and `save_ybin_scene()`
// 5. if desired, the function `load_scene()` and `save_scene()` will either
//    load using the internal format or convert on the fly using on the
//    supported conversions
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

#ifndef _YOCTO_SCENEIO_H_
#define _YOCTO_SCENEIO_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_scene.h"

// -----------------------------------------------------------------------------
// SCENE IO FUNCTIONS
// -----------------------------------------------------------------------------

namespace yocto {

// Scene load options
struct load_scene_options {
    bool          skip_textures             = false;
    bool          skip_meshes               = false;
    bool          obj_split_shapes          = true;
    bool          obj_preserve_face_varying = false;
    atomic<bool>* cancel_flag               = nullptr;
    bool          run_serially              = false;
};
// Scene save options
struct save_scene_options {
    bool          skip_textures = false;
    bool          skip_meshes   = false;
    atomic<bool>* cancel_flag   = nullptr;
    bool          run_serially  = false;
};

// Load/save a scene in the supported formats.
void load_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a scene in the builtin JSON format.
void load_json_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_json_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a scene from/to OBJ.
void load_obj_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_obj_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a scene from/to PLY. Loads/saves only one mesh with no other data.
void load_ply_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_ply_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a scene from/to glTF.
void load_gltf_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_gltf_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a scene from/to pbrt. This is not robust at all and only
// works on scene that have been previously adapted since the two renderers
// are too different to match.
void load_pbrt_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_pbrt_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// Load/save a binary dump useful for very fast scene IO. This format is not
// an archival format and should only be used as an intermediate format.
void load_ybin_scene(const string& filename, yocto_scene& scene,
    const load_scene_options& options = {});
void save_ybin_scene(const string& filename, const yocto_scene& scene,
    const save_scene_options& options = {});

// sceneio error
struct sceneio_error : runtime_error {
    explicit sceneio_error(const char* msg) : runtime_error{msg} {}
    explicit sceneio_error(const std::string& msg) : runtime_error{msg} {}
};

}  // namespace yocto

// -----------------------------------------------------------------------------
// MESH IO FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Load/Save a mesh
void load_mesh(const string& filename, vector<int>& points,
    vector<vec2i>& lines, vector<vec3i>& triangles, vector<vec4i>& quads,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, vector<vec4f>& colors, vector<float>& radius,
    bool force_triangles);
void save_mesh(const string& filename, const vector<int>& points,
    const vector<vec2i>& lines, const vector<vec3i>& triangles,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    const vector<vec4f>& colors, const vector<float>& radius,
    bool ascii = false);

// Load/Save a ply mesh
void load_ply_mesh(const string& filename, vector<int>& points,
    vector<vec2i>& lines, vector<vec3i>& triangles, vector<vec4i>& quads,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, vector<vec4f>& color, vector<float>& radius,
    bool force_triangles, bool flip_texcoord = true);
void save_ply_mesh(const string& filename, const vector<int>& points,
    const vector<vec2i>& lines, const vector<vec3i>& triangles,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    const vector<vec4f>& colors, const vector<float>& radius,
    bool ascii = false, bool flip_texcoord = true);

// Load/Save an OBJ mesh
void load_obj_mesh(const string& filename, vector<int>& points,
    vector<vec2i>& lines, vector<vec3i>& triangles, vector<vec4i>& quads,
    vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texturecoords, bool force_triangles,
    bool flip_texcoord = true);
void save_obj_mesh(const string& filename, const vector<int>& points,
    const vector<vec2i>& lines, const vector<vec3i>& triangles,
    const vector<vec4i>& quads, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    bool flip_texcoord = true);

// Load/Save a face-varying mesh
void load_facevarying_mesh(const string& filename,
    vector<vec4i>& quads_positions, vector<vec4i>& quads_normals,
    vector<vec4i>& quads_texturecoords, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
    vector<int>& quads_materials);
void save_facevarying_mesh(const string& filename,
    const vector<vec4i>& quads_positions, const vector<vec4i>& quads_normals,
    const vector<vec4i>& quads_texturecoords, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    const vector<int>& quads_materials, bool ascii = false);

// Load/Save an OBJ mesh
void load_obj_facevarying_mesh(const string& filename,
    vector<vec4i>& quads_positions, vector<vec4i>& quads_normals,
    vector<vec4i>& quads_texturecoords, vector<vec3f>& positions,
    vector<vec3f>& normals, vector<vec2f>& texturecoords,
    vector<int>& quads_materials, bool flip_texcoord = true);
void save_obj_facevarying_mesh(const string& filename,
    const vector<vec4i>& quads_positions, const vector<vec4i>& quads_normals,
    const vector<vec4i>& quads_texturecoords, const vector<vec3f>& positions,
    const vector<vec3f>& normals, const vector<vec2f>& texturecoords,
    const vector<int>& quads_materials, bool flip_texcoord = true);

}  // namespace yocto

#endif
