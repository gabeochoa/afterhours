#pragma once


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace afterhours {

struct HeightfieldVec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};

struct HeightfieldMesh {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<std::uint32_t> indices;
  int vertex_cols = 0, vertex_rows = 0;
};

struct Heightfield {
  int cols = 0, rows = 0;
  float cell = 1.f;
  float origin_x = 0.f, origin_z = 0.f;
  std::vector<float> heights;

  Heightfield() = default;
  Heightfield(int cols_, int rows_, float cell_, std::vector<float> heights_,
              float origin_x_ = 0.f, float origin_z_ = 0.f)
      : cols(cols_), rows(rows_), cell(cell_ > 0.f ? cell_ : 1.f),
        origin_x(origin_x_), origin_z(origin_z_),
        heights(std::move(heights_)) {}

  [[nodiscard]] bool valid() const {
    return cols >= 2 && rows >= 2 &&
           heights.size() >= static_cast<size_t>(cols) * rows;
  }

  [[nodiscard]] float height_at_grid(int col, int row) const {
    if (!valid()) return 0.f;
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    return heights[static_cast<size_t>(row) * cols + col];
  }

  [[nodiscard]] float sample_height(float x, float z) const {
    if (!valid()) return 0.f;
    float gx = std::clamp((x - origin_x) / cell, 0.f, float(cols - 1));
    float gz = std::clamp((z - origin_z) / cell, 0.f, float(rows - 1));
    int c0 = int(gx), r0 = int(gz);
    int c1 = std::min(c0 + 1, cols - 1), r1 = std::min(r0 + 1, rows - 1);
    float fx = gx - float(c0), fz = gz - float(r0);
    float top = height_at_grid(c0, r0) * (1.f - fx) + height_at_grid(c1, r0) * fx;
    float bot = height_at_grid(c0, r1) * (1.f - fx) + height_at_grid(c1, r1) * fx;
    return top * (1.f - fz) + bot * fz;
  }

  [[nodiscard]] HeightfieldVec3 normal_at(float x, float z) const {
    if (!valid()) return {0.f, 1.f, 0.f};
    float dx = (sample_height(x + cell, z) - sample_height(x - cell, z)) / (2.f * cell);
    float dz = (sample_height(x, z + cell) - sample_height(x, z - cell)) / (2.f * cell);
    HeightfieldVec3 n{-dx, 1.f, -dz};
    float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 0.f) { n.x /= len; n.y /= len; n.z /= len; }
    return n;
  }

  [[nodiscard]] HeightfieldMesh build_tile_mesh(int tile_col, int tile_row,
                                                int tile_cols, int tile_rows) const {
    HeightfieldMesh m;
    if (!valid() || tile_cols <= 0 || tile_rows <= 0) return m;
    int c0 = std::clamp(tile_col, 0, cols - 1), r0 = std::clamp(tile_row, 0, rows - 1);
    int c1 = std::min(c0 + tile_cols, cols - 1), r1 = std::min(r0 + tile_rows, rows - 1);
    if (c1 <= c0 || r1 <= r0) return m;
    m.vertex_cols = c1 - c0 + 1;
    m.vertex_rows = r1 - r0 + 1;
    for (int r = r0; r <= r1; r++) {
      for (int c = c0; c <= c1; c++) {
        float x = origin_x + float(c) * cell, z = origin_z + float(r) * cell;
        m.positions.insert(m.positions.end(), {x, height_at_grid(c, r), z});
        HeightfieldVec3 n = normal_at(x, z);
        m.normals.insert(m.normals.end(), {n.x, n.y, n.z});
        m.uvs.insert(m.uvs.end(), {cols > 1 ? float(c) / float(cols - 1) : 0.f,
                                   rows > 1 ? float(r) / float(rows - 1) : 0.f});
      }
    }
    for (int r = 0; r + 1 < m.vertex_rows; r++) {
      for (int c = 0; c + 1 < m.vertex_cols; c++) {
        auto a = std::uint32_t(r * m.vertex_cols + c), b = a + 1;
        auto d = a + std::uint32_t(m.vertex_cols), e = d + 1;
        m.indices.insert(m.indices.end(), {a, d, b, b, d, e});
      }
    }
    return m;
  }
};

}
