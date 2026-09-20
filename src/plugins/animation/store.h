#pragma once

#include <memory>
#include <unordered_map>

#include "../../core/system.h"
#include "../profiling.h"
#include "track.h"

namespace afterhours {
namespace motion {

inline bool &paused_flag() {
  static bool v = false;
  return v;
}
inline float &time_scale_ref() {
  static float v = 1.f;
  return v;
}
inline void pause(bool on) { paused_flag() = on; }
inline bool is_paused() { return paused_flag(); }
inline void set_time_scale(float scale) {
  time_scale_ref() = std::max(scale, 0.f);
}
inline float time_scale() { return time_scale_ref(); }
inline float scaled_dt(float dt) {
  return is_paused() ? 0.f : dt * time_scale();
}

struct HasTracks : BaseComponent {
  std::unordered_map<size_t, Track<float>> floats;
  std::unordered_map<size_t, Track<Vector2Type>> vec2s;
  std::unordered_map<size_t, Track<RectangleType>> rects;
  std::unordered_map<size_t, Track<ColorType>> colors;

  template <typename T> Track<T> &track(size_t key) {
    if constexpr (std::is_same_v<T, float>)
      return floats[key];
    else if constexpr (std::is_same_v<T, Vector2Type>)
      return vec2s[key];
    else if constexpr (std::is_same_v<T, RectangleType>)
      return rects[key];
    else
      return colors[key];
  }

  void advance(float dt) {
    for (auto &[k, t] : floats)
      t.advance(dt);
    for (auto &[k, t] : vec2s)
      t.advance(dt);
    for (auto &[k, t] : rects)
      t.advance(dt);
    for (auto &[k, t] : colors)
      t.advance(dt);
  }

  size_t size() const {
    return floats.size() + vec2s.size() + rects.size() + colors.size();
  }
};

struct MotionRoot : BaseComponent {};

inline Entity &root_entity() {
  if (!EntityHelper::has_singleton<MotionRoot>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    e.addComponent<MotionRoot>();
    EntityHelper::registerSingleton<MotionRoot>(e);
  }
  return EntityHelper::get_singleton<MotionRoot>().get();
}

template <typename T = float, typename E> Track<T> &anim(E key) {
  return root_entity().addComponentIfMissing<HasTracks>().track<T>(
      static_cast<size_t>(key));
}

template <typename T = float, typename E> Track<T> &anim(E key, EntityID id) {
  return EntityHelper::getEntityForIDEnforce(id)
      .addComponentIfMissing<HasTracks>()
      .track<T>(static_cast<size_t>(key));
}

struct AdvanceTracks : System<HasTracks> {
  size_t total = 0;
  void once(const float) override { total = 0; }
  void for_each_with(Entity &, HasTracks &tracks, const float dt) override {
    tracks.advance(scaled_dt(dt));
    total += tracks.size();
  }
  void after(const float) override {
    AFTERHOURS_PROFILE_COUNTER(profiling::default_collector(), "motion.tracks",
                               "count", static_cast<double>(total));
  }
};

inline void register_update_systems(SystemManager &sm) {
  sm.register_update_system(std::make_unique<AdvanceTracks>());
}

} // namespace motion
} // namespace afterhours
