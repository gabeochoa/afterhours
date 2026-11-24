#pragma once

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../warn_once.h"

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
    void set_position(float, float) {
      warn_once(0, "@notimplemented camera::HasCamera::set_position on "
                   "this backend (camera is raylib-only)");
    }
    void set_offset(float, float) {
      warn_once(0, "@notimplemented camera::HasCamera::set_offset on "
                   "this backend (camera is raylib-only)");
    }
    void set_zoom(float) {
      warn_once(0, "@notimplemented camera::HasCamera::set_zoom on this "
                   "backend (camera is raylib-only)");
    }
    void set_rotation(float) {
      warn_once(0, "@notimplemented camera::HasCamera::set_rotation on "
                   "this backend (camera is raylib-only)");
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

  static void register_begin_camera(SystemManager &) {}

  static void register_end_camera(SystemManager &) {}
#endif
};

// Compile-time verification that camera satisfies the PluginCore concept
static_assert(developer::PluginCore<camera>,
              "camera must implement the core plugin interface");

struct CameraPlugin {
  Vector2Type target{0.f, 0.f};
  Vector2Type offset{0.f, 0.f};
  float rotation = 0.f;
  float zoom = 0.75f;
  bool with_begin_end = false;

  void install(PluginContext &ctx) const {
    const bool existed = ctx.collection.has_singleton<camera::HasCamera>();
    auto &cam = ctx.ensure_singleton<camera::HasCamera>();
    if (!existed) {
#ifdef AFTER_HOURS_USE_RAYLIB
      cam.camera.target = target;
      cam.camera.offset = offset;
      cam.camera.rotation = rotation;
      cam.camera.zoom = zoom;
#else
      (void)cam;
#endif
    }
    if (with_begin_end) {
#ifdef AFTER_HOURS_USE_RAYLIB
      ctx.add_system(Stage::PreRender,
                     std::make_unique<camera::BeginCameraMode>());
      ctx.add_system(Stage::OverlayRender,
                     std::make_unique<camera::EndCameraMode>());
#endif
    }
  }
};
static_assert(InstallablePlugin<CameraPlugin>);

} // namespace afterhours
