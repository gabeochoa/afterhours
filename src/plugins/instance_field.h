#pragma once


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "../developer.h"
#include "../random_engine.h"

namespace afterhours {

struct InstanceTransform {
  Vector3Type pos{};
  float yaw_deg = 0.f;
  Vector3Type scale{1.f, 1.f, 1.f};
  bool operator==(const InstanceTransform &) const = default;
};

inline std::vector<InstanceTransform> scatter_field(Vector3Type min,
                                                    Vector3Type max, int count,
                                                    RngStream &rng,
                                                    float scale_min = 1.f,
                                                    float scale_max = 1.f) {
  std::vector<InstanceTransform> out;
  if (count <= 0) return out;
  out.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    float s = rng.get_float(scale_min, scale_max);
    out.push_back({{rng.get_float(min.x, max.x), min.y,
                    rng.get_float(min.z, max.z)},
                   rng.get_float(0.f, 360.f),
                   {s, s, s}});
  }
  return out;
}

inline std::vector<InstanceTransform>
scatter_field(Vector3Type min, Vector3Type max, int count, uint32_t seed,
              float scale_min = 1.f, float scale_max = 1.f) {
  RngStream rng(seed);
  return scatter_field(min, max, count, rng, scale_min, scale_max);
}

struct FieldChunkKey {
  int x = 0, z = 0;
  bool operator==(const FieldChunkKey &) const = default;
};

struct FieldChunk {
  FieldChunkKey key;
  std::vector<InstanceTransform> instances;
  [[nodiscard]] Vector3Type center(float chunk_size) const {
    return {(key.x + 0.5f) * chunk_size, 0.f, (key.z + 0.5f) * chunk_size};
  }
};

enum class FieldLod { Full, Reduced, Skip };

struct FieldLodBands {
  float full = 40.f;
  float reduced = 90.f;
  float density = 0.25f;
};

[[nodiscard]] inline FieldChunkKey
field_chunk_key_for(Vector3Type pos, float chunk_size) {
  return {static_cast<int>(std::floor(pos.x / chunk_size)),
          static_cast<int>(std::floor(pos.z / chunk_size))};
}

[[nodiscard]] inline FieldLod field_lod_for(float distance_xz,
                                            const FieldLodBands &bands) {
  if (distance_xz <= bands.full) return FieldLod::Full;
  if (distance_xz <= bands.reduced) return FieldLod::Reduced;
  return FieldLod::Skip;
}

namespace detail_field {
[[nodiscard]] inline float keep_fraction(FieldChunkKey key, size_t index) {
  uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(key.x)) *
                   0x9e3779b97f4a7c15ull +
               static_cast<uint64_t>(static_cast<uint32_t>(key.z)) *
                   0xc2b2ae3d27d4eb4full +
               static_cast<uint64_t>(index + 1) * 0x165667b19e3779f9ull;
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdull;
  h ^= h >> 33;
  return static_cast<float>(h >> 40) / 16777216.f;
}
}

struct InstanceField {
  float chunk_size = 16.f;
  std::vector<FieldChunk> chunks;

  void build(std::span<const InstanceTransform> instances, float chunk) {
    chunk_size = chunk > 0.f ? chunk : 16.f;
    chunks.clear();
    for (const InstanceTransform &t : instances) {
      FieldChunkKey k = field_chunk_key_for(t.pos, chunk_size);
      auto it = std::find_if(chunks.begin(), chunks.end(), [&](const FieldChunk &c) {
        return c.key == k;
      });
      if (it == chunks.end()) {
        chunks.push_back({k, {t}});
      } else {
        it->instances.push_back(t);
      }
    }
    std::sort(chunks.begin(), chunks.end(),
              [](const FieldChunk &a, const FieldChunk &b) {
                return a.key.z != b.key.z ? a.key.z < b.key.z
                                          : a.key.x < b.key.x;
              });
  }

  [[nodiscard]] const FieldChunk *chunk_at(Vector3Type pos) const {
    FieldChunkKey k = field_chunk_key_for(pos, chunk_size);
    for (const FieldChunk &c : chunks)
      if (c.key == k) return &c;
    return nullptr;
  }

  [[nodiscard]] size_t count() const {
    size_t n = 0;
    for (const FieldChunk &c : chunks) n += c.instances.size();
    return n;
  }

  [[nodiscard]] float distance_xz(Vector3Type camera,
                                  const FieldChunk &chunk) const {
    Vector3Type c = chunk.center(chunk_size);
    float dx = camera.x - c.x, dz = camera.z - c.z;
    return std::sqrt(dx * dx + dz * dz);
  }

  [[nodiscard]] FieldLod lod(Vector3Type camera, const FieldChunk &chunk,
                             const FieldLodBands &bands) const {
    return field_lod_for(distance_xz(camera, chunk), bands);
  }

  void append_selected(Vector3Type camera, const FieldLodBands &bands,
                       std::vector<InstanceTransform> &out) const {
    float density = std::clamp(bands.density, 0.f, 1.f);
    for (const FieldChunk &c : chunks) {
      FieldLod l = lod(camera, c, bands);
      if (l == FieldLod::Skip) continue;
      if (l == FieldLod::Full) {
        out.insert(out.end(), c.instances.begin(), c.instances.end());
      } else {
        for (size_t i = 0; i < c.instances.size(); i++)
          if (detail_field::keep_fraction(c.key, i) < density)
            out.push_back(c.instances[i]);
      }
    }
  }

  [[nodiscard]] std::vector<InstanceTransform>
  selected(Vector3Type camera, const FieldLodBands &bands) const {
    std::vector<InstanceTransform> out;
    append_selected(camera, bands, out);
    return out;
  }
};

}
