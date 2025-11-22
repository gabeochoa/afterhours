#pragma once

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/system.h"
#include "../developer.h"
#include <functional>
#include <limits>
#include <set>

#ifdef AFTER_HOURS_USE_RAYLIB
namespace raylib {
  struct Vector2;
}
using vec2 = raylib::Vector2;
#else
struct vec2 {
  float x, y;
};
#endif

namespace afterhours {

struct collision : developer::Plugin {

  struct CollisionConfig {
    float mass{1.f};
    float friction{0.f};
    float restitution{.5f};
  };

  template <typename TransformType>
  struct UpdateCollidingEntities : System<TransformType> {
    using Transform = TransformType;

    std::set<int> ids;

    struct Config {
      std::function<float()> get_collision_scalar;
      std::function<float()> get_max_speed;
    } config;

    struct Callbacks {
      std::function<bool(const Entity &)> should_skip_entity;
      std::function<bool(const Entity &)> is_floor_overlay;
      std::function<bool(const Entity &)> gets_absorbed;
      std::function<std::optional<int>(const Entity &)> get_absorber_parent_id;
      std::function<vec2(const vec2 &)> normalize_vec;
      std::function<float(const vec2 &, const vec2 &)> dot_product;
      std::function<float(const vec2 &)> vector_length;
      std::function<float(const Transform &)> get_speed;
      std::function<bool(const Transform &, const Transform &)> check_overlap;
    } callbacks;

    virtual void once(float) override { ids.clear(); }

    void positional_correction(Transform &a, Transform &b,
                               const vec2 &collisionNormal,
                               float penetrationDepth) {
      float correctionMagnitude =
          std::max(penetrationDepth, 0.0f) /
          (1.0f / a.collision_config.mass + 1.0f / b.collision_config.mass);
      vec2 scaled = {collisionNormal.x * correctionMagnitude, collisionNormal.y * correctionMagnitude};
      vec2 correction = callbacks.normalize_vec(scaled);

      float inv_mass_a = 1.0f / a.collision_config.mass;
      float inv_mass_b = 1.0f / b.collision_config.mass;
      a.position = {a.position.x - correction.x * inv_mass_a, a.position.y - correction.y * inv_mass_a};
      b.position = {b.position.x + correction.x * inv_mass_b, b.position.y + correction.y * inv_mass_b};
    }

    void resolve_collision(Transform &a, Transform &b, const float dt) {
      vec2 diff = {b.position.x - a.position.x, b.position.y - a.position.y};
      vec2 collisionNormal = callbacks.normalize_vec(diff);

      float impulse = calculate_impulse(a, b, collisionNormal);
      float collision_scalar = config.get_collision_scalar();
      float impulse_magnitude = impulse * collision_scalar * dt;
      vec2 impulseVector = {collisionNormal.x * impulse_magnitude, collisionNormal.y * impulse_magnitude};

      if (a.collision_config.mass > 0.0f &&
          a.collision_config.mass != std::numeric_limits<float>::max()) {
        float inv_mass_a = 1.0f / a.collision_config.mass;
        a.velocity = {a.velocity.x - impulseVector.x * inv_mass_a, a.velocity.y - impulseVector.y * inv_mass_a};
      }

      if (b.collision_config.mass > 0.0f &&
          b.collision_config.mass != std::numeric_limits<float>::max()) {
        float inv_mass_b = 1.0f / b.collision_config.mass;
        b.velocity = {b.velocity.x + impulseVector.x * inv_mass_b, b.velocity.y + impulseVector.y * inv_mass_b};
      }

      vec2 relativeVelocity = {b.velocity.x - a.velocity.x, b.velocity.y - a.velocity.y};
      float dot = callbacks.dot_product(relativeVelocity, collisionNormal);
      vec2 scaled_normal = {collisionNormal.x * dot, collisionNormal.y * dot};
      vec2 tangent_vec = {relativeVelocity.x - scaled_normal.x, relativeVelocity.y - scaled_normal.y};
      vec2 tangent = callbacks.normalize_vec(tangent_vec);

      float frictionImpulseMagnitude =
          callbacks.dot_product(relativeVelocity, tangent) /
          (1.0f / a.collision_config.mass + 1.0f / b.collision_config.mass);
      float frictionCoefficient =
          std::sqrt(a.collision_config.friction * b.collision_config.friction);
      frictionImpulseMagnitude =
          std::clamp(frictionImpulseMagnitude, -impulse * frictionCoefficient,
                     impulse * frictionCoefficient);

      float friction_magnitude = frictionImpulseMagnitude * collision_scalar * dt;
      vec2 frictionImpulse = {tangent.x * friction_magnitude, tangent.y * friction_magnitude};

      if (a.collision_config.mass > 0.0f &&
          a.collision_config.mass != std::numeric_limits<float>::max()) {
        float inv_mass_a = 1.0f / a.collision_config.mass;
        a.velocity = {a.velocity.x - frictionImpulse.x * inv_mass_a, a.velocity.y - frictionImpulse.y * inv_mass_a};
      }

      if (b.collision_config.mass > 0.0f &&
          b.collision_config.mass != std::numeric_limits<float>::max()) {
        float inv_mass_b = 1.0f / b.collision_config.mass;
        b.velocity = {b.velocity.x + frictionImpulse.x * inv_mass_b, b.velocity.y + frictionImpulse.y * inv_mass_b};
      }

      float maxSpeed = config.get_max_speed();
      if (callbacks.get_speed(a) > maxSpeed) {
        vec2 normalized = callbacks.normalize_vec(a.velocity);
        a.velocity = {normalized.x * maxSpeed, normalized.y * maxSpeed};
      }
      if (callbacks.get_speed(b) > maxSpeed) {
        vec2 normalized = callbacks.normalize_vec(b.velocity);
        b.velocity = {normalized.x * maxSpeed, normalized.y * maxSpeed};
      }

      float penetrationDepth = calculate_penetration_depth(a, b);
      positional_correction(a, b, collisionNormal, penetrationDepth);
    }

    float calculate_penetration_depth(const Transform &a, const Transform &b) {
      float overlapX =
          std::min(a.position.x + a.size.x, b.position.x + b.size.x) -
          std::max(a.position.x, b.position.x);

      float overlapY =
          std::min(a.position.y + a.size.y, b.position.y + b.size.y) -
          std::max(a.position.y, b.position.y);

      if (overlapX <= 0.0f || overlapY <= 0.0f)
        return 0.0f;

      return std::min(overlapX, overlapY);
    }

    float calculate_dynamic_restitution(const Transform &a, const Transform &b) {
      float baseRestitution = std::min(a.collision_config.restitution,
                                       b.collision_config.restitution);

      vec2 relativeVelocity = {b.velocity.x - a.velocity.x, b.velocity.y - a.velocity.y};
      float speed = callbacks.vector_length(relativeVelocity);

      if (speed > (config.get_max_speed() * .75f)) {
        baseRestitution *= 0.5f;
      }

      return baseRestitution;
    }

    float calculate_impulse(const Transform &a, const Transform &b,
                            const vec2 &collisionNormal) {
      vec2 relativeVelocity = {b.velocity.x - a.velocity.x, b.velocity.y - a.velocity.y};
      float velocityAlongNormal = callbacks.dot_product(relativeVelocity, collisionNormal);

      if (velocityAlongNormal > 0.0f)
        return 0.0f;

      float restitution = calculate_dynamic_restitution(a, b);

      float impulse = -(1.0f + restitution) * velocityAlongNormal;
      impulse /=
          (1.0f / a.collision_config.mass + 1.0f / b.collision_config.mass);

      return impulse;
    }

    virtual void for_each_with(Entity &entity, Transform &transform,
                               float dt) override {

      if (ids.contains(entity.id)) {
        return;
      }

      if (callbacks.should_skip_entity && callbacks.should_skip_entity(entity)) {
        return;
      }

      if (callbacks.is_floor_overlay && callbacks.is_floor_overlay(entity)) {
        return;
      }

      if (callbacks.gets_absorbed && callbacks.gets_absorbed(entity)) {
        return;
      }

      auto can_collide = EntityQuery()
                             .whereHasComponent<Transform>()
                             .whereNotID(entity.id)
                             .gen();

      for (Entity &other : can_collide) {
        if (callbacks.is_floor_overlay && callbacks.is_floor_overlay(other)) {
          continue;
        }

        Transform &b = other.get<Transform>();

        if (callbacks.gets_absorbed && callbacks.gets_absorbed(other)) {
          if (callbacks.get_absorber_parent_id) {
            auto parent_id = callbacks.get_absorber_parent_id(other);
            if (parent_id.has_value() && parent_id.value() == entity.id) {
              ids.insert(other.id);
              continue;
            }
          }
          ids.insert(other.id);
          continue;
        }

        if (!callbacks.check_overlap(transform, b)) {
          continue;
        }

        resolve_collision(transform, b, dt);
        ids.insert(other.id);
      }
    }
  };

  static void register_update_systems(SystemManager &sm) {
  }
};

} // namespace afterhours
