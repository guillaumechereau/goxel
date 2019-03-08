//
// # Yocto/Json: Tiny collection of utilities to support JSON in Yocto/GL
//
//
// Yocto/Json is a collection of utilities used in writing other Yocto/GL
// libraries that use the JSON file format. We support converting basic Yocto/GL
// types to JSON and load/saving JSON files. Internally we use nlohmann::json as
// the Json implementation. This might change in the future. You should consider
// this library as just a helper for the rest of Yocto/GL.
//
//
// ## JSON conversion
//
// We adopt nlohmann::json (https://github.com/nlohmann/json) conventions to
// convert to and from JSON. Please see the documentation of the library. That
// implementation makes heavy use of exceptions. We prefer to use exception-free
// code and provide the functions `serialize_json_value()` for it.
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

#ifndef _YOCTO_JSON_H_
#define _YOCTO_JSON_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_math.h"
#include "yocto_utils.h"

#include "ext/json.hpp"

#include <memory>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

using nlohmann::json;
using std::unique_ptr;

}  // namespace yocto

// -----------------------------------------------------------------------------
// FILE IO
// -----------------------------------------------------------------------------
namespace yocto {

// Load/save a text file
inline void load_json(const string& filename, json& js);
inline void save_json(const string& filename, const json& js);

}  // namespace yocto

// ---------------------------------------------------------------------------//
//                                                                            //
//                             IMPLEMENTATION                                 //
//                                                                            //
// ---------------------------------------------------------------------------//

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR FILE READING
// -----------------------------------------------------------------------------
namespace yocto {

// Load a JSON object
inline void load_json(const string& filename, json& js) {
    auto fs = fopen(filename.c_str(), "rt");
    if (!fs) throw runtime_error("could not load json " + filename);
    auto fs_guard = unique_ptr<FILE, void (*)(FILE*)>{
        fs, [](FILE* f) { fclose(f); }};
    js = json::parse(fs);
}

// Save a JSON object
inline void save_json(const string& filename, const json& js) {
    auto str = js.dump(4);
    auto fs  = fopen(filename.c_str(), "wt");
    if (!fs) throw runtime_error("could not save json " + filename);
    auto fs_guard = unique_ptr<FILE, void (*)(FILE*)>{
        fs, [](FILE* f) { fclose(f); }};
    if (fprintf(fs, "%s", str.c_str()) < 0) {
        throw runtime_error("could not save json " + filename);
    }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF CONVERSION TO/FROM JSON
// -----------------------------------------------------------------------------
namespace yocto {

template <typename T, int N>
inline void to_json(json& js, const vec<T, N>& val) {
    nlohmann::to_json(js, (const std::array<T, N>&)val);
}
template <typename T, int N>
inline void from_json(const json& js, vec<T, N>& val) {
    nlohmann::from_json(js, (std::array<T, N>&)val);
}

template <typename T, int N>
inline void to_json(json& js, const frame<T, N>& val) {
    nlohmann::to_json(js, (const std::array<T, N*(N + 1)>&)val);
}
template <typename T, int N>
inline void from_json(const json& js, frame<T, N>& val) {
    nlohmann::from_json(js, (std::array<T, N*(N + 1)>&)val);
}

template <typename T, int N, int M>
inline void to_json(json& js, const mat<T, N, M>& val) {
    nlohmann::to_json(js, (const std::array<T, N * M>&)val);
}
template <typename T, int N, int M>
inline void from_json(const json& js, mat<T, N, M>& val) {
    nlohmann::from_json(js, (std::array<T, N * M>&)val);
}

template <typename T, int N>
inline void to_json(json& js, const bbox<T, N>& val) {
    nlohmann::from_json(js, (std::array<T, N * 2>&)val);
}
template <typename T, int N>
inline void from_json(const json& js, bbox<T, N>& val) {
    nlohmann::to_json(js, (const std::array<T, N * 2>&)val);
}

}  // namespace yocto

#endif
