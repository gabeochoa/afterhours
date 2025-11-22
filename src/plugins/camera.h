#pragma once

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"

namespace afterhours {

struct camera : developer::Plugin {

#ifdef AFTER_HOURS_USE_RAYLIB
  struct HasCamera : public BaseComponent {
    raylib::Camera2D camera;

    HasCamera() {
      camera.offset = {0.0f, 0.0f};
      camera.target = {0.0f, 0.0f};
      camera.rotation = 0.0f;
      camera.zoom = 0.75f;
    }

    void set_position(raylib::Vector2 position) { camera.target = position; }

    void set_offset(raylib::Vector2 offset) { camera.offset = offset; }

    void set_zoom(float zoom) { camera.zoom = zoom; }

    void set_rotation(float rotation) { camera.rotation = rotation; }
  };

  struct BeginCameraMode : System<HasCamera> {
    virtual void once(float) const override {
      auto *camera_entity = EntityHelper::get_singleton_cmp<HasCamera>();
      if (camera_entity) {
        raylib::BeginMode2D(camera_entity->camera);
      }
    }
  };

  struct EndCameraMode : System<HasCamera> {
    virtual void once(float) const override {
      auto *camera_entity = EntityHelper::get_singleton_cmp<HasCamera>();
      if (camera_entity) {
        raylib::EndMode2D();
      }
    }
  };

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<HasCamera>();
    EntityHelper::registerSingleton<HasCamera>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<HasCamera>>());
  }

  static void register_update_systems(SystemManager &) {}

  static void register_begin_camera(SystemManager &sm) {
    sm.register_render_system(std::make_unique<BeginCameraMode>());
  }

  static void register_end_camera(SystemManager &sm) {
    sm.register_render_system(std::make_unique<EndCameraMode>());
  }
#else
  struct HasCamera : public BaseComponent {
    HasCamera() {}
    void set_position(float, float) {}
    void set_offset(float, float) {}
    void set_zoom(float) {}
    void set_rotation(float) {}
  };

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<HasCamera>();
    EntityHelper::registerSingleton<HasCamera>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<HasCamera>>());
  }

  static void register_update_systems(SystemManager &) {}

  static void register_begin_camera(SystemManager &) {}

  static void register_end_camera(SystemManager &) {}
#endif
};

} // namespace afterhours
