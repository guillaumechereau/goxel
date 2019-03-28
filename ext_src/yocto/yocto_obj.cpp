//
// Implementation for Yocto/OBJ loader.
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

#include "yocto_obj.h"

#include <memory>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

using std::unique_ptr;

}

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF FAST PARSING
// -----------------------------------------------------------------------------
namespace yocto {

// normalize obj line for simpler parsing
inline void normalize_obj_line(char* str, char comment_char = '#') {
    auto has_content = false;
    auto start       = str;
    while (*str) {
        if (*str == comment_char) {
            *str = 0;
            break;
        } else if (*str == ' ' || *str == '\t' || *str == '\r' ||
                   *str == '\n') {
            *str = ' ';
        } else {
            has_content = true;
        }
        str++;
    }
    if (!has_content) *start = 0;
}

// Parse values from a string
inline void parse_value(char*& str, int& value) {
    char* end = nullptr;
    value     = (int)strtol(str, &end, 10);
    if (str == end) throw obj_error("cannot parse value");
    str = end;
}
inline void parse_value(char*& str, bool& value) {
    auto valuei = 0;
    parse_value(str, valuei);
    value = (bool)valuei;
}
inline void parse_value(char*& str, float& value) {
    char* end = nullptr;
    value     = strtof(str, &end);
    if (str == end) throw obj_error("cannot parse value");
    str = end;
}
inline void parse_value(char*& str, string& value, bool ok_if_empty = false) {
    value = "";
    while (*str == ' ') str++;
    if (!*str && !ok_if_empty) {
        throw obj_error("cannot parse value");
    }
    while (*str && *str != ' ') {
        value += *str;
        str++;
    }
}
template <typename T, int N>
inline void parse_value(char*& str, vec<T, N>& value) {
    for (auto i = 0; i < N; i++) parse_value(str, value[i]);
}
template <typename T, int N>
inline void parse_value(char*& str, frame<T, N>& value) {
    for (auto i = 0; i < N + 1; i++) parse_value(str, value[i]);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// OBJ CONVERSION
// -----------------------------------------------------------------------------
namespace yocto {

inline void parse_value(char*& str, obj_vertex& value) {
    value = obj_vertex{0, 0, 0};
    parse_value(str, value.position);
    if (*str == '/') {
        str++;
        if (*str == '/') {
            str++;
            parse_value(str, value.normal);
        } else {
            parse_value(str, value.texturecoord);
            if (*str == '/') {
                str++;
                parse_value(str, value.normal);
            }
        }
    }
}

// Input for OBJ textures
inline void parse_value(char*& str, obj_texture_info& info) {
    // initialize
    info = obj_texture_info();

    // get tokens
    auto tokens = vector<string>();
    while (*str == ' ') str++;
    while (*str) {
        auto token = ""s;
        parse_value(str, token);
        tokens.push_back(token);
        while (*str == ' ') str++;
    }
    if (tokens.empty()) throw obj_error("cannot parse value");

    // texture name
    info.path = normalize_path(tokens.back());

    // texture options
    auto last = string();
    for (auto i = 0; i < tokens.size() - 1; i++) {
        if (tokens[i] == "-bm") info.scale = atof(tokens[i + 1].c_str());
        if (tokens[i] == "-clamp") info.clamp = true;
    }
}

// Load obj materials
void load_mtl(const string& filename, const obj_callbacks& cb,
    const load_obj_options& options) {
    // open file
    auto fs = fopen(filename.c_str(), "rt");
    if (!fs) throw obj_error("cannot load mtl " + filename);
    auto fs_guard = unique_ptr<FILE, void (*)(FILE*)>{
        fs, [](FILE* f) { fclose(f); }};

    // currently parsed material
    auto material = obj_material();
    auto first    = true;

    // read the file line by line
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fs)) {
        // line
        auto line = buffer;
        normalize_obj_line(line);
        if (!*line) continue;

        // get command
        auto cmd = ""s;
        parse_value(line, cmd);
        if (cmd == "") continue;

        // possible token values
        if (cmd == "newmtl") {
            if (!first && cb.material) cb.material(material);
            first    = false;
            material = obj_material();
            parse_value(line, material.name);
        } else if (cmd == "illum") {
            parse_value(line, material.illum);
        } else if (cmd == "Ke") {
            parse_value(line, material.ke);
        } else if (cmd == "Kd") {
            parse_value(line, material.kd);
        } else if (cmd == "Ks") {
            parse_value(line, material.ks);
        } else if (cmd == "Kt") {
            parse_value(line, material.kt);
        } else if (cmd == "Tf") {
            material.kt = {-1, -1, -1};
            parse_value(line, material.kt);
            if (material.kt.y < 0)
                material.kt = {material.kt.x, material.kt.x, material.kt.x};
            if (options.flip_tr) material.kt = vec3f{1, 1, 1} - material.kt;
        } else if (cmd == "Tr") {
            parse_value(line, material.op);
            if (options.flip_tr) material.op = 1 - material.op;
        } else if (cmd == "Ns") {
            parse_value(line, material.ns);
            material.rs = pow(2 / (material.ns + 2), 1 / 4.0f);
            if (material.rs < 0.01f) material.rs = 0;
            if (material.rs > 0.99f) material.rs = 1;
        } else if (cmd == "d") {
            parse_value(line, material.op);
        } else if (cmd == "Pr" || cmd == "rs") {
            parse_value(line, material.rs);
        } else if (cmd == "map_Ke") {
            parse_value(line, material.ke_txt);
        } else if (cmd == "map_Kd") {
            parse_value(line, material.kd_txt);
        } else if (cmd == "map_Ks") {
            parse_value(line, material.ks_txt);
        } else if (cmd == "map_Tr") {
            parse_value(line, material.kt_txt);
        } else if (cmd == "map_d" || cmd == "map_Tr") {
            parse_value(line, material.op_txt);
        } else if (cmd == "map_Pr" || cmd == "map_rs") {
            parse_value(line, material.rs_txt);
        } else if (cmd == "map_occ" || cmd == "occ") {
            parse_value(line, material.occ_txt);
        } else if (cmd == "map_bump" || cmd == "bump") {
            parse_value(line, material.bump_txt);
        } else if (cmd == "map_disp" || cmd == "disp") {
            parse_value(line, material.disp_txt);
        } else if (cmd == "map_norm" || cmd == "norm") {
            parse_value(line, material.norm_txt);
        } else if (cmd == "Ve") {
            parse_value(line, material.ve);
        } else if (cmd == "Va") {
            parse_value(line, material.va);
        } else if (cmd == "Vd") {
            parse_value(line, material.vd);
        } else if (cmd == "Vg") {
            parse_value(line, material.vg);
        } else if (cmd == "map_Vd") {
            parse_value(line, material.vd_txt);
        }
    }

    // issue current material
    if (!first && cb.material) cb.material(material);
}

// Load obj extensions
void load_objx(const string& filename, const obj_callbacks& cb,
    const load_obj_options& options) {
    // open file
    auto fs = fopen(filename.c_str(), "rt");
    if (!fs) throw obj_error("cannot load objx " + filename);
    auto fs_guard = unique_ptr<FILE, void (*)(FILE*)>{
        fs, [](FILE* f) { fclose(f); }};

    // read the file line by line
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fs)) {
        // line
        auto line = buffer;
        normalize_obj_line(line);
        if (!*line) continue;

        // get command
        auto cmd = ""s;
        parse_value(line, cmd);
        if (cmd == "") continue;

        // possible token values
        if (cmd == "c") {
            auto camera = obj_camera();
            parse_value(line, camera.name);
            parse_value(line, camera.ortho);
            parse_value(line, camera.width);
            parse_value(line, camera.height);
            parse_value(line, camera.focal);
            parse_value(line, camera.focus);
            parse_value(line, camera.aperture);
            parse_value(line, camera.frame);
            if (cb.camera) cb.camera(camera);
        } else if (cmd == "e") {
            auto environment = obj_environment();
            parse_value(line, environment.name);
            parse_value(line, environment.ke);
            parse_value(line, environment.ke_txt.path);
            parse_value(line, environment.frame);
            if (environment.ke_txt.path == "\"\"") environment.ke_txt.path = "";
            if (cb.environmnet) cb.environmnet(environment);
        } else if (cmd == "po") {
            auto procedural = obj_procedural();
            parse_value(line, procedural.name);
            parse_value(line, procedural.type);
            parse_value(line, procedural.material);
            parse_value(line, procedural.size);
            parse_value(line, procedural.level);
            parse_value(line, procedural.frame);
            if (cb.procedural) cb.procedural(procedural);
        } else {
            // unused
        }
    }
}

// Load obj scene
void load_obj(const string& filename, const obj_callbacks& cb,
    const load_obj_options& options) {
    // open file
    auto fs = fopen(filename.c_str(), "rt");
    if (!fs) throw obj_error("cannot load obj " + filename);
    auto fs_guard = unique_ptr<FILE, void (*)(FILE*)>{
        fs, [](FILE* f) { fclose(f); }};

    // track vertex size
    auto vert_size = obj_vertex();
    auto verts     = vector<obj_vertex>();  // buffer to avoid reallocation

    // read the file line by line
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fs)) {
        // line
        auto line = buffer;
        normalize_obj_line(line);
        if (!*line) continue;

        // get command
        auto cmd = ""s;
        parse_value(line, cmd);
        if (cmd == "") continue;

        // possible token values
        if (cmd == "v") {
            auto vert = zero3f;
            parse_value(line, vert);
            if (cb.vert) cb.vert(vert);
            vert_size.position += 1;
        } else if (cmd == "vn") {
            auto vert = zero3f;
            parse_value(line, vert);
            if (cb.norm) cb.norm(vert);
            vert_size.normal += 1;
        } else if (cmd == "vt") {
            auto vert = zero2f;
            parse_value(line, vert);
            if (options.flip_texcoord) vert.y = 1 - vert.y;
            if (cb.texcoord) cb.texcoord(vert);
            vert_size.texturecoord += 1;
        } else if (cmd == "f" || cmd == "l" || cmd == "p") {
            verts.clear();
            while (*line == ' ') line++;
            while (*line) {
                auto vert = obj_vertex{};
                parse_value(line, vert);
                if (!vert.position) break;
                if (vert.position < 0)
                    vert.position = vert_size.position + vert.position + 1;
                if (vert.texturecoord < 0)
                    vert.texturecoord = vert_size.texturecoord +
                                        vert.texturecoord + 1;
                if (vert.normal < 0)
                    vert.normal = vert_size.normal + vert.normal + 1;
                verts.push_back(vert);
                while (*line == ' ') line++;
            }
            if (cmd == "f" && cb.face) cb.face(verts);
            if (cmd == "l" && cb.line) cb.line(verts);
            if (cmd == "p" && cb.point) cb.point(verts);
        } else if (cmd == "o") {
            auto name = ""s;
            parse_value(line, name, true);
            if (cb.object) cb.object(name);
        } else if (cmd == "usemtl") {
            auto name = ""s;
            parse_value(line, name, true);
            if (cb.usemtl) cb.usemtl(name);
        } else if (cmd == "g") {
            auto name = ""s;
            parse_value(line, name, true);
            if (cb.group) cb.group(name);
        } else if (cmd == "s") {
            auto name = ""s;
            parse_value(line, name, true);
            if (cb.smoothing) cb.smoothing(name);
        } else if (cmd == "mtllib") {
            if (options.geometry_only) continue;
            auto mtlname = ""s;
            parse_value(line, mtlname);
            if (cb.mtllib) cb.mtllib(mtlname);
            auto mtlpath = get_dirname(filename) + mtlname;
            load_mtl(mtlpath, cb, options);
        } else {
            // unused
        }
    }

    // parse extensions if presents
    if (!options.geometry_only) {
        auto extname    = replace_extension(filename, "objx");
        auto ext_exists = exists_file(extname);
        if (ext_exists) {
            load_objx(extname, cb, options);
        }
    }
}

}  // namespace yocto
