#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/hash.h"

#if __has_include(<expected>)
#include <expected>
#if defined(__cpp_lib_expected)
namespace model_lib_std = std;
#else
#include "../../expected.hpp"
namespace model_lib_std = tl;
#endif
#else
#include "../../expected.hpp"
namespace model_lib_std = tl;
#endif

namespace afterhours {

template <typename Texture> struct TextureRegistry {
  enum struct Error { DUPLICATE_NAME };
  std::size_t upload_count = 0;

  template <typename Upload>
  const Texture &get_or_upload(std::span<const std::uint8_t> bytes, Upload &&upload) {
    std::uint64_t key = core_hash(bytes);
    auto it = by_hash.find(key);
    if (it != by_hash.end()) return *it->second;
    upload_count++;
    auto inserted = by_hash.emplace(key, std::make_unique<Texture>(upload(bytes)));
    return *inserted.first->second;
  }

  template <typename Upload>
  model_lib_std::expected<const Texture *, Error>
  load(const char *name, std::span<const std::uint8_t> bytes, Upload &&upload) {
    if (by_name.contains(name)) return model_lib_std::unexpected(Error::DUPLICATE_NAME);
    const Texture &tex = get_or_upload(bytes, std::forward<Upload>(upload));
    by_name.emplace(name, &tex);
    return &tex;
  }

  [[nodiscard]] const Texture *find(const std::string &name) const {
    auto it = by_name.find(name);
    return it == by_name.end() ? nullptr : it->second;
  }

  [[nodiscard]] std::size_t size() const { return by_hash.size(); }
  template <typename Unload> void clear(Unload &&unload) {
    for (auto &[_, tex] : by_hash) unload(*tex);
    by_hash.clear(); by_name.clear(); upload_count = 0;
  }

private:
  std::unordered_map<std::uint64_t, std::unique_ptr<Texture>> by_hash;
  std::map<std::string, const Texture *> by_name;
};

struct GLTFBounds {
  std::array<float, 3> min{}, max{};
  [[nodiscard]] std::array<float, 3> center() const {
    return {(min[0] + max[0]) * .5f, (min[1] + max[1]) * .5f, (min[2] + max[2]) * .5f};
  }

  [[nodiscard]] std::array<float, 3> size() const {
    return {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
  }
};

enum struct GLTFError {
  FILE_OPEN_FAILED, BAD_MAGIC, UNSUPPORTED_VERSION, TRUNCATED,
  BAD_JSON, NO_POSITION_BOUNDS,
};

namespace detail_gltf {
struct Json {
  enum Type { Null, Num, Str, Arr, Obj } type = Null;
  double num = 0; std::string str;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;
  const Json *find(const char *key) const {
    if (type != Obj) return nullptr;
    for (auto &[k, v] : obj) if (k == key) return &v;
    return nullptr;
  }
};
struct Parser {
  const char *p, *end;
  bool parse(Json &out) { ws(); return value(out) && (ws(), p == end); }
  void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }
  bool value(Json &out) {
    if (p >= end) return false;
    if (*p == '{') return object(out);
    if (*p == '[') return array(out);
    if (*p == '"') { out.type = Json::Str; return string(out.str); }
    if (*p == 't') return lit("true", out);
    if (*p == 'f') return lit("false", out);
    if (*p == 'n') { out.type = Json::Null; return lit("null", out); }
    std::string tmp(p, end); char *e = nullptr; out.num = std::strtod(tmp.c_str(), &e);
    if (e == tmp.c_str()) return false;
    out.type = Json::Num; p += static_cast<std::ptrdiff_t>(e - tmp.c_str()); return true;
  }

  bool lit(const char *s, Json &out) {
    std::size_t n = std::strlen(s); if (static_cast<std::size_t>(end - p) < n || std::memcmp(p, s, n)) return false;
    p += static_cast<std::ptrdiff_t>(n); if (out.type != Json::Null) out.type = Json::Num, out.num = (s[0] == 't');
    return true;
  }

  bool string(std::string &out) {
    if (*p != '"') return false;
    for (p++; p < end && *p != '"'; p++) {
      if (*p == '\\') {
        if (++p >= end) return false;
        if (*p == 'u' && p + 4 < end) { out += '?'; p += 4; } else out += *p;
        continue;
      }
      out += *p;
    }
    return p < end && (*p++ == '"');
  }

  bool object(Json &out) {
    out.type = Json::Obj; p++;
    ws(); if (p < end && *p == '}') { p++; return true; }
    while (p < end) {
      ws(); std::string key; if (!string(key)) return false;
      ws(); if (p >= end || *p++ != ':') return false;
      ws(); Json v; if (!value(v)) return false;
      out.obj.emplace_back(std::move(key), std::move(v));
      ws(); if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == '}') { p++; return true; }
      return false;
    }
    return false;
  }

  bool array(Json &out) {
    out.type = Json::Arr; p++;
    ws(); if (p < end && *p == ']') { p++; return true; }
    while (p < end) {
      ws(); Json v; if (!value(v)) return false;
      out.arr.push_back(std::move(v));
      ws(); if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == ']') { p++; return true; }
      return false;
    }
    return false;
  }
};
inline bool vec3(const Json *j, std::array<float, 3> &out) {
  if (j == nullptr || j->type != Json::Arr || j->arr.size() != 3) return false;
  for (int i = 0; i < 3; i++) out[static_cast<std::size_t>(i)] = static_cast<float>(j->arr[static_cast<std::size_t>(i)].num);
  return true;
}
}

inline model_lib_std::expected<GLTFBounds, GLTFError>
gltf_bounds_from_json(std::span<const char> text) {
  using namespace detail_gltf;
  Json root; Parser parser{text.data(), text.data() + text.size()};
  if (!parser.parse(root)) return model_lib_std::unexpected(GLTFError::BAD_JSON);
  const Json *accessors = root.find("accessors"), *meshes = root.find("meshes");
  if (accessors == nullptr || meshes == nullptr || accessors->type != Json::Arr ||
      meshes->type != Json::Arr)
    return model_lib_std::unexpected(GLTFError::NO_POSITION_BOUNDS);
  GLTFBounds bounds; bool have = false;
  for (const Json &mesh : meshes->arr) {
    const Json *prims = mesh.find("primitives");
    if (prims == nullptr) continue;
    for (const Json &prim : prims->arr) {
      const Json *attrs = prim.find("attributes");
      const Json *pos = attrs ? attrs->find("POSITION") : nullptr;
      if (pos == nullptr || pos->type != Json::Num) continue;
      auto idx = static_cast<std::size_t>(pos->num);
      if (idx >= accessors->arr.size()) return model_lib_std::unexpected(GLTFError::BAD_JSON);
      std::array<float, 3> mn{}, mx{};
      if (!vec3(accessors->arr[idx].find("min"), mn) || !vec3(accessors->arr[idx].find("max"), mx))
        return model_lib_std::unexpected(GLTFError::NO_POSITION_BOUNDS);
      if (!have) { bounds = {mn, mx}; have = true; continue; }
      for (int i = 0; i < 3; i++) {
        auto k = static_cast<std::size_t>(i);
        bounds.min[k] = mn[k] < bounds.min[k] ? mn[k] : bounds.min[k];
        bounds.max[k] = mx[k] > bounds.max[k] ? mx[k] : bounds.max[k];
      }
    }
  }
  if (!have) return model_lib_std::unexpected(GLTFError::NO_POSITION_BOUNDS);
  return bounds;
}

inline model_lib_std::expected<GLTFBounds, GLTFError>
gltf_bounds_from_bytes(std::span<const std::uint8_t> bytes) {
  auto as_text = [&] {
    return gltf_bounds_from_json({reinterpret_cast<const char *>(bytes.data()), bytes.size()});
  };
  if (bytes.size() < 4 || std::memcmp(bytes.data(), "glTF", 4) != 0) {
    std::size_t first = 0; while (first < bytes.size() && (bytes[first] == ' ' || bytes[first] == '\n')) first++;
    if (first < bytes.size() && bytes[first] != '{') return model_lib_std::unexpected(GLTFError::BAD_MAGIC);
    return as_text();
  }
  if (bytes.size() < 20) return model_lib_std::unexpected(GLTFError::TRUNCATED);
  auto u32 = [&](std::size_t off) {
    std::uint32_t v; std::memcpy(&v, bytes.data() + off, 4); return v;
  };
  if (u32(4) != 2) return model_lib_std::unexpected(GLTFError::UNSUPPORTED_VERSION);
  std::uint32_t json_len = u32(12);
  if (u32(16) != 0x4E4F534A || 20ull + json_len > bytes.size())
    return model_lib_std::unexpected(GLTFError::TRUNCATED);
  return gltf_bounds_from_json({reinterpret_cast<const char *>(bytes.data() + 20), json_len});
}

inline model_lib_std::expected<GLTFBounds, GLTFError>
gltf_bounds_from_file(const char *path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return model_lib_std::unexpected(GLTFError::FILE_OPEN_FAILED);
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return gltf_bounds_from_bytes(bytes);
}

}
