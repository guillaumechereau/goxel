//
// Implementation for Yocto/Ply.
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

#include "yocto_modelio.h"

#include <fast_float/fast_float.h>

#define _USE_MATH_DEFINES
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::pair;
using std::string_view;
using std::unordered_map;
using std::unordered_set;
using namespace std::string_literals;
using namespace std::string_view_literals;
using byte = unsigned char;
using std::cos;
using std::sin;

}  // namespace yocto

// -----------------------------------------------------------------------------
// FILE IO
// -----------------------------------------------------------------------------
namespace yocto {

// Opens a file with a utf8 file name
static FILE* fopen_utf8(const char* filename, const char* mode) {
#ifdef _WIN32
  auto path8    = std::filesystem::u8path(filename);
  auto str_mode = string{mode};
  auto wmode    = std::wstring(str_mode.begin(), str_mode.end());
  return _wfopen(path8.c_str(), wmode.c_str());
#else
  return fopen(filename, mode);
#endif
}

// Load a text file
static bool load_text(const string& filename, string& str, string& error) {
  // https://stackoverflow.com/questions/174531/how-to-read-the-content-of-a-file-to-a-string-in-c
  auto fs = fopen_utf8(filename.c_str(), "rb");
  if (!fs) {
    error = "cannot open " + filename;
    return false;
  }
  fseek(fs, 0, SEEK_END);
  auto length = ftell(fs);
  fseek(fs, 0, SEEK_SET);
  str.resize(length);
  if (fread(str.data(), 1, length, fs) != length) {
    fclose(fs);
    error = "cannot read " + filename;
    return false;
  }
  fclose(fs);
  return true;
}

// Save a text file
static bool save_text(
    const string& filename, const string& str, string& error) {
  auto fs = fopen_utf8(filename.c_str(), "wt");
  if (!fs) {
    error = "cannot create " + filename;
    return false;
  }
  if (fprintf(fs, "%s", str.c_str()) < 0) {
    fclose(fs);
    error = "cannot write " + filename;
    return false;
  }
  fclose(fs);
  return true;
}

// Load a binary file
static bool load_binary(
    const string& filename, vector<byte>& data, string& error) {
  // https://stackoverflow.com/questions/174531/how-to-read-the-content-of-a-file-to-a-string-in-c
  auto fs = fopen_utf8(filename.c_str(), "rb");
  if (!fs) {
    error = "cannot open " + filename;
    return false;
  }
  fseek(fs, 0, SEEK_END);
  auto length = ftell(fs);
  fseek(fs, 0, SEEK_SET);
  data.resize(length);
  if (fread(data.data(), 1, length, fs) != length) {
    fclose(fs);
    error = "cannot read " + filename;
    return false;
  }
  fclose(fs);
  return true;
}

// Save a binary file
static bool save_binary(
    const string& filename, const vector<byte>& data, string& error) {
  auto fs = fopen_utf8(filename.c_str(), "wb");
  if (!fs) {
    error = "cannot create " + filename;
    return false;
  }
  if (fwrite(data.data(), 1, data.size(), fs) != data.size()) {
    fclose(fs);
    error = "cannot write " + filename;
    return false;
  }
  fclose(fs);
  return true;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// PATH UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Make a path from a utf8 string
static std::filesystem::path make_path(const string& filename) {
  return std::filesystem::u8path(filename);
}

// Get directory name (not including /)
static string path_dirname(const string& filename) {
  return make_path(filename).parent_path().generic_u8string();
}

// Get filename without directory.
static string path_filename(const string& filename) {
  return make_path(filename).filename().generic_u8string();
}

// Joins paths
static string path_join(const string& patha, const string& pathb) {
  return (make_path(patha) / make_path(pathb)).generic_u8string();
}

// Replaces extensions
static string replace_extension(const string& filename, const string& ext) {
  return make_path(filename).replace_extension(ext).generic_u8string();
}

// Check if a file can be opened for reading.
static bool path_exists(const string& filename) {
  return exists(make_path(filename));
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// ARRAY MATH HELPERS
// -----------------------------------------------------------------------------
namespace yocto {

static array<float, 3> cross(
    const array<float, 3>& a, const array<float, 3>& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0]};
}
static float length(const array<float, 3>& a) {
  return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}
static array<float, 3> normalize(const array<float, 3>& a) {
  auto l = length(a);
  if (l == 0) return a;
  return {a[0] / l, a[1] / l, a[2] / l};
}

static array<float, 3> triangle_normal(const array<float, 3>& p0,
    const array<float, 3>& p1, const array<float, 3>& p2) {
  return normalize(cross({p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]},
      {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]}));
}

static array<array<float, 3>, 4> lookat_frame(const array<float, 3>& eye,
    const array<float, 3>& center, const array<float, 3>& up,
    bool inv_xz = false) {
  auto w = normalize(
      {eye[0] - center[0], eye[1] - center[1], eye[2] - center[2]});
  auto u = normalize(cross(up, w));
  auto v = normalize(cross(w, u));
  if (inv_xz) {
    w = {-w[0], -w[1], -w[2]};
    u = {-u[0], -u[1], -u[2]};
  }
  return {u, v, w, eye};
}

static array<float, 12> flatten(const array<array<float, 3>, 4>& a) {
  return (const array<float, 12>&)a;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// HELPER FOR STL
// -----------------------------------------------------------------------------
namespace std {

// Hash functor for vector for use with hash_map
template <typename T, size_t N>
struct hash<array<T, N>> {
  size_t operator()(const array<T, N>& value) const {
    const std::hash<float> hasher = std::hash<T>();
    auto                   h      = (size_t)0;
    for (auto item : value) {
      h ^= hasher(item) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
  }
};

}  // namespace std

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// Formats values to string
static void format_value(string& str, const string& value) { str += value; }
static void format_value(string& str, const char* value) { str += value; }
template <typename T>
static void format_value(string& str, T value) {
  auto buffer = array<char, 64>{};
  if constexpr (std::is_same_v<T, float>) {
#ifdef _WIN32
    auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    str.append(buffer.data(), result.ptr);
#else
    auto len = snprintf(buffer.data(), buffer.size(), "%.9g", value);
    str.append(buffer.data(), buffer.data() + len);
#endif
  } else if constexpr (std::is_same_v<T, double>) {
#ifdef _WIN32
    auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    str.append(buffer.data(), result.ptr);
#else
    auto len = snprintf(buffer.data(), buffer.size(), "%.17g", value);
    str.append(buffer.data(), buffer.data() + len);
#endif
  } else {
    auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    str.append(buffer.data(), result.ptr);
  }
}
static void format_value(string& str, bool value) {
  format_value(str, value ? 1 : 0);
}
template <typename T, size_t N>
static void format_value(string& str, const array<T, N>& value) {
  for (auto i = 0; i < N; i++) {
    if (i != 0) str += " ";
    format_value(str, value[i]);
  }
}

// Foramt to file
static void format_values(string& str, string_view fmt) {
  auto pos = fmt.find("{}"sv);
  if (pos != string::npos) throw std::invalid_argument("bad format string");
  str += fmt;
}
template <typename Arg, typename... Args>
static void format_values(
    string& str, string_view fmt, const Arg& arg, const Args&... args) {
  auto pos = fmt.find("{}"sv);
  if (pos == string::npos) throw std::invalid_argument("bad format string");
  str += fmt.substr(0, pos);
  format_value(str, arg);
  format_values(str, fmt.substr(pos + 2), args...);
}

static bool is_newline(char c) { return c == '\r' || c == '\n'; }
static bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static void skip_whitespace(string_view& str) {
  while (!str.empty() && is_space(str.front())) str.remove_prefix(1);
}

static void remove_comment(
    string_view& str, char comment_char = '#', bool handle_quotes = false) {
  if (!handle_quotes) {
    while (!str.empty() && is_newline(str.back())) str.remove_suffix(1);
    auto cpy = str;
    while (!cpy.empty() && cpy.front() != comment_char) cpy.remove_prefix(1);
    str.remove_suffix(cpy.size());
  } else {
    while (!str.empty() && is_newline(str.back())) str.remove_suffix(1);
    auto cpy       = str;
    auto in_string = false;
    while (!cpy.empty()) {
      if (cpy.front() == '"') in_string = !in_string;
      if (cpy.front() == comment_char && !in_string) break;
      cpy.remove_prefix(1);
    }
    str.remove_suffix(cpy.size());
  }
}

// Read a line
static bool read_line(string_view& str, string_view& line) {
  if (str.empty()) return false;
  auto data = str.data();
  auto size = (size_t)0;
  while (!str.empty()) {
    if (str.front() == '\n') {
      str.remove_prefix(1);
      size++;
      break;
    } else {
      str.remove_prefix(1);
      size++;
    }
  }
  line = {data, size};
  return true;
}

// Parse values from a string
[[nodiscard]] static bool parse_value(string_view& str, string_view& value) {
  skip_whitespace(str);
  if (str.empty()) return false;
  if (str.front() != '"') {
    auto cpy = str;
    while (!cpy.empty() && !is_space(cpy.front())) cpy.remove_prefix(1);
    value = str;
    value.remove_suffix(cpy.size());
    str.remove_prefix(str.size() - cpy.size());
  } else {
    if (str.front() != '"') return false;
    str.remove_prefix(1);
    if (str.empty()) return false;
    auto cpy = str;
    while (!cpy.empty() && cpy.front() != '"') cpy.remove_prefix(1);
    if (cpy.empty()) return false;
    value = str;
    value.remove_suffix(cpy.size());
    str.remove_prefix(str.size() - cpy.size());
    str.remove_prefix(1);
  }
  return true;
}
[[nodiscard]] static bool parse_value(string_view& str, string& value) {
  auto valuev = string_view{};
  if (!parse_value(str, valuev)) return false;
  value = string{valuev};
  return true;
}
template <typename T>
[[nodiscard]] static bool parse_value(string_view& str, T& value) {
  skip_whitespace(str);
  if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    auto result = fast_float::from_chars(
        str.data(), str.data() + str.size(), value);
    if (result.ptr == str.data()) return false;
    str.remove_prefix(result.ptr - str.data());
  } else {
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    if (result.ptr == str.data()) return false;
    str.remove_prefix(result.ptr - str.data());
  }
  return true;
}
[[nodiscard]] static bool parse_value(string_view& str, bool& value) {
  auto valuei = 0;
  if (!parse_value(str, valuei)) return false;
  value = (bool)valuei;
  return true;
}
template <typename T, size_t N>
[[nodiscard]] static bool parse_value(string_view& str, array<T, N>& value) {
  for (auto i = 0; i < N; i++)
    if (!parse_value(str, value[i])) return false;
  return true;
}

template <typename T>
[[nodiscard]] static bool read_value(string_view& str, T& value) {
  if (str.size() < sizeof(value)) return false;
  memcpy(&value, str.data(), sizeof(T));
  str.remove_prefix(sizeof(T));
  return true;
}

// Write data from a file
template <typename T>
static void write_value(vector<byte>& data, const T& value) {
  if constexpr (sizeof(T) == 1) {
    data.push_back(*(byte*)&value);
  } else {
    data.insert(data.end(), (byte*)(&value), (byte*)(&value) + sizeof(T));
  }
}

template <typename T>
static T swap_endian(T value) {
  // https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
  static_assert(sizeof(char) == 1, "sizeof(char) == 1");
  union {
    T             value;
    unsigned char bytes[sizeof(T)];
  } source, dest;
  source.value = value;
  for (auto k = (size_t)0; k < sizeof(T); k++)
    dest.bytes[k] = source.bytes[sizeof(T) - k - 1];
  return dest.value;
}

template <typename T>
[[nodiscard]] static bool read_value(
    string_view& fs, T& value, bool big_endian) {
  if (!read_value(fs, value)) return false;
  if (big_endian) value = swap_endian(value);
  return true;
}

template <typename T>
static void write_value(vector<byte>& data, const T& value_, bool big_endian) {
  auto value = big_endian ? swap_endian(value_) : value_;
  return write_value(data, value);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PLY LOADER AND WRITER
// -----------------------------------------------------------------------------
namespace yocto {

// Load ply
bool load_ply(const string& filename, ply_model& ply, string& error) {
  // ply type names
  static auto type_map = unordered_map<string, ply_type>{{"char", ply_type::i8},
      {"short", ply_type::i16}, {"int", ply_type::i32}, {"long", ply_type::i64},
      {"uchar", ply_type::u8}, {"ushort", ply_type::u16},
      {"uint", ply_type::u32}, {"ulong", ply_type::u64},
      {"float", ply_type::f32}, {"double", ply_type::f64},
      {"int8", ply_type::i8}, {"int16", ply_type::i16},
      {"int32", ply_type::i32}, {"int64", ply_type::i64},
      {"uint8", ply_type::u8}, {"uint16", ply_type::u16},
      {"uint32", ply_type::u32}, {"uint64", ply_type::u64},
      {"float32", ply_type::f32}, {"float64", ply_type::f64}};

  // load data
  auto data = vector<byte>{};
  if (!load_binary(filename, data, error)) return false;

  // parsing checks
  auto first_line = true;
  auto end_header = false;

  // read header ---------------------------------------------
  auto data_view   = string_view{(const char*)data.data(), data.size()};
  auto str         = string_view{};
  auto parse_error = [&filename, &error]() {
    error = "cannot parse " + filename;
    return false;
  };
  auto read_error = [&filename, &error]() {
    error = "cannot read " + filename;
    return false;
  };
  while (read_line(data_view, str)) {
    // str
    remove_comment(str);
    skip_whitespace(str);
    if (str.empty()) continue;

    // get command
    auto cmd = ""s;
    if (!parse_value(str, cmd)) return parse_error();
    if (cmd.empty()) continue;

    // check magic number
    if (first_line) {
      if (cmd != "ply") return parse_error();
      first_line = false;
      continue;
    }

    // possible token values
    if (cmd == "ply") {
      if (!first_line) return parse_error();
    } else if (cmd == "format") {
      auto fmt = ""s;
      if (!parse_value(str, fmt)) return parse_error();
      if (fmt == "ascii") {
        ply.format = ply_format::ascii;
      } else if (fmt == "binary_little_endian") {
        ply.format = ply_format::binary_little_endian;
      } else if (fmt == "binary_big_endian") {
        ply.format = ply_format::binary_big_endian;
      } else {
        return parse_error();
      }
    } else if (cmd == "comment") {
      skip_whitespace(str);
      ply.comments.emplace_back(str);
    } else if (cmd == "obj_info") {
      skip_whitespace(str);
      // comment is the rest of the str
    } else if (cmd == "element") {
      auto& elem = ply.elements.emplace_back();
      if (!parse_value(str, elem.name)) return parse_error();
      if (!parse_value(str, elem.count)) return parse_error();
    } else if (cmd == "property") {
      if (ply.elements.empty()) return parse_error();
      auto& prop  = ply.elements.back().properties.emplace_back();
      auto  tname = ""s;
      if (!parse_value(str, tname)) return parse_error();
      if (tname == "list") {
        prop.is_list = true;
        if (!parse_value(str, tname)) return parse_error();
        auto itype = type_map.at(tname);
        if (itype != ply_type::u8) return parse_error();
        if (!parse_value(str, tname)) return parse_error();
        prop.type = type_map.at(tname);
      } else {
        prop.is_list = false;
        prop.type    = type_map.at(tname);
      }
      if (!parse_value(str, prop.name)) return parse_error();
    } else if (cmd == "end_header") {
      end_header = true;
      break;
    } else {
      return parse_error();
    }
  }

  // check exit
  if (!end_header) return parse_error();

  // allocate data ---------------------------------
  for (auto& element : ply.elements) {
    for (auto& property : element.properties) {
      auto count = property.is_list ? element.count * 3 : element.count;
      switch (property.type) {
        case ply_type::i8: property.data_i8.reserve(count); break;
        case ply_type::i16: property.data_i16.reserve(count); break;
        case ply_type::i32: property.data_i32.reserve(count); break;
        case ply_type::i64: property.data_i64.reserve(count); break;
        case ply_type::u8: property.data_u8.reserve(count); break;
        case ply_type::u16: property.data_u16.reserve(count); break;
        case ply_type::u32: property.data_u32.reserve(count); break;
        case ply_type::u64: property.data_u64.reserve(count); break;
        case ply_type::f32: property.data_f32.reserve(count); break;
        case ply_type::f64: property.data_f64.reserve(count); break;
      }
      if (property.is_list) property.ldata_u8.reserve(element.count);
    }
  }

  // read data -------------------------------------
  if (ply.format == ply_format::ascii) {
    for (auto& elem : ply.elements) {
      for (auto idx = (size_t)0; idx < elem.count; idx++) {
        if (!read_line(data_view, str)) return read_error();
        for (auto& prop : elem.properties) {
          if (prop.is_list)
            if (!parse_value(str, prop.ldata_u8.emplace_back()))
              return parse_error();
          auto vcount = prop.is_list ? prop.ldata_u8.back() : 1;
          for (auto i = 0; i < vcount; i++) {
            switch (prop.type) {
              case ply_type::i8:
                if (!parse_value(str, prop.data_i8.emplace_back()))
                  return parse_error();
                break;
              case ply_type::i16:
                if (!parse_value(str, prop.data_i16.emplace_back()))
                  return parse_error();
                break;
              case ply_type::i32:
                if (!parse_value(str, prop.data_i32.emplace_back()))
                  return parse_error();
                break;
              case ply_type::i64:
                if (!parse_value(str, prop.data_i64.emplace_back()))
                  return parse_error();
                break;
              case ply_type::u8:
                if (!parse_value(str, prop.data_u8.emplace_back()))
                  return parse_error();
                break;
              case ply_type::u16:
                if (!parse_value(str, prop.data_u16.emplace_back()))
                  return parse_error();
                break;
              case ply_type::u32:
                if (!parse_value(str, prop.data_u32.emplace_back()))
                  return parse_error();
                break;
              case ply_type::u64:
                if (!parse_value(str, prop.data_u64.emplace_back()))
                  return parse_error();
                break;
              case ply_type::f32:
                if (!parse_value(str, prop.data_f32.emplace_back()))
                  return parse_error();
                break;
              case ply_type::f64:
                if (!parse_value(str, prop.data_f64.emplace_back()))
                  return parse_error();
                break;
            }
          }
        }
      }
    }
  } else {
    auto big_endian = ply.format == ply_format::binary_big_endian;
    for (auto& elem : ply.elements) {
      for (auto idx = (size_t)0; idx < elem.count; idx++) {
        for (auto& prop : elem.properties) {
          if (prop.is_list) {
            if (!read_value(
                    data_view, prop.ldata_u8.emplace_back(), big_endian))
              return read_error();
          }
          auto vcount = prop.is_list ? prop.ldata_u8.back() : 1;
          for (auto i = 0; i < vcount; i++) {
            switch (prop.type) {
              case ply_type::i8:
                if (!read_value(
                        data_view, prop.data_i8.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::i16:
                if (!read_value(
                        data_view, prop.data_i16.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::i32:
                if (!read_value(
                        data_view, prop.data_i32.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::i64:
                if (!read_value(
                        data_view, prop.data_i64.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::u8:
                if (!read_value(
                        data_view, prop.data_u8.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::u16:
                if (!read_value(
                        data_view, prop.data_u16.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::u32:
                if (!read_value(
                        data_view, prop.data_u32.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::u64:
                if (!read_value(
                        data_view, prop.data_u64.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::f32:
                if (!read_value(
                        data_view, prop.data_f32.emplace_back(), big_endian))
                  return read_error();
                break;
              case ply_type::f64:
                if (!read_value(
                        data_view, prop.data_f64.emplace_back(), big_endian))
                  return read_error();
                break;
            }
          }
        }
      }
    }
  }

  // done
  return true;
}

// save ply
bool save_ply(const string& filename, const ply_model& ply, string& error) {
  // ply type names
  static auto type_map = unordered_map<ply_type, string>{{ply_type::i8, "char"},
      {ply_type::i16, "short"}, {ply_type::i32, "int"}, {ply_type::i64, "uint"},
      {ply_type::u8, "uchar"}, {ply_type::u16, "ushort"},
      {ply_type::u32, "uint"}, {ply_type::u64, "ulong"},
      {ply_type::f32, "float"}, {ply_type::f64, "double"}};
  static auto format_map = unordered_map<ply_format, string>{
      {ply_format::ascii, "ascii"},
      {ply_format::binary_little_endian, "binary_little_endian"},
      {ply_format::binary_big_endian, "binary_big_endian"}};

  // buffer
  auto header = string{};

  // header
  format_values(header, "ply\n");
  format_values(header, "format {} 1.0\n", format_map.at(ply.format));
  format_values(header, "comment Written by Yocto/GL\n");
  format_values(header, "comment https://github.com/xelatihy/yocto-gl\n");
  for (auto& comment : ply.comments)
    format_values(header, "comment {}\n", comment);
  for (auto& elem : ply.elements) {
    format_values(header, "element {} {}\n", elem.name, (uint64_t)elem.count);
    for (auto& prop : elem.properties) {
      if (prop.is_list) {
        format_values(header, "property list uchar {} {}\n",
            type_map[prop.type], prop.name);
      } else {
        format_values(
            header, "property {} {}\n", type_map[prop.type], prop.name);
      }
    }
  }

  format_values(header, "end_header\n");

  // properties
  if (ply.format == ply_format::ascii) {
    // buffer
    auto buffer = header;

    for (auto& elem : ply.elements) {
      auto cur = vector<size_t>(elem.properties.size(), 0);
      for (auto idx = (size_t)0; idx < elem.count; idx++) {
        for (auto& prop : elem.properties) {
          if (prop.is_list)
            format_values(buffer, "{} ", (int)prop.ldata_u8[idx]);
          auto vcount = prop.is_list ? prop.ldata_u8[idx] : 1;
          for (auto i = 0; i < vcount; i++) {
            switch (prop.type) {
              case ply_type::i8:
                format_values(buffer, "{} ", prop.data_i8[cur[idx]++]);
                break;
              case ply_type::i16:
                format_values(buffer, "{} ", prop.data_i16[cur[idx]++]);
                break;
              case ply_type::i32:
                format_values(buffer, "{} ", prop.data_i32[cur[idx]++]);
                break;
              case ply_type::i64:
                format_values(buffer, "{} ", prop.data_i64[cur[idx]++]);
                break;
              case ply_type::u8:
                format_values(buffer, "{} ", prop.data_u8[cur[idx]++]);
                break;
              case ply_type::u16:
                format_values(buffer, "{} ", prop.data_u16[cur[idx]++]);
                break;
              case ply_type::u32:
                format_values(buffer, "{} ", prop.data_u32[cur[idx]++]);
                break;
              case ply_type::u64:
                format_values(buffer, "{} ", prop.data_u64[cur[idx]++]);
                break;
              case ply_type::f32:
                format_values(buffer, "{} ", prop.data_f32[cur[idx]++]);
                break;
              case ply_type::f64:
                format_values(buffer, "{} ", prop.data_f64[cur[idx]++]);
                break;
            }
          }
          format_values(buffer, "\n");
        }
      }
    }

    // save file
    if (!save_text(filename, buffer, error)) return false;
  } else {
    // buffer
    auto buffer = vector<byte>{
        (const byte*)header.data(), (const byte*)header.data() + header.size()};

    auto big_endian = ply.format == ply_format::binary_big_endian;
    for (auto& elem : ply.elements) {
      auto cur = vector<size_t>(elem.properties.size(), 0);
      for (auto idx = (size_t)0; idx < elem.count; idx++) {
        for (auto pidx = (size_t)0; pidx < elem.properties.size(); pidx++) {
          auto& prop = elem.properties[pidx];
          if (prop.is_list) write_value(buffer, prop.ldata_u8[idx], big_endian);
          auto vcount = prop.is_list ? prop.ldata_u8[idx] : 1;
          for (auto i = 0; i < vcount; i++) {
            switch (prop.type) {
              case ply_type::i8:
                write_value(buffer, prop.data_i8[cur[pidx]++], big_endian);
                break;
              case ply_type::i16:
                write_value(buffer, prop.data_i16[cur[pidx]++], big_endian);
                break;
              case ply_type::i32:
                write_value(buffer, prop.data_i32[cur[pidx]++], big_endian);
                break;
              case ply_type::i64:
                write_value(buffer, prop.data_i64[cur[pidx]++], big_endian);
                break;
              case ply_type::u8:
                write_value(buffer, prop.data_u8[cur[pidx]++], big_endian);
                break;
              case ply_type::u16:
                write_value(buffer, prop.data_u16[cur[pidx]++], big_endian);
                break;
              case ply_type::u32:
                write_value(buffer, prop.data_u32[cur[pidx]++], big_endian);
                break;
              case ply_type::u64:
                write_value(buffer, prop.data_u64[cur[pidx]++], big_endian);
                break;
              case ply_type::f32:
                write_value(buffer, prop.data_f32[cur[pidx]++], big_endian);
                break;
              case ply_type::f64:
                write_value(buffer, prop.data_f64[cur[pidx]++], big_endian);
                break;
            }
          }
        }
      }
    }

    // save file
    if (!save_binary(filename, buffer, error)) return false;
  }

  // done
  return true;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR OBJ LOADER AND WRITER
// -----------------------------------------------------------------------------
namespace yocto {

[[nodiscard]] static bool parse_value(string_view& str, obj_vertex& value) {
  value = obj_vertex{0, 0, 0};
  if (!parse_value(str, value.position)) return false;
  if (!str.empty() && str.front() == '/') {
    str.remove_prefix(1);
    if (!str.empty() && str.front() == '/') {
      str.remove_prefix(1);
      if (!parse_value(str, value.normal)) return false;
    } else {
      if (!parse_value(str, value.texcoord)) return false;
      if (!str.empty() && str.front() == '/') {
        str.remove_prefix(1);
        if (!parse_value(str, value.normal)) return false;
      }
    }
  }
  return true;
}

// Input for OBJ textures
[[nodiscard]] static bool parse_value(string_view& str, obj_texture& info) {
  // initialize
  info = obj_texture();

  // get tokens
  auto tokens = vector<string>();
  skip_whitespace(str);
  while (!str.empty()) {
    auto token = ""s;
    if (!parse_value(str, token)) return false;
    tokens.push_back(token);
    skip_whitespace(str);
  }
  if (tokens.empty()) return false;

  // texture name
  info.path = tokens.back();
  for (auto& c : info.path)
    if (c == '\\') c = '/';

  // texture params
  auto last = string();
  for (auto i = 0; i < (int)tokens.size() - 1; i++) {
    if (tokens[i] == "-bm") info.scale = (float)atof(tokens[i + 1].c_str());
    if (tokens[i] == "-clamp") info.clamp = true;
  }

  return true;
}

// Read obj
static bool load_mtl(const string& filename, obj_model& obj, string& error) {
  // texture map
  auto texture_map = unordered_map<string, int>{};
  auto texture_id  = 0;
  for (auto& texture : obj.textures) texture_map[texture.path] = texture_id++;
  auto parse_texture = [&texture_map, &obj](string_view& str, int& texture_id) {
    auto texture_path = obj_texture{};
    if (!parse_value(str, texture_path)) return false;
    auto texture_it = texture_map.find(texture_path.path);
    if (texture_it == texture_map.end()) {
      auto& texture             = obj.textures.emplace_back();
      texture.path              = texture_path.path;
      texture_id                = (int)obj.textures.size() - 1;
      texture_map[texture.path] = texture_id;
    } else {
      texture_id = texture_it->second;
    }
    return true;
  };

  // load data
  auto data = string{};
  if (!load_text(filename, data, error)) return false;

  // init parsing
  obj.materials.emplace_back();

  // read the file str by str
  auto data_view   = string_view{data.data(), data.size()};
  auto str         = string_view{};
  auto parse_error = [&filename, &error]() {
    error = "cannot parse " + filename;
    return false;
  };
  while (read_line(data_view, str)) {
    // str
    remove_comment(str);
    skip_whitespace(str);
    if (str.empty()) continue;

    // get command
    auto cmd = ""s;
    if (!parse_value(str, cmd)) return parse_error();
    if (cmd.empty()) continue;

    // grab material
    auto& material = obj.materials.back();

    // possible token values
    if (cmd == "newmtl") {
      auto& material = obj.materials.emplace_back();
      if (!parse_value(str, material.name)) return parse_error();
    } else if (cmd == "illum") {
      if (!parse_value(str, material.illum)) return parse_error();
    } else if (cmd == "Ke") {
      if (!parse_value(str, material.emission)) return parse_error();
    } else if (cmd == "Ka") {
      if (!parse_value(str, material.ambient)) return parse_error();
    } else if (cmd == "Kd") {
      if (!parse_value(str, material.diffuse)) return parse_error();
    } else if (cmd == "Ks") {
      if (!parse_value(str, material.specular)) return parse_error();
    } else if (cmd == "Kt") {
      if (!parse_value(str, material.transmission)) return parse_error();
    } else if (cmd == "Tf") {
      if (!parse_value(str, material.transmission)) return parse_error();
      material.transmission = {std::max(1 - material.transmission[0], 0.0f),
          std::max(1 - material.transmission[1], 0.0f),
          std::max(1 - material.transmission[2], 0.0f)};
      if (std::max(material.transmission[0],
              std::max(material.transmission[1], material.transmission[2])) <
          0.001f)
        material.transmission = {0, 0, 0};
    } else if (cmd == "Tr") {
      if (!parse_value(str, material.opacity)) return parse_error();
      material.opacity = 1 - material.opacity;
    } else if (cmd == "Ns") {
      if (!parse_value(str, material.exponent)) return parse_error();
    } else if (cmd == "d") {
      if (!parse_value(str, material.opacity)) return parse_error();
    } else if (cmd == "map_Ke") {
      if (!parse_texture(str, material.emission_tex)) return parse_error();
    } else if (cmd == "map_Ka") {
      if (!parse_texture(str, material.ambient_tex)) return parse_error();
    } else if (cmd == "map_Kd") {
      if (!parse_texture(str, material.diffuse_tex)) return parse_error();
    } else if (cmd == "map_Ks") {
      if (!parse_texture(str, material.specular_tex)) return parse_error();
    } else if (cmd == "map_Tr") {
      if (!parse_texture(str, material.transmission_tex)) return parse_error();
    } else if (cmd == "map_d" || cmd == "map_Tr") {
      if (!parse_texture(str, material.opacity_tex)) return parse_error();
    } else if (cmd == "map_bump" || cmd == "bump") {
      if (!parse_texture(str, material.bump_tex)) return parse_error();
    } else if (cmd == "map_disp" || cmd == "disp") {
      if (!parse_texture(str, material.displacement_tex)) return parse_error();
    } else if (cmd == "map_norm" || cmd == "norm") {
      if (!parse_texture(str, material.normal_tex)) return parse_error();
    } else {
      continue;
    }
  }

  // remove placeholder material
  obj.materials.erase(obj.materials.begin());

  // done
  return true;
}

// Read obj
static bool load_obx(const string& filename, obj_model& obj, string& error) {
  // texture map
  auto texture_map = unordered_map<string, int>{};
  auto texture_id  = 0;
  for (auto& texture : obj.textures) texture_map[texture.path] = texture_id++;
  auto parse_texture = [&texture_map, &obj](string_view& str, int& texture_id) {
    auto texture_path = obj_texture{};
    if (!parse_value(str, texture_path)) return false;
    auto texture_it = texture_map.find(texture_path.path);
    if (texture_it == texture_map.end()) {
      auto& texture             = obj.textures.emplace_back();
      texture.path              = texture_path.path;
      texture_id                = (int)obj.textures.size() - 1;
      texture_map[texture.path] = texture_id;
    } else {
      texture_id = texture_it->second;
    }
    return true;
  };

  // load data
  auto data = string{};
  if (!load_text(filename, data, error)) return false;

  // init parsing
  obj.cameras.emplace_back();
  obj.environments.emplace_back();

  // read the file str by str
  auto data_view   = string_view{data.data(), data.size()};
  auto str         = string_view{};
  auto parse_error = [&filename, &error]() {
    error = "cannot parse " + filename;
    return false;
  };
  while (read_line(data_view, str)) {
    // str
    remove_comment(str);
    skip_whitespace(str);
    if (str.empty()) continue;

    // get command
    auto cmd = ""s;
    if (!parse_value(str, cmd)) return parse_error();
    if (cmd.empty()) continue;

    // grab elements
    auto& camera      = obj.cameras.back();
    auto& environment = obj.environments.back();

    // read values
    if (cmd == "newCam") {
      auto& camera = obj.cameras.emplace_back();
      if (!parse_value(str, camera.name)) return parse_error();
    } else if (cmd == "Co") {
      if (!parse_value(str, camera.ortho)) return parse_error();
    } else if (cmd == "Ca") {
      if (!parse_value(str, camera.aspect)) return parse_error();
    } else if (cmd == "Cl") {
      if (!parse_value(str, camera.lens)) return parse_error();
    } else if (cmd == "Cs") {
      if (!parse_value(str, camera.film)) return parse_error();
    } else if (cmd == "Cf") {
      if (!parse_value(str, camera.focus)) return parse_error();
    } else if (cmd == "Cp") {
      if (!parse_value(str, camera.aperture)) return parse_error();
    } else if (cmd == "Cx") {
      if (!parse_value(str, camera.frame)) return parse_error();
    } else if (cmd == "Ct") {
      auto lookat = array<array<float, 3>, 3>{};
      if (!parse_value(str, lookat)) return parse_error();
      camera.frame = flatten(lookat_frame(lookat[0], lookat[1], lookat[2]));
      if (camera.focus == 0)
        camera.focus = length({lookat[1][0] - lookat[0][0],
            lookat[1][1] - lookat[0][1], lookat[1][2] - lookat[0][2]});
    } else if (cmd == "newEnv") {
      auto& environment = obj.environments.emplace_back();
      if (!parse_value(str, environment.name)) return parse_error();
    } else if (cmd == "Ee") {
      if (!parse_value(str, environment.emission)) return parse_error();
    } else if (cmd == "map_Ee") {
      if (!parse_texture(str, environment.emission_tex)) return parse_error();
    } else if (cmd == "Ex") {
      if (!parse_value(str, environment.frame)) return parse_error();
    } else if (cmd == "Et") {
      auto lookat = array<array<float, 3>, 3>{};
      if (!parse_value(str, lookat)) return parse_error();
      environment.frame = flatten(
          lookat_frame(lookat[0], lookat[1], lookat[2], true));
    } else {
      // unused
    }
  }

  // remove placeholders
  obj.cameras.erase(obj.cameras.begin());
  obj.environments.erase(obj.environments.begin());

  // done
  return true;
}

// Read obj
bool load_obj(const string& filename, obj_model& obj, string& error,
    bool face_varying, bool split_materials) {
  // load data
  auto data = string{};
  if (!load_text(filename, data, error)) return false;

  // parsing state
  auto opositions   = vector<array<float, 3>>{};
  auto onormals     = vector<array<float, 3>>{};
  auto otexcoords   = vector<array<float, 2>>{};
  auto oname        = ""s;
  auto gname        = ""s;
  auto mtllibs      = vector<string>{};
  auto material_map = unordered_map<string, int>{};
  int  cur_material = -1;
  auto cur_shape    = (obj_shape*)nullptr;
  auto cur_shapes   = unordered_map<int, int>{};

  // initialize obj
  obj       = {};
  cur_shape = &obj.shapes.emplace_back();
  if (split_materials) {
    cur_shapes = {{cur_material, (int)obj.shapes.size() - 1}};
  }

  // read the file str by str
  auto data_view   = string_view{data.data(), data.size()};
  auto str         = string_view{};
  auto parse_error = [&filename, &error]() {
    error = "cannot parse " + filename;
    return false;
  };
  auto dependent_error = [&filename, &error]() {
    error = "cannot load " + filename + " since " + error;
    return false;
  };
  while (read_line(data_view, str)) {
    // str
    remove_comment(str);
    skip_whitespace(str);
    if (str.empty()) continue;

    // get command
    auto cmd = ""s;
    if (!parse_value(str, cmd)) return parse_error();
    if (cmd.empty()) continue;

    // possible token values
    if (cmd == "v") {
      if (!parse_value(str, opositions.emplace_back())) return parse_error();
    } else if (cmd == "vn") {
      if (!parse_value(str, onormals.emplace_back())) return parse_error();
    } else if (cmd == "vt") {
      if (!parse_value(str, otexcoords.emplace_back())) return parse_error();
    } else if (cmd == "f" || cmd == "l" || cmd == "p") {
      // elemnet type
      auto etype = (cmd == "f")   ? obj_etype::face
                   : (cmd == "l") ? obj_etype::line
                                  : obj_etype::point;
      // grab shape and add element
      auto& shape   = *cur_shape;
      auto& element = shape.elements.emplace_back();
      if (cur_material < 0) {
        auto& material              = obj.materials.emplace_back();
        material.name               = "__default__";
        material.diffuse            = {0.8f, 0.8f, 0.8f};
        cur_material                = 0;
        material_map[material.name] = 0;
      }
      element.material = cur_material;
      element.etype    = etype;
      // parse vertices
      skip_whitespace(str);
      while (!str.empty()) {
        auto vert = obj_vertex{};
        if (!parse_value(str, vert)) return parse_error();
        if (vert.position == 0) break;
        if (vert.position < 0)
          vert.position = (int)opositions.size() + vert.position + 1;
        if (vert.texcoord < 0)
          vert.texcoord = (int)otexcoords.size() + vert.texcoord + 1;
        if (vert.normal < 0)
          vert.normal = (int)onormals.size() + vert.normal + 1;
        shape.vertices.push_back(vert);
        element.size += 1;
        skip_whitespace(str);
      }
    } else if (cmd == "o" || cmd == "g") {
      skip_whitespace(str);
      auto& name = cmd == "o" ? oname : gname;
      if (str.empty()) {
        name = "";
      } else {
        if (!parse_value(str, name)) return parse_error();
      }
      if (split_materials) {
        cur_shape       = &obj.shapes.emplace_back();
        cur_shapes      = {{cur_material, (int)obj.shapes.size() - 1}};
        cur_shape->name = oname + gname;
      } else {
        if (!cur_shape->vertices.empty()) {
          cur_shape = &obj.shapes.emplace_back();
        }
        cur_shape->name = oname + gname;
      }
    } else if (cmd == "usemtl") {
      auto mname = string{};
      if (!parse_value(str, mname)) return parse_error();
      auto material_it = material_map.find(mname);
      if (material_it == material_map.end()) return parse_error();
      if (split_materials && cur_material != material_it->second) {
        cur_material  = material_it->second;
        auto shape_it = cur_shapes.find(cur_material);
        if (shape_it == cur_shapes.end()) {
          cur_shape                = &obj.shapes.emplace_back();
          cur_shapes[cur_material] = (int)obj.shapes.size() - 1;
          cur_shape->name          = oname + gname;
        } else {
          cur_shape = &obj.shapes.at(shape_it->second);
        }
      } else {
        cur_material = material_it->second;
      }
    } else if (cmd == "mtllib") {
      auto mtllib = ""s;
      if (!parse_value(str, mtllib)) return parse_error();
      if (std::find(mtllibs.begin(), mtllibs.end(), mtllib) == mtllibs.end()) {
        mtllibs.push_back(mtllib);
        if (!load_mtl(path_join(path_dirname(filename), mtllib), obj, error))
          return dependent_error();
        auto material_id = 0;
        for (auto& material : obj.materials)
          material_map[material.name] = material_id++;
      }
    } else {
      // unused
    }
  }

  // remove empty shapes if splitting by materials
  if (split_materials) {
    obj.shapes.erase(
        std::remove_if(obj.shapes.begin(), obj.shapes.end(),
            [](const obj_shape& shape) { return shape.elements.empty(); }),
        obj.shapes.end());
  }

  // convert vertex data
  if (face_varying) {
    auto ipositions = vector<int>{};
    auto inormals   = vector<int>{};
    auto itexcoords = vector<int>{};
    for (auto& shape : obj.shapes) {
      ipositions.assign(opositions.size() + 1, 0);
      inormals.assign(onormals.size() + 1, 0);
      itexcoords.assign(otexcoords.size() + 1, 0);
      for (auto& vertex : shape.vertices) {
        if (vertex.position != 0 && ipositions[vertex.position] == 0) {
          shape.positions.push_back(opositions[vertex.position - 1]);
          ipositions[vertex.position] = (int)shape.positions.size();
        }
        if (vertex.normal != 0 && inormals[vertex.normal] == 0) {
          shape.normals.push_back(onormals[vertex.normal - 1]);
          inormals[vertex.normal] = (int)shape.normals.size();
        }
        if (vertex.texcoord != 0 && itexcoords[vertex.texcoord] == 0) {
          shape.texcoords.push_back(otexcoords[vertex.texcoord - 1]);
          itexcoords[vertex.texcoord] = (int)shape.texcoords.size();
        }
        vertex.position = ipositions[vertex.position];
        vertex.normal   = inormals[vertex.normal];
        vertex.texcoord = itexcoords[vertex.texcoord];
      }
    }
  } else {
    auto vertex_map = unordered_map<obj_vertex, obj_vertex>{};
    for (auto& shape : obj.shapes) {
      vertex_map.clear();
      for (auto& vertex : shape.vertices) {
        auto vertex_it = vertex_map.find(vertex);
        if (vertex_it == vertex_map.end()) {
          auto new_vertex = vertex;
          auto index      = (int)vertex_map.size();
          if (vertex.position > 0) {
            shape.positions.push_back(opositions[vertex.position - 1]);
            new_vertex.position = index + 1;
          }
          if (vertex.normal > 0) {
            shape.normals.push_back(onormals[vertex.normal - 1]);
            new_vertex.normal = index + 1;
          }
          if (vertex.texcoord > 0) {
            shape.texcoords.push_back(otexcoords[vertex.texcoord - 1]);
            new_vertex.texcoord = index + 1;
          }
          vertex_map[vertex] = new_vertex;
          vertex             = new_vertex;
        } else {
          vertex = vertex_it->second;
        }
      }
    }
  }

  // load extensions
  auto extfilename = replace_extension(filename, ".obx");
  if (path_exists(extfilename)) {
    if (!load_obx(extfilename, obj, error)) return dependent_error();
  }

  // done
  return true;
}

// Read obj
bool load_obj(const string& filename, obj_shape& shape, string& error,
    bool face_varying) {
  // load data
  auto data = string{};
  if (!load_text(filename, data, error)) return false;

  // parsing state
  auto material_map = unordered_map<string, int>{};
  int  cur_material = -1;

  // initialize obj
  shape = {};

  // read the file str by str
  auto data_view   = string_view{data.data(), data.size()};
  auto str         = string_view{};
  auto parse_error = [&filename, &error]() {
    error = "cannot parse " + filename;
    return false;
  };
  while (read_line(data_view, str)) {
    // str
    remove_comment(str);
    skip_whitespace(str);
    if (str.empty()) continue;

    // get command
    auto cmd = ""s;
    if (!parse_value(str, cmd)) return parse_error();
    if (cmd.empty()) continue;

    // possible token values
    if (cmd == "v") {
      if (!parse_value(str, shape.positions.emplace_back()))
        return parse_error();
    } else if (cmd == "vn") {
      if (!parse_value(str, shape.normals.emplace_back())) return parse_error();
    } else if (cmd == "vt") {
      if (!parse_value(str, shape.texcoords.emplace_back()))
        return parse_error();
    } else if (cmd == "f" || cmd == "l" || cmd == "p") {
      // elemnet type
      auto etype = (cmd == "f")   ? obj_etype::face
                   : (cmd == "l") ? obj_etype::line
                                  : obj_etype::point;
      // grab shape and add element
      auto& element    = shape.elements.emplace_back();
      element.material = cur_material;
      element.etype    = etype;
      // parse vertices
      skip_whitespace(str);
      while (!str.empty()) {
        auto vert = obj_vertex{};
        if (!parse_value(str, vert)) return parse_error();
        if (vert.position == 0) break;
        if (vert.position < 0)
          vert.position = (int)shape.positions.size() + vert.position + 1;
        if (vert.texcoord < 0)
          vert.texcoord = (int)shape.texcoords.size() + vert.texcoord + 1;
        if (vert.normal < 0)
          vert.normal = (int)shape.normals.size() + vert.normal + 1;
        shape.vertices.push_back(vert);
        element.size += 1;
        skip_whitespace(str);
      }
    } else if (cmd == "usemtl") {
      auto mname = string{};
      if (!parse_value(str, mname)) return parse_error();
      auto material_it = material_map.find(mname);
      if (material_it == material_map.end()) {
        cur_material        = (int)material_map.size();
        material_map[mname] = cur_material;
      } else {
        cur_material = material_it->second;
      }
    } else {
      // unused
    }
  }

  // convert vertex data
  if (!face_varying) {
    auto opositions = vector<array<float, 3>>{};
    auto onormals   = vector<array<float, 3>>{};
    auto otexcoords = vector<array<float, 2>>{};
    shape.positions.swap(opositions);
    shape.normals.swap(onormals);
    shape.texcoords.swap(otexcoords);
    auto vertex_map = unordered_map<obj_vertex, obj_vertex>{};
    for (auto& vertex : shape.vertices) {
      auto vertex_it = vertex_map.find(vertex);
      if (vertex_it == vertex_map.end()) {
        auto new_vertex = vertex;
        auto index      = (int)vertex_map.size();
        if (vertex.position > 0) {
          shape.positions.push_back(opositions[vertex.position - 1]);
          new_vertex.position = index + 1;
        }
        if (vertex.normal > 0) {
          shape.normals.push_back(onormals[vertex.normal - 1]);
          new_vertex.normal = index + 1;
        }
        if (vertex.texcoord > 0) {
          shape.texcoords.push_back(otexcoords[vertex.texcoord - 1]);
          new_vertex.texcoord = index + 1;
        }
        vertex_map[vertex] = new_vertex;
        vertex             = new_vertex;
      } else {
        vertex = vertex_it->second;
      }
    }
  }

  // done
  return true;
}

// Format values
static void format_value(string& str, const obj_vertex& value) {
  format_value(str, value.position);
  if (value.texcoord != 0) {
    str += "/";
    format_value(str, value.texcoord);
    if (value.normal != 0) {
      str += "/";
      format_value(str, value.normal);
    }
  } else if (value.normal != 0) {
    str += "//";
    format_value(str, value.normal);
  }
}

// Save obj
static bool save_mtl(
    const string& filename, const obj_model& obj, string& error) {
  // buffer
  auto buffer = string{};

  // save comments
  format_values(buffer, "#\n");
  format_values(buffer, "# Written by Yocto/GL\n");
  format_values(buffer, "# https://github.com/xelatihy/yocto-gl\n");
  format_values(buffer, "#\n\n");
  for (auto& comment : obj.comments) {
    format_values(buffer, "# {}\n", comment);
  }
  format_values(buffer, "\n");

  // write material
  for (auto& material : obj.materials) {
    format_values(buffer, "newmtl {}\n", material.name);
    format_values(buffer, "illum {}\n", material.illum);
    if (material.emission != array<float, 3>{0, 0, 0})
      format_values(buffer, "Ke {}\n", material.emission);
    if (material.ambient != array<float, 3>{0, 0, 0})
      format_values(buffer, "Ka {}\n", material.ambient);
    format_values(buffer, "Kd {}\n", material.diffuse);
    format_values(buffer, "Ks {}\n", material.specular);
    if (material.reflection != array<float, 3>{0, 0, 0})
      format_values(buffer, "Kr {}\n", material.reflection);
    if (material.transmission != array<float, 3>{0, 0, 0})
      format_values(buffer, "Kt {}\n", material.transmission);
    format_values(buffer, "Ns {}\n", (int)material.exponent);
    if (material.opacity != 1)
      format_values(buffer, "d {}\n", material.opacity);
    if (material.emission_tex >= 0)
      format_values(
          buffer, "map_Ke {}\n", obj.textures[material.emission_tex].path);
    if (material.diffuse_tex >= 0)
      format_values(
          buffer, "map_Kd {}\n", obj.textures[material.diffuse_tex].path);
    if (material.specular_tex >= 0)
      format_values(
          buffer, "map_Ks {}\n", obj.textures[material.specular_tex].path);
    if (material.transmission_tex >= 0)
      format_values(
          buffer, "map_Kt {}\n", obj.textures[material.transmission_tex].path);
    if (material.reflection_tex >= 0)
      format_values(
          buffer, "map_Kr {}\n", obj.textures[material.reflection_tex].path);
    if (material.exponent_tex >= 0)
      format_values(
          buffer, "map_Ns {}\n", obj.textures[material.exponent_tex].path);
    if (material.opacity_tex >= 0)
      format_values(
          buffer, "map_d {}\n", obj.textures[material.opacity_tex].path);
    if (material.bump_tex >= 0)
      format_values(
          buffer, "map_bump {}\n", obj.textures[material.bump_tex].path);
    if (material.displacement_tex >= 0)
      format_values(buffer, "map_disp {}\n",
          obj.textures[material.displacement_tex].path);
    if (material.normal_tex >= 0)
      format_values(
          buffer, "map_norm {}\n", obj.textures[material.normal_tex].path);
    format_values(buffer, "\n");
  }

  // save file
  if (!save_text(filename, buffer, error)) return false;

  // done
  return true;
}

// Save obj
static bool save_obx(
    const string& filename, const obj_model& obj, string& error) {
  // buffer
  auto buffer = string{};

  // save comments
  format_values(buffer, "#\n");
  format_values(buffer, "# Written by Yocto/GL\n");
  format_values(buffer, "# https://github.com/xelatihy/yocto-gl\n");
  format_values(buffer, "#\n\n");
  for (auto& comment : obj.comments) {
    format_values(buffer, "# {}\n", comment);
  }
  format_values(buffer, "\n");

  // cameras
  for (auto& camera : obj.cameras) {
    format_values(buffer, "newCam {}\n", camera.name);
    format_values(buffer, "  Co {}\n", camera.ortho);
    format_values(buffer, "  Ca {}\n", camera.aspect);
    format_values(buffer, "  Cl {}\n", camera.lens);
    format_values(buffer, "  Cs {}\n", camera.film);
    format_values(buffer, "  Cf {}\n", camera.focus);
    format_values(buffer, "  Cp {}\n", camera.aperture);
    format_values(buffer, "  Cx {}\n", camera.frame);
  }

  // environments
  for (auto& environment : obj.environments) {
    format_values(buffer, "newEnv {}\n", environment.name);
    format_values(buffer, "  Ee {}\n", environment.emission);
    if (environment.emission_tex >= 0) {
      format_values(
          buffer, "  map_Ee {}\n", obj.textures[environment.emission_tex].path);
    }
    format_values(buffer, "  Ex {}\n", environment.frame);
  }

  // save file
  if (!save_text(filename, buffer, error)) return false;

  // done
  return true;
}

// Save obj
bool save_obj(const string& filename, const obj_model& obj, string& error) {
  // buffer
  auto buffer = string{};

  // save comments
  format_values(buffer, "#\n");
  format_values(buffer, "# Written by Yocto/GL\n");
  format_values(buffer, "# https://github.com/xelatihy/yocto-gl\n");
  format_values(buffer, "#\n\n");
  for (auto& comment : obj.comments) {
    format_values(buffer, "# {}\n", comment);
  }
  format_values(buffer, "\n");

  // save material library
  if (!obj.materials.empty()) {
    format_values(buffer, "mtllib {}\n\n",
        replace_extension(path_filename(filename), ".mtl"));
  }

  // save objects
  auto vert_size = obj_vertex{0, 0, 0};
  for (auto& shape : obj.shapes) {
    format_values(buffer, "o {}\n", shape.name);
    for (auto& p : shape.positions) format_values(buffer, "v {}\n", p);
    for (auto& n : shape.normals) format_values(buffer, "vn {}\n", n);
    for (auto& t : shape.texcoords) format_values(buffer, "vt {}\n", t);
    auto cur_material = -1, cur_vertex = 0;
    for (auto& element : shape.elements) {
      if (!obj.materials.empty() && cur_material != element.material) {
        format_values(
            buffer, "usemtl {}\n", obj.materials[element.material].name);
        cur_material = element.material;
      }
      if (element.etype == obj_etype::face) {
        format_values(buffer, "{}", "f");
      } else if (element.etype == obj_etype::line) {
        format_values(buffer, "{}", "l");
      } else if (element.etype == obj_etype::point) {
        format_values(buffer, "{}", "p");
      }
      for (auto c = 0; c < element.size; c++) {
        auto vert = shape.vertices[cur_vertex++];
        if (vert.position != 0) vert.position += vert_size.position;
        if (vert.normal != 0) vert.normal += vert_size.normal;
        if (vert.texcoord != 0) vert.texcoord += vert_size.texcoord;
        format_values(buffer, " {}", vert);
      }
      format_values(buffer, "\n");
    }
    format_values(buffer, "\n");
    vert_size.position += (int)shape.positions.size();
    vert_size.normal += (int)shape.normals.size();
    vert_size.texcoord += (int)shape.texcoords.size();
  }

  // save file
  if (!save_text(filename, buffer, error)) return false;

  auto dependent_error = [&filename, &error]() {
    error = "cannot save " + filename + " since " + error;
    return false;
  };

  // save mtl
  if (!obj.materials.empty()) {
    if (!save_mtl(replace_extension(filename, ".mtl"), obj, error))
      return dependent_error();
  }

  // save obx
  if (!obj.cameras.empty() || !obj.environments.empty()) {
    if (!save_obx(replace_extension(filename, ".obx"), obj, error))
      return dependent_error();
  }

  // done
  return true;
}

// Save obj
bool save_obj(const string& filename, const obj_shape& shape, string& error) {
  // buffer
  auto buffer = string{};

  // save comments
  format_values(buffer, "#\n");
  format_values(buffer, "# Written by Yocto/GL\n");
  format_values(buffer, "# https://github.com/xelatihy/yocto-gl\n");
  format_values(buffer, "#\n\n");
  format_values(buffer, "\n");

  // save objects
  format_values(buffer, "o {}\n", shape.name);
  for (auto& p : shape.positions) format_values(buffer, "v {}\n", p);
  for (auto& n : shape.normals) format_values(buffer, "vn {}\n", n);
  for (auto& t : shape.texcoords) format_values(buffer, "vt {}\n", t);
  auto cur_material = -1, cur_vertex = 0;
  for (auto& element : shape.elements) {
    if (cur_material != element.material) {
      format_values(
          buffer, "usemtl {}\n", "material" + std::to_string(element.material));
      cur_material = element.material;
    }
    if (element.etype == obj_etype::face) {
      format_values(buffer, "{}", "f");
    } else if (element.etype == obj_etype::line) {
      format_values(buffer, "{}", "l");
    } else if (element.etype == obj_etype::point) {
      format_values(buffer, "{}", "p");
    }
    for (auto c = 0; c < element.size; c++) {
      auto& vert = shape.vertices[cur_vertex++];
      format_values(buffer, " {}", vert);
    }
    format_values(buffer, "\n");
  }

  // save file
  if (!save_text(filename, buffer, error)) return false;

  // done
  return true;
}

// Get obj shape.
void get_positions(const obj_shape& shape, vector<array<float, 3>>& positions) {
  positions = shape.positions;
}
void get_normals(const obj_shape& shape, vector<array<float, 3>>& normals) {
  normals = shape.normals;
}
void get_texcoords(
    const obj_shape& shape, vector<array<float, 2>>& texcoords, bool flipv) {
  texcoords = shape.texcoords;
  if (flipv) {
    for (auto& texcoord : texcoords) texcoord = {texcoord[0], 1 - texcoord[1]};
  }
}
void get_faces(const obj_shape& shape, vector<array<int, 3>>& triangles,
    vector<array<int, 4>>& quads, vector<int>& materials) {
  if (has_quads(shape)) {
    get_quads(shape, quads, materials);
  } else {
    get_triangles(shape, triangles, materials);
  }
}
void get_triangles(const obj_shape& shape, vector<array<int, 3>>& triangles,
    vector<int>& materials) {
  triangles.clear();
  materials.clear();
  triangles.reserve(shape.elements.size());
  materials.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::face) continue;
    for (auto c = 2; c < element.size; c++) {
      triangles.push_back({shape.vertices[cur + 0].position - 1,
          shape.vertices[cur + c - 1].position - 1,
          shape.vertices[cur + c].position - 1});
      materials.push_back(element.material);
    }
    cur += element.size;
  }
}
void get_quads(const obj_shape& shape, vector<array<int, 4>>& quads,
    vector<int>& materials) {
  quads.clear();
  materials.clear();
  quads.reserve(shape.elements.size());
  materials.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::face) continue;
    if (element.size == 4) {
      quads.push_back({shape.vertices[cur + 0].position - 1,
          shape.vertices[cur + 1].position - 1,
          shape.vertices[cur + 2].position - 1,
          shape.vertices[cur + 3].position - 1});
      materials.push_back(element.material);
    } else {
      for (auto c = 2; c < element.size; c++) {
        quads.push_back({shape.vertices[cur + 0].position - 1,
            shape.vertices[cur + c - 1].position - 1,
            shape.vertices[cur + c].position - 1,
            shape.vertices[cur + c].position - 1});
        materials.push_back(element.material);
      }
    }
    cur += element.size;
  }
}
void get_lines(const obj_shape& shape, vector<array<int, 2>>& lines,
    vector<int>& materials) {
  lines.clear();
  materials.clear();
  lines.reserve(shape.elements.size());
  materials.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::line) continue;
    for (auto c = 1; c < element.size; c++) {
      lines.push_back({shape.vertices[cur + c - 1].position - 1,
          shape.vertices[cur + c].position - 1});
      materials.push_back(element.material);
    }
    cur += element.size;
  }
}
void get_points(
    const obj_shape& shape, vector<int>& points, vector<int>& materials) {
  points.clear();
  materials.clear();
  points.reserve(shape.elements.size());
  materials.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::point) continue;
    for (auto c = 0; c < element.size; c++) {
      points.push_back({shape.vertices[cur + 0].position - 1});
      materials.push_back(element.material);
    }
    cur += element.size;
  }
}
void get_fvquads(const obj_shape& shape, vector<array<int, 4>>& quadspos,
    vector<array<int, 4>>& quadsnorm, vector<array<int, 4>>& quadstexcoord,
    vector<int>& materials) {
  quadspos.clear();
  quadsnorm.clear();
  quadstexcoord.clear();
  materials.clear();
  quadspos.reserve(shape.elements.size());
  quadsnorm.reserve(shape.elements.size());
  quadstexcoord.reserve(shape.elements.size());
  materials.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::face) continue;
    if (element.size == 4) {
      if (shape.vertices[0].position != 0)
        quadspos.push_back({shape.vertices[cur + 0].position - 1,
            shape.vertices[cur + 1].position - 1,
            shape.vertices[cur + 2].position - 1,
            shape.vertices[cur + 3].position - 1});
      if (shape.vertices[0].normal != 0)
        quadsnorm.push_back({shape.vertices[cur + 0].normal - 1,
            shape.vertices[cur + 1].normal - 1,
            shape.vertices[cur + 2].normal - 1,
            shape.vertices[cur + 3].normal - 1});
      if (shape.vertices[0].texcoord != 0)
        quadstexcoord.push_back({shape.vertices[cur + 0].texcoord - 1,
            shape.vertices[cur + 1].texcoord - 1,
            shape.vertices[cur + 2].texcoord - 1,
            shape.vertices[cur + 3].texcoord - 1});
      materials.push_back(element.material);
    } else {
      for (auto c = 2; c < element.size; c++) {
        if (shape.vertices[0].position != 0)
          quadspos.push_back({shape.vertices[cur + 0].position - 1,
              shape.vertices[cur + c - 1].position - 1,
              shape.vertices[cur + c].position - 1,
              shape.vertices[cur + c].position - 1});
        if (shape.vertices[0].normal != 0)
          quadsnorm.push_back({shape.vertices[cur + 0].normal - 1,
              shape.vertices[cur + c - 1].normal - 1,
              shape.vertices[cur + c].normal - 1,
              shape.vertices[cur + c].normal - 1});
        if (shape.vertices[0].texcoord != 0)
          quadstexcoord.push_back({shape.vertices[cur + 0].texcoord - 1,
              shape.vertices[cur + c - 1].texcoord - 1,
              shape.vertices[cur + c].texcoord - 1,
              shape.vertices[cur + c].texcoord - 1});
        materials.push_back(element.material);
      }
    }
    cur += element.size;
  }
}
void get_faces(const obj_shape& shape, int material,
    vector<array<int, 3>>& triangles, vector<array<int, 4>>& quads) {
  if (has_quads(shape)) {
    get_quads(shape, material, quads);
  } else {
    get_triangles(shape, material, triangles);
  }
}
void get_triangles(
    const obj_shape& shape, int material, vector<array<int, 3>>& triangles) {
  triangles.clear();
  if (shape.elements.empty()) return;
  triangles.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::face) continue;
    if (element.material != material) continue;
    for (auto c = 2; c < element.size; c++) {
      triangles.push_back({shape.vertices[cur + 0].position - 1,
          shape.vertices[cur + c - 1].position - 1,
          shape.vertices[cur + c].position - 1});
    }
    cur += element.size;
  }
}
void get_quads(
    const obj_shape& shape, int material, vector<array<int, 4>>& quads) {
  quads.clear();
  if (shape.elements.empty()) return;
  quads.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::face) continue;
    if (element.material != material) continue;
    if (element.size == 4) {
      quads.push_back({shape.vertices[cur + 0].position - 1,
          shape.vertices[cur + 1].position - 1,
          shape.vertices[cur + 2].position - 1,
          shape.vertices[cur + 3].position - 1});
    } else {
      for (auto c = 2; c < element.size; c++) {
        quads.push_back({shape.vertices[cur + 0].position - 1,
            shape.vertices[cur + c - 1].position - 1,
            shape.vertices[cur + c].position - 1,
            shape.vertices[cur + c].position - 1});
      }
    }
    cur += element.size;
  }
}
void get_lines(
    const obj_shape& shape, int material, vector<array<int, 2>>& lines) {
  lines.clear();
  if (shape.elements.empty()) return;
  lines.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::line) continue;
    if (element.material != material) continue;
    for (auto c = 1; c < element.size; c++) {
      lines.push_back({shape.vertices[cur + c - 1].position - 1,
          shape.vertices[cur + c].position - 1});
    }
    cur += element.size;
  }
}
void get_points(const obj_shape& shape, int material, vector<int>& points) {
  points.clear();
  if (shape.elements.empty()) return;
  points.reserve(shape.elements.size());
  auto cur = 0;
  for (auto& element : shape.elements) {
    if (element.etype != obj_etype::point) continue;
    if (element.material != material) continue;
    for (auto c = 0; c < element.size; c++) {
      points.push_back({shape.vertices[cur + 0].position - 1});
    }
    cur += element.size;
  }
}

bool has_quads(const obj_shape& shape) {
  for (auto& element : shape.elements)
    if (element.etype == obj_etype::face && element.size == 4) return true;
  return false;
}

vector<int> get_materials(const obj_shape& shape) {
  auto materials    = vector<int>{};
  auto material_set = unordered_set<int>{};
  for (auto& element : shape.elements) {
    if (material_set.find(element.material) == material_set.end()) {
      material_set.insert(element.material);
      materials.push_back(element.material);
    }
  }
  return materials;
}

// Add obj shape
void add_positions(obj_shape& shape, const vector<array<float, 3>>& positions) {
  shape.positions.insert(
      shape.positions.end(), positions.begin(), positions.end());
}
void add_normals(obj_shape& shape, const vector<array<float, 3>>& normals) {
  shape.normals.insert(shape.normals.end(), normals.begin(), normals.end());
}
void add_texcoords(
    obj_shape& shape, const vector<array<float, 2>>& texcoords, bool flipv) {
  shape.texcoords.insert(
      shape.texcoords.end(), texcoords.begin(), texcoords.end());
  if (flipv) {
    for (auto idx = shape.texcoords.size() - texcoords.size();
         idx < shape.texcoords.size(); idx++)
      shape.texcoords[idx] = {
          shape.texcoords[idx][0], 1 - shape.texcoords[idx][1]};
  }
}
void add_triangles(obj_shape& shape, const vector<array<int, 3>>& triangles,
    int material, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)triangles.size(); idx++) {
    auto& triangle = triangles[idx];
    for (auto c = 0; c < 3; c++) {
      shape.vertices.push_back({
          triangle[c] + 1,
          !has_texcoord ? 0 : triangle[c] + 1,
          !has_normals ? 0 : triangle[c] + 1,
      });
    }
    shape.elements.push_back({3, obj_etype::face, material});
  }
}
void add_quads(obj_shape& shape, const vector<array<int, 4>>& quads,
    int material, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)quads.size(); idx++) {
    auto& quad = quads[idx];
    auto  nv   = quad[2] == quad[3] ? 3 : 4;
    for (auto c = 0; c < nv; c++) {
      shape.vertices.push_back({
          quad[c] + 1,
          !has_texcoord ? 0 : quad[c] + 1,
          !has_normals ? 0 : quad[c] + 1,
      });
    }
    shape.elements.push_back({(uint16_t)nv, obj_etype::face, material});
  }
}
void add_lines(obj_shape& shape, const vector<array<int, 2>>& lines,
    int material, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)lines.size(); idx++) {
    auto& line = lines[idx];
    for (auto c = 0; c < 2; c++) {
      shape.vertices.push_back({
          line[c] + 1,
          !has_texcoord ? 0 : line[c] + 1,
          !has_normals ? 0 : line[c] + 1,
      });
    }
    shape.elements.push_back({2, obj_etype::line, material});
  }
}
void add_points(obj_shape& shape, const vector<int>& points, int material,
    bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)points.size(); idx++) {
    auto& point = points[idx];
    shape.vertices.push_back({
        point + 1,
        !has_texcoord ? 0 : point + 1,
        !has_normals ? 0 : point + 1,
    });
    shape.elements.push_back({1, obj_etype::point, material});
  }
}
void add_fvquads(obj_shape& shape, const vector<array<int, 4>>& quadspos,
    const vector<array<int, 4>>& quadsnorm,
    const vector<array<int, 4>>& quadstexcoord, int material) {
  for (auto idx = 0; idx < (int)quadspos.size(); idx++) {
    auto nv = quadspos[idx][2] == quadspos[idx][3] ? 3 : 4;
    for (auto c = 0; c < nv; c++) {
      shape.vertices.push_back({
          quadspos.empty() ? 0 : quadspos[idx][c] + 1,
          quadstexcoord.empty() ? 0 : quadstexcoord[idx][c] + 1,
          quadsnorm.empty() ? 0 : quadsnorm[idx][c] + 1,
      });
    }
    shape.elements.push_back({(uint16_t)nv, obj_etype::face, material});
  }
}
void add_quads(obj_shape& shape, const vector<array<int, 4>>& quads,
    const vector<int>& materials, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)quads.size(); idx++) {
    auto& quad = quads[idx];
    auto  nv   = quad[2] == quad[3] ? 3 : 4;
    for (auto c = 0; c < nv; c++) {
      shape.vertices.push_back({
          quad[c] + 1,
          !has_texcoord ? 0 : quad[c] + 1,
          !has_normals ? 0 : quad[c] + 1,
      });
    }
    shape.elements.push_back({(uint16_t)nv, obj_etype::face, materials[idx]});
  }
}
void add_lines(obj_shape& shape, const vector<array<int, 2>>& lines,
    const vector<int>& materials, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)lines.size(); idx++) {
    auto& line = lines[idx];
    for (auto c = 0; c < 2; c++) {
      shape.vertices.push_back({
          line[c] + 1,
          !has_texcoord ? 0 : line[c] + 1,
          !has_normals ? 0 : line[c] + 1,
      });
    }
    shape.elements.push_back({2, obj_etype::line, materials[idx]});
  }
}
void add_points(obj_shape& shape, const vector<int>& points,
    const vector<int>& materials, bool has_normals, bool has_texcoord) {
  for (auto idx = 0; idx < (int)points.size(); idx++) {
    auto& point = points[idx];
    shape.vertices.push_back({
        point + 1,
        !has_texcoord ? 0 : point + 1,
        !has_normals ? 0 : point + 1,
    });
    shape.elements.push_back({1, obj_etype::point, materials[idx]});
  }
}
void add_fvquads(obj_shape& shape, const vector<array<int, 4>>& quadspos,
    const vector<array<int, 4>>& quadsnorm,
    const vector<array<int, 4>>& quadstexcoord, const vector<int>& materials) {
  for (auto idx = 0; idx < (int)quadspos.size(); idx++) {
    auto nv = quadspos[idx][2] == quadspos[idx][3] ? 3 : 4;
    for (auto c = 0; c < nv; c++) {
      shape.vertices.push_back({
          quadspos.empty() ? 0 : quadspos[idx][c] + 1,
          quadstexcoord.empty() ? 0 : quadstexcoord[idx][c] + 1,
          quadsnorm.empty() ? 0 : quadsnorm[idx][c] + 1,
      });
    }
    shape.elements.push_back({(uint16_t)nv, obj_etype::face, materials[idx]});
  }
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// STL PARSING
// -----------------------------------------------------------------------------
namespace yocto {

// Load/save stl
bool load_stl(const string& filename, stl_model& stl, string& error,
    bool unique_vertices) {
  stl.shapes.clear();

  // load data
  auto data = vector<byte>{};
  if (!load_binary(filename, data, error)) return false;

  // parse
  auto data_view  = string_view{(const char*)data.data(), data.size()};
  auto read_error = [&filename, &error]() {
    error = "cannot read " + filename;
    return false;
  };

  // assume it is binary and read hader
  auto header = array<char, 80>{};
  if (!read_value(data_view, header)) return read_error();

  // check if binary
  auto binary = header[0] != 's' || header[1] != 'o' || header[2] != 'l' ||
                header[3] != 'i' || header[4] != 'd';

  // check size in case the binary had a bad header
  if (!binary) {
    auto ntriangles = (uint32_t)0;
    if (!read_value(data_view, ntriangles)) return read_error();
    auto length = data.size();
    auto size   = 80 + 4 + (4 * 12 + 2) * (size_t)ntriangles;
    binary      = length == size;
  }

  // switch on type
  if (binary) {
    // load data
    auto data = vector<byte>{};
    if (!load_binary(filename, data, error)) return false;
    auto data_view = string_view{(const char*)data.data(), data.size()};

    // skip header
    auto header = array<char, 80>{};
    if (!read_value(data_view, header)) return read_error();

    // read shapes until the end
    auto ntriangles = (uint32_t)0;
    while (!data_view.empty()) {
      // append shape
      auto& shape = stl.shapes.emplace_back();

      // read num triangles
      if (!read_value(data_view, ntriangles)) return read_error();

      // resize buffers
      shape.fnormals.resize(ntriangles);
      shape.triangles.resize(ntriangles);
      shape.positions.resize(ntriangles * 3);

      // read all data
      for (auto triangle_id = 0; triangle_id < (int)ntriangles; triangle_id++) {
        // read triangle data
        if (!read_value(data_view, shape.fnormals[triangle_id]))
          return read_error();
        if (!read_value(data_view, shape.positions[triangle_id * 3 + 0]))
          return read_error();
        if (!read_value(data_view, shape.positions[triangle_id * 3 + 1]))
          return read_error();
        if (!read_value(data_view, shape.positions[triangle_id * 3 + 2]))
          return read_error();
        shape.triangles[triangle_id] = {
            triangle_id * 3 + 0, triangle_id * 3 + 1, triangle_id * 3 + 2};
        // read unused attrobute count
        auto attribute_count = (uint16_t)0;
        if (!read_value(data_view, attribute_count)) return read_error();
      }
    }

    // check if read at least one
    if (stl.shapes.empty()) return read_error();
  } else {
    // load data
    auto data = string{};
    if (!load_text(filename, data, error)) return false;

    // parse state
    auto in_solid = false, in_facet = false, in_loop = false;

    // read all lines
    auto data_view   = string_view{data.data(), data.size()};
    auto str         = string_view{};
    auto parse_error = [&filename, &error]() {
      error = "cannot parse " + filename;
      return false;
    };
    while (read_line(data_view, str)) {
      // str
      remove_comment(str);
      skip_whitespace(str);
      if (str.empty()) continue;

      // get command
      auto cmd = ""s;
      if (!parse_value(str, cmd)) return parse_error();
      if (cmd.empty()) continue;

      // switch over command
      if (cmd == "solid") {
        if (in_solid) return parse_error();
        in_solid = true;
        stl.shapes.emplace_back();
      } else if (cmd == "endsolid") {
        if (!in_solid) return parse_error();
        in_solid = false;
      } else if (cmd == "facet") {
        if (!in_solid || in_facet) return parse_error();
        in_facet = true;
        // next command
        if (!parse_value(str, cmd)) return parse_error();
        if (cmd != "normal") return parse_error();
        // vertex normal
        if (!parse_value(str, stl.shapes.back().fnormals.emplace_back()))
          return parse_error();
      } else if (cmd == "endfacet") {
        if (!in_solid || !in_facet || in_loop) return parse_error();
        in_facet = false;
        // check that it was a triangle
        auto last_pos = (int)stl.shapes.back().positions.size() - 3;
        if (stl.shapes.back().triangles.empty() && last_pos != 0)
          return parse_error();
        if (!stl.shapes.back().triangles.empty() &&
            last_pos != stl.shapes.back().triangles.back()[2] + 1)
          return parse_error();
        // add triangle
        stl.shapes.back().triangles.push_back(
            {last_pos + 0, last_pos + 1, last_pos + 2});
      } else if (cmd == "outer") {
        if (!in_solid || !in_facet || in_loop) return parse_error();
        in_loop = true;
        // next command
        if (!parse_value(str, cmd)) return parse_error();
        if (cmd != "loop") return parse_error();
        return parse_error();
      } else if (cmd == "endloop") {
        if (!in_solid || !in_facet || !in_loop) return parse_error();
        in_loop = false;
      } else if (cmd == "vertex") {
        // vertex position
        if (!parse_value(str, stl.shapes.back().positions.emplace_back()))
          return parse_error();
      } else {
        return parse_error();
      }
    }
  }

  // make unique vertices
  if (unique_vertices) {
    for (auto& shape : stl.shapes) {
      auto vertex_map       = unordered_map<array<float, 3>, int>{};
      auto unique_positions = vector<array<float, 3>>{};
      for (auto& triangle : shape.triangles) {
        for (auto& vertex_id : triangle) {
          auto vertex_it = vertex_map.find(shape.positions[vertex_id]);
          if (vertex_it == vertex_map.end()) {
            auto new_vertex_id = (int)unique_positions.size();
            unique_positions.push_back(shape.positions[vertex_id]);
            vertex_map.insert(
                vertex_it, {unique_positions.back(), new_vertex_id});
            vertex_id = new_vertex_id;
          } else {
            vertex_id = vertex_it->second;
          }
        }
      }
      std::swap(unique_positions, shape.positions);
    }
  }

  // done
  return true;
}

bool save_stl(
    const string& filename, const stl_model& stl, string& error, bool ascii) {
  // switch on format
  if (!ascii) {
    // buffer
    auto buffer = vector<byte>{};

    // header
    auto header = array<char, 80>{0};
    snprintf(header.data(), header.size(), "Binary STL - Written by Yocto/GL");
    write_value(buffer, header);

    // write shapes
    for (auto& shape : stl.shapes) {
      auto ntriangles = (uint32_t)shape.triangles.size();
      write_value(buffer, ntriangles);
      for (auto triangle_idx = 0; triangle_idx < (int)shape.triangles.size();
           triangle_idx++) {
        auto& triangle = shape.triangles[triangle_idx];
        auto  fnormal  = !shape.fnormals.empty()
                             ? shape.fnormals[triangle_idx]
                             : triangle_normal(shape.positions[triangle[0]],
                                   shape.positions[triangle[1]],
                                   shape.positions[triangle[2]]);
        write_value(buffer, fnormal);
        write_value(buffer, shape.positions[triangle[0]]);
        write_value(buffer, shape.positions[triangle[1]]);
        write_value(buffer, shape.positions[triangle[2]]);
        auto attribute_count = (uint16_t)0;
        write_value(buffer, attribute_count);
      }
    }

    // save file
    if (!save_binary(filename, buffer, error)) return false;
  } else {
    // buffer
    auto buffer = string{};

    for (auto& shape : stl.shapes) {
      format_values(buffer, "solid \n");
      for (auto triangle_idx = 0; triangle_idx < (int)shape.triangles.size();
           triangle_idx++) {
        auto& triangle = shape.triangles[triangle_idx];
        auto  fnormal  = !shape.fnormals.empty()
                             ? shape.fnormals[triangle_idx]
                             : triangle_normal(shape.positions[triangle[0]],
                                   shape.positions[triangle[1]],
                                   shape.positions[triangle[2]]);
        format_values(buffer, "facet normal {}\n", fnormal);
        format_values(buffer, "outer loop\n");
        format_values(buffer, "vertex {}\n", shape.positions[triangle[0]]);
        format_values(buffer, "vertex {}\n", shape.positions[triangle[1]]);
        format_values(buffer, "vertex {}\n", shape.positions[triangle[2]]);
        format_values(buffer, "endloop\n");
        format_values(buffer, "endfacet\n");
      }
      format_values(buffer, "endsolid \n");
    }

    // save file
    if (!save_text(filename, buffer, error)) return false;
  }

  // done
  return true;
}

// Get/set data
bool get_triangles(const stl_model& stl, int shape_id,
    vector<array<int, 3>>& triangles, vector<array<float, 3>>& positions,
    vector<array<float, 3>>& fnormals) {
  if (shape_id < 0 || shape_id >= (int)stl.shapes.size()) return false;
  auto& shape = stl.shapes.at(shape_id);
  triangles   = shape.triangles;
  positions   = shape.positions;
  fnormals    = shape.fnormals;
  return true;
}
void add_triangles(stl_model& stl, const vector<array<int, 3>>& triangles,
    const vector<array<float, 3>>& positions,
    const vector<array<float, 3>>& fnormals) {
  auto& shape     = stl.shapes.emplace_back();
  shape.triangles = triangles;
  shape.positions = positions;
  shape.fnormals  = fnormals;
}

}  // namespace yocto
