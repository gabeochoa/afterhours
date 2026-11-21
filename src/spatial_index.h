#pragma once

// Uniform-grid spatial index with a static/dynamic split: static entries
// are built once, dynamic entries are rebuilt per update, and queries scan
// both layers. No ECS binding yet — callers pass plain points and ids.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace afterhours {

template <typename Id> struct SpatialIndex {
  struct Point {
    float x = 0.f;
    float y = 0.f;
  };
  struct Entry {
    Id id;
    Point pos;
  };

  explicit SpatialIndex(float cell_size = 64.f) : cell_size_(cell_size > 0.f ? cell_size : 64.f) {}

  void set_cell_size(float s) {
    cell_size_ = s > 0.f ? s : cell_size_; // non-positive size would divide by zero
  }
  [[nodiscard]] float cell_size() const { return cell_size_; }

  void rebuild_static(std::span<const Entry> entries) { build(static_, entries); }
  void rebuild_dynamic(std::span<const Entry> entries) { build(dynamic_, entries); }
  // Convenience: rebuild(points) rebuilds the dynamic layer.
  void rebuild(std::span<const Entry> entries) { rebuild_dynamic(entries); }
  void clear_static() { static_.clear(); }
  void clear_dynamic() { dynamic_.clear(); }
  void clear() { clear_static(); clear_dynamic(); }
  [[nodiscard]] std::size_t static_size() const { return count(static_); }
  [[nodiscard]] std::size_t dynamic_size() const { return count(dynamic_); }
  [[nodiscard]] std::size_t size() const { return static_size() + dynamic_size(); }
  [[nodiscard]] bool empty() const { return size() == 0; }

  [[nodiscard]] std::vector<Id> query_radius(Point center, float r) const {
    std::vector<Id> out;
    if (r < 0.f) return out;
    const float r2 = r * r;
    query_box_cells(center.x - r, center.y - r, center.x + r, center.y + r,
                    [&](const Entry &e) {
                      const float dx = e.pos.x - center.x, dy = e.pos.y - center.y;
                      if (dx * dx + dy * dy <= r2) out.push_back(e.id);
                    });
    return out;
  }

  [[nodiscard]] std::vector<Id> query_box(Point min, Point max) const {
    std::vector<Id> out;
    if (min.x > max.x || min.y > max.y) return out;
    query_box_cells(min.x, min.y, max.x, max.y, [&](const Entry &e) {
      if (e.pos.x >= min.x && e.pos.x <= max.x && e.pos.y >= min.y &&
          e.pos.y <= max.y)
        out.push_back(e.id);
    });
    return out;
  }

private:
  using Layer = std::unordered_map<std::int64_t, std::vector<Entry>>;
  Layer static_, dynamic_;
  float cell_size_;

  [[nodiscard]] std::int32_t cell_coord(float v) const {
    return static_cast<std::int32_t>(std::floor(v / cell_size_));
  }

  static std::int64_t key(std::int32_t cx, std::int32_t cy) {
    return (static_cast<std::int64_t>(cx) << 32) |
           static_cast<std::uint32_t>(cy);
  }

  void build(Layer &layer, std::span<const Entry> entries) const {
    layer.clear();
    for (const auto &e : entries)
      layer[key(cell_coord(e.pos.x), cell_coord(e.pos.y))].push_back(e);
  }

  static std::size_t count(const Layer &layer) {
    std::size_t n = 0;
    for (const auto &[k, v] : layer) n += v.size();
    return n;
  }

  template <typename Fn>
  void query_box_cells(float x0, float y0, float x1, float y1, Fn &&fn) const {
    const auto cx0 = cell_coord(x0), cx1 = cell_coord(x1);
    const auto cy0 = cell_coord(y0), cy1 = cell_coord(y1);
    for (const Layer *layer : {&static_, &dynamic_})
      for (auto cx = cx0; cx <= cx1; ++cx)
        for (auto cy = cy0; cy <= cy1; ++cy) {
          auto it = layer->find(key(cx, cy));
          if (it != layer->end())
            for (const auto &e : it->second) fn(e);
        }
  }
};

} // namespace afterhours
