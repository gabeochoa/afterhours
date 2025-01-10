
#pragma once
#include "../developer.h"

namespace afterhours {
namespace texture_manager {

#ifdef AFTER_HOURS_USE_RAYLIB
using Rectangle = raylib::Rectangle;
using Texture = raylib::Texture2D;
using Color = raylib::Color;

inline void draw_texture_pro(Texture sheet, Rectangle frame, Rectangle location,
                             Vector2Type size, float angle, Color tint) {
  raylib::DrawTexturePro(sheet, frame, location, size, angle, tint);
}
#else
using Rectangle = RectangleType;
using Texture = TextureType;
using Color = ColorType;

inline void draw_texture_pro(Texture, Rectangle, Rectangle, Vector2Type, float,
                             Color) {}
#endif

struct HasSpritesheet : BaseComponent {
  Texture texture;
  HasSpritesheet(const Texture tex) : texture(tex) {}
};

struct HasSprite : BaseComponent {
  // TODO stolen from transform...
  Vector2Type position;
  Vector2Type size;
  float angle;

  Rectangle frame;
  float scale;
  Color colorTint;
  HasSprite(Vector2Type pos, Vector2Type size_, float angle_, Rectangle frm,
            float scl, Color colorTintIn)
      : position(pos), size(size_), angle(angle_), frame{frm}, scale{scl},
        colorTint{colorTintIn} {}

  auto &update_position(const Vector2Type &pos) {
    position = pos;
    return *this;
  }

  auto &update_size(const Vector2Type &size_) {
    size = size_;
    return *this;
  }

  auto &update_angle(const float &ang) {
    angle = ang;
    return *this;
  }

  auto &update_transform(const Vector2Type &pos, const Vector2Type &size_,
                         const float &ang) {
    return update_position(pos).update_size(size_).update_angle(ang);
  }

  Vector2Type center() const {
    return {
        position.x + (size.x / 2.f),
        position.y + (size.y / 2.f),
    };
  }

  Rectangle destination() const {
    return Rectangle{
        center().x,
        center().y,
        frame.width * scale,
        frame.height * scale,
    };
  }
};

struct RenderSprites : System<HasSprite> {

  Texture sheet;

  virtual void once(float) override {
    sheet = EntityHelper::get_singleton_cmp<HasSpritesheet>()->texture;
  }

  virtual void for_each_with(const Entity &, const HasSprite &hasSprite,
                             float) const override {

    draw_texture_pro(sheet, hasSprite.frame, hasSprite.destination(),
                     Vector2Type{hasSprite.size.x / 2.f,
                                 hasSprite.size.y / 2.f}, // transform.center(),
                     hasSprite.angle, hasSprite.colorTint);
  }
};

static void add_singleton_components(Entity &entity,
                                     const Texture &spriteSheet) {
  entity.addComponent<HasSpritesheet>(spriteSheet);
  EntityHelper::registerSingleton<HasSpritesheet>(entity);
}

static void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<HasSpritesheet>>());
}

static void register_update_systems(SystemManager &) {}
static void register_render_systems(SystemManager &sm) {
  sm.register_render_system(std::make_unique<RenderSprites>());
}

} // namespace texture_manager
} // namespace afterhours
