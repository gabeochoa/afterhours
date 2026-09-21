#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "../../core/base_component.h"
#include "../animation.h"
#include "text_units.h"

namespace afterhours {
namespace ui {

enum struct StaggerOrder { Forward, Reverse, CenterOut, Random };

struct TextUnitMotion {
  TextUnit unit = TextUnit::Char;
  float from_x = 0.f;
  float from_y = 8.f;
  float from_opacity = 0.f;
  float from_scale = 1.f;
  float duration = 0.35f;
  float stagger = 0.04f;
  StaggerOrder order = StaggerOrder::Forward;
};

inline float stagger_delay(size_t index, size_t count, float step, StaggerOrder order) {
  if (count == 0)
    return 0.f;
  size_t rank = index;
  switch (order) {
  case StaggerOrder::Forward:
    break;
  case StaggerOrder::Reverse:
    rank = count - 1 - index;
    break;
  case StaggerOrder::CenterOut: {
    const float centre = (static_cast<float>(count) - 1.f) / 2.f;
    rank = static_cast<size_t>(std::lround(std::fabs(static_cast<float>(index) - centre)));
    break;
  }
  case StaggerOrder::Random:
    rank = (index * 2654435761u + count * 40503u) % count;
    break;
  }
  return step * static_cast<float>(rank);
}

enum struct UnitProp : size_t { X, Y, Opacity, Scale };
constexpr size_t kUnitKeyBase = 1000;
inline size_t unit_key(size_t index, UnitProp prop) {
  return kUnitKeyBase + index * 4 + static_cast<size_t>(prop);
}

struct HasTextUnitMotion : BaseComponent {
  TextUnitMotion cfg;
  TextUnitCache cache;
  std::vector<UnitSpan> spans;
  std::vector<std::string> units;
  std::vector<std::string> last;
  bool primed = false;
};

struct UnitDraw {
  float x = 0.f;
  float y = 0.f;
  float opacity = 1.f;
  float scale = 1.f;
};

inline void update_text_units(Entity &entity, const std::string &label) {
  auto &st = entity.get<HasTextUnitMotion>();
  auto &tracks = entity.addComponentIfMissing<motion::HasTracks>();
  st.spans = st.cache.get(label, st.cfg.unit);
  st.units.clear();
  for (const UnitSpan &sp : st.spans)
    st.units.push_back(label.substr(sp.begin, sp.size()));

  const motion::Timeline ease{.keys = {{0.f, 0.f}, {st.cfg.duration, 1.f}},
                              .curve = motion::curves::ease_out_quad};
  size_t changed = 0;
  std::vector<size_t> changed_indices;
  for (size_t i = 0; i < st.units.size(); ++i)
    if (!st.primed || i >= st.last.size() || st.last[i] != st.units[i])
      changed_indices.push_back(i);
  for (size_t i : changed_indices) {
    const float delay = stagger_delay(changed, changed_indices.size(), st.cfg.stagger, st.cfg.order);
    ++changed;
    tracks.track<float>(unit_key(i, UnitProp::X)).from(st.cfg.from_x).to(0.f, ease).delay(delay);
    tracks.track<float>(unit_key(i, UnitProp::Y)).from(st.cfg.from_y).to(0.f, ease).delay(delay);
    tracks.track<float>(unit_key(i, UnitProp::Opacity)).from(st.cfg.from_opacity).to(1.f, ease).delay(delay);
    tracks.track<float>(unit_key(i, UnitProp::Scale)).from(st.cfg.from_scale).to(1.f, ease).delay(delay);
  }
  st.primed = true;
  st.last = st.units;
}

inline void restart_text_units(Entity &entity) {
  if (entity.has<HasTextUnitMotion>())
    entity.get<HasTextUnitMotion>().primed = false;
}

inline UnitDraw unit_draw(const Entity &entity, size_t index) {
  UnitDraw d;
  if (!entity.has<motion::HasTracks>())
    return d;
  const auto &floats = entity.get<motion::HasTracks>().floats;
  const auto read = [&](UnitProp p, float fallback) {
    auto it = floats.find(unit_key(index, p));
    return it == floats.end() ? fallback : it->second.value_or(fallback);
  };
  d.x = read(UnitProp::X, 0.f);
  d.y = read(UnitProp::Y, 0.f);
  d.opacity = read(UnitProp::Opacity, 1.f);
  d.scale = read(UnitProp::Scale, 1.f);
  return d;
}

inline bool text_units_active(const Entity &entity) {
  if (!entity.has<HasTextUnitMotion>() || !entity.has<motion::HasTracks>())
    return false;
  const auto &floats = entity.get<motion::HasTracks>().floats;
  for (const auto &[key, track] : floats)
    if (key >= kUnitKeyBase && track.active())
      return true;
  return false;
}

} // namespace ui
} // namespace afterhours
