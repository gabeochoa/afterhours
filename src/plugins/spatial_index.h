#pragma once

// Spatial index plugin: a uniform grid over entity positions with a
// static/dynamic split. Static entries are built once, dynamic entries are
// rebuilt per update, and queries scan both layers.
//
// ECS surface: give entities HasSpatialPosition (sync `pos` from your own
// transform however you like, set `is_static` for geometry that rarely
// moves), register the plugin, and query entity ids by radius/box through
// ProvidesSpatialIndex or the spatial_index::query_* helpers. The nested
// Index<Id> is the plain engine underneath, usable without ECS.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <unordered_map>
#include <vector>

#include "../core/base_component.h"
#include "../core/entity_query.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"

namespace afterhours {

struct spatial_index : developer::Plugin {
  // Uniform-grid index over caller-supplied {id, pos} entries.
  template <typename Id> struct Index {
    struct Entry {
      Id id;
      Vector2Type pos;
    };

    float cell_size;

    explicit Index(float cell = 64.f) : cell_size(cell > 0.f ? cell : 64.f) {}

    void set_cell_size(float s) {
      if (s > 0.f) cell_size = s; // non-positive would divide by zero
    }

    void rebuild_static(std::span<const Entry> entries) { build(static_layer, entries); }
    void rebuild_dynamic(std::span<const Entry> entries) { build(dynamic_layer, entries); }
    // Convenience: rebuild(points) rebuilds the dynamic layer.
    void rebuild(std::span<const Entry> entries) { rebuild_dynamic(entries); }
    void clear_static() { static_layer.clear(); }
    void clear_dynamic() { dynamic_layer.clear(); }
    void clear() { clear_static(); clear_dynamic(); }
    [[nodiscard]] std::size_t static_size() const { return count(static_layer); }
    [[nodiscard]] std::size_t dynamic_size() const { return count(dynamic_layer); }
    [[nodiscard]] std::size_t size() const { return static_size() + dynamic_size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    [[nodiscard]] std::vector<Id> query_radius(Vector2Type center, float r) const {
      std::vector<Id> out;
      if (r < 0.f) return out;
      const float r2 = r * r;
      query_box_cells(center.x - r, center.y - r, center.x + r, center.y + r,
                      [&](const Entry &e) {
                        if (distance_sq(e.pos, center) <= r2) out.push_back(e.id);
                      });
      return out;
    }

    [[nodiscard]] std::vector<Id> query_box(Vector2Type min, Vector2Type max) const {
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
    Layer static_layer, dynamic_layer;

    [[nodiscard]] std::int32_t cell_coord(float v) const {
      return static_cast<std::int32_t>(std::floor(v / cell_size));
    }

    static std::int64_t key(std::int32_t cx, std::int32_t cy) {
      return (static_cast<std::int64_t>(cx) << 32) | static_cast<std::uint32_t>(cy);
    }

    void build(Layer &layer, std::span<const Entry> entries) const {
      layer.clear();
      for (const auto &e : entries)
        layer[key(cell_coord(e.pos.x), cell_coord(e.pos.y))].push_back(e);
    }

    static std::size_t count(const Layer &layer) {
      return std::accumulate(layer.begin(), layer.end(), std::size_t{0},
                             [](std::size_t n, const auto &cell) { return n + cell.second.size(); });
    }

    template <typename Fn>
    void query_box_cells(float x0, float y0, float x1, float y1, Fn &&fn) const {
      const auto cx0 = cell_coord(x0), cx1 = cell_coord(x1);
      const auto cy0 = cell_coord(y0), cy1 = cell_coord(y1);
      for (const Layer *layer : {&static_layer, &dynamic_layer})
        for (auto cx = cx0; cx <= cx1; ++cx)
          for (auto cy = cy0; cy <= cy1; ++cy) {
            auto it = layer->find(key(cx, cy));
            if (it != layer->end())
              for (const auto &e : it->second) fn(e);
          }
    }
  };

  // Opt-in position for indexed entities. `is_static` entries go in the
  // static layer; everything else is rebuilt every update.
  struct HasSpatialPosition : BaseComponent {
    Vector2Type pos{0.f, 0.f};
    bool is_static = false;

    HasSpatialPosition() = default;
    explicit HasSpatialPosition(Vector2Type p, bool statik = false)
        : pos(p), is_static(statik) {}
  };

  // Singleton holding the built index. Query it directly, or use the
  // spatial_index::query_* helpers below.
  struct ProvidesSpatialIndex : BaseComponent {
    Index<EntityID> index;

    [[nodiscard]] std::vector<EntityID> query_radius(Vector2Type center, float r) const {
      return index.query_radius(center, r);
    }
    [[nodiscard]] std::vector<EntityID> query_box(Vector2Type min, Vector2Type max) const {
      return index.query_box(min, max);
    }
  };

  // Rebuilds both layers from every HasSpatialPosition entity, once per
  // update pass. Static-layer entities are re-fed each pass too (cheap:
  // the split is by component flag, not by change tracking), so a static
  // entity that does move is never stale.
  struct RebuildSpatialIndex : System<HasSpatialPosition> {
    void once(float) override {
      auto *provider = EntityHelper::get_singleton_cmp<ProvidesSpatialIndex>();
      if (!provider) return;
      RefEntities entities;
      EntityQuery().whereHasComponent<HasSpatialPosition>().gen_into(entities);
      std::vector<Index<EntityID>::Entry> statik, dynamic;
      for (const Entity &e : entities) {
        const auto &hp = e.get<HasSpatialPosition>();
        (hp.is_static ? statik : dynamic).push_back({e.id, hp.pos});
      }
      provider->index.rebuild_static(statik);
      provider->index.rebuild_dynamic(dynamic);
    }
  };

  [[nodiscard]] static std::vector<EntityID> query_radius(Vector2Type center, float r) {
    const auto *provider = EntityHelper::get_singleton_cmp_const<ProvidesSpatialIndex>();
    return provider ? provider->query_radius(center, r) : std::vector<EntityID>{};
  }

  [[nodiscard]] static std::vector<EntityID> query_box(Vector2Type min, Vector2Type max) {
    const auto *provider = EntityHelper::get_singleton_cmp_const<ProvidesSpatialIndex>();
    return provider ? provider->query_box(min, max) : std::vector<EntityID>{};
  }

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<ProvidesSpatialIndex>();
    EntityHelper::registerSingleton<ProvidesSpatialIndex>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesSpatialIndex>>());
  }

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<RebuildSpatialIndex>());
  }
};

// Compile-time verification that spatial_index satisfies the PluginCore concept
static_assert(developer::PluginCore<spatial_index>,
              "spatial_index must implement the core plugin interface");

} // namespace afterhours
