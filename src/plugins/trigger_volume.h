#pragma once

// Entrants are local ids; multiplayer replication is the game's job.

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_set>
#include <vector>

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"

namespace afterhours {

struct trigger_volume : developer::Plugin {

  struct AABB {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    float z = 0.f, d = 0.f; // d==0 means 2D: z is ignored
    [[nodiscard]] bool contains(float px, float py, float pz = 0.f) const {
      if (px < x || px > x + w || py < y || py > y + h) return false;
      return d == 0.f || (pz >= z && pz <= z + d);
    }
  };

  struct Candidate {
    EntityID id = -1;
    float x = 0.f, y = 0.f, z = 0.f;
  };

  enum class Phase { Empty, Occupied, Complete };

  struct Volume {
    AABB bounds{};
    int required_count = 1;          // entrants needed for Complete
    float presence_rate = 6.f;       // exp. approach rate, per second
    std::function<bool(EntityID)> validator; // empty = accept all

    std::unordered_set<EntityID> entrants;
    std::vector<EntityID> entered; // last update() only
    std::vector<EntityID> exited;
    float presence = 0.f;

    [[nodiscard]] int count() const { return (int)entrants.size(); }
    [[nodiscard]] bool inside(EntityID id) const { return entrants.count(id) != 0; }
    [[nodiscard]] Phase phase() const {
      if (entrants.empty()) return Phase::Empty;
      return count() >= std::max(1, required_count) ? Phase::Complete
                                                     : Phase::Occupied;
    }
    [[nodiscard]] float progress() const {
      return std::clamp((float)count() / (float)std::max(1, required_count),
                        0.f, 1.f);
    }

    void update(const std::vector<Candidate> &candidates, float dt) {
      std::unordered_set<EntityID> next;
      for (const auto &c : candidates) {
        if (!bounds.contains(c.x, c.y, c.z)) continue;
        if (validator && !validator(c.id)) continue;
        next.insert(c.id);
      }
      entered.clear();
      exited.clear();
      for (EntityID id : next)
        if (!entrants.count(id)) entered.push_back(id);
      for (EntityID id : entrants)
        if (!next.count(id)) exited.push_back(id);
      entrants = std::move(next);

      const float target = 1.f - static_cast<float>(entrants.empty());
      // Frame-rate independent; stays in [0,1].
      const float k = dt <= 0.f ? 1.f : 1.f - std::exp(-presence_rate * dt);
      presence = std::clamp(presence + (target - presence) * k, 0.f, 1.f);
    }

    void reset() {
      entrants.clear();
      entered.clear();
      exited.clear();
      presence = 0.f;
    }
  };

  struct HasTriggerVolume : BaseComponent {
    Volume volume;
    HasTriggerVolume() = default;
    explicit HasTriggerVolume(AABB b, int required = 1) {
      volume.bounds = b;
      volume.required_count = required;
    }
  };

  // Game supplies position reads via make_candidate.
  template <typename TransformType>
  struct UpdateTriggerVolumes : System<HasTriggerVolume> {
    std::function<Candidate(const Entity &, const TransformType &)>
        make_candidate;
    std::vector<Candidate> candidates;

    void for_each_with(Entity &, HasTriggerVolume &hv, float dt) override {
      if (!make_candidate) return;
      candidates.clear();
      EntityQuery().whereHasComponent<TransformType>().for_each_stream(
          [&](Entity &ent) {
            candidates.push_back(
                make_candidate(ent, ent.get<TransformType>()));
          });
      hv.volume.update(candidates, dt);
    }
  };

  static void add_singleton_components(Entity &) {}
  static void enforce_singletons(SystemManager &) {}
  static void register_update_systems(SystemManager &) {}

  template <typename TransformType>
  static void register_update_systems(
      SystemManager &sm,
      std::function<Candidate(const Entity &, const TransformType &)>
          make_candidate) {
    auto sys = std::make_unique<UpdateTriggerVolumes<TransformType>>();
    sys->make_candidate = std::move(make_candidate);
    sm.register_update_system(std::move(sys));
  }
};

static_assert(developer::PluginCore<trigger_volume>,
              "trigger_volume must implement the core plugin interface");

template <typename TransformType> struct TriggerVolumePlugin {
  std::function<trigger_volume::Candidate(const Entity &, const TransformType &)>
      make_candidate{};

  void install(PluginContext &ctx) const {
    auto sys =
        std::make_unique<trigger_volume::UpdateTriggerVolumes<TransformType>>();
    sys->make_candidate = make_candidate;
    ctx.add_system(Stage::Update, std::move(sys));
  }
};

} // namespace afterhours
