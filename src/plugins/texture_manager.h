
#pragma once
#include "../developer.h"

namespace afterhours {
namespace texture_manager {

#ifdef AFTER_HOURS_USE_RAYLIB
using Rectangle = raylib::Rectangle;
using Texture = raylib::Texture2D;
using Color = raylib::Color;

inline void draw_texture_pro(const Texture sheet, const Rectangle frame,
                             const Rectangle location, const Vector2Type size,
                             const float angle, const Color tint) {
  raylib::DrawTexturePro(sheet, frame, location, size, angle, tint);
}
#else
using Rectangle = RectangleType;
using Texture = TextureType;
using Color = ColorType;

inline void draw_texture_pro(const Texture, const Rectangle, const Rectangle,
                             const Vector2Type, const float, const Color) {}
#endif

#ifndef AFTERHOURS_SPRITE_SIZE_PX
#define AFTERHOURS_SPRITE_SIZE_PX 32
#endif

#ifndef AFTERHOURS_SPRITE_SHEET_NUM_SPRITES_WIDE
#define AFTERHOURS_SPRITE_SHEET_NUM_SPRITES_WIDE 32
#endif

constexpr static Rectangle idx_to_sprite_frame(const int i, const int j) {
  return Rectangle{.x = (float)i * AFTERHOURS_SPRITE_SIZE_PX,
                   .y = (float)j * AFTERHOURS_SPRITE_SIZE_PX,
                   .width = AFTERHOURS_SPRITE_SIZE_PX,
                   .height = AFTERHOURS_SPRITE_SIZE_PX};
}

constexpr static std::pair<int, int> idx_to_next_sprite_location(int i, int j) {
  i++;
  if (i == AFTERHOURS_SPRITE_SHEET_NUM_SPRITES_WIDE) {
    i = 0;
    j++;
  }
  return {i, j};
}

struct TransformData {
  // TODO stolen from transform...
  Vector2Type position;
  Vector2Type size;
  float angle;

  TransformData(const Vector2Type pos, const Vector2Type size_,
                const float angle_)
      : position(pos), size(size_), angle(angle_) {}

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

  Vector2Type center() const {
    return {
        position.x + (size.x / 2.f),
        position.y + (size.y / 2.f),
    };
  }
};

struct HasTexture : BaseComponent {
  Texture texture;

  enum struct Alignment {
    // TODO add support for InnerLeft, InnerRight
    Left,
    Center,
    Right,
    None = Left,
  } alignment = Alignment::None;

  HasTexture(const Texture &tex, const Alignment align)
      : texture(tex), alignment(align) {}
};

struct HasSpritesheet : BaseComponent {
  Texture texture;
  HasSpritesheet(const Texture tex) : texture(tex) {}
};

struct HasSprite : BaseComponent {
  TransformData transform;

  Rectangle frame;
  float scale;
  Color colorTint;
  HasSprite(const Vector2Type pos, const Vector2Type size_, const float angle_,
            const Rectangle frm, const float scl, const Color colorTintIn)
      : transform(pos, size_, angle_), frame{frm}, scale{scl},
        colorTint{colorTintIn} {}

  auto &update_transform(const Vector2Type &pos, const Vector2Type &size_,
                         const float &ang) {
    return transform.update_position(pos).update_size(size_).update_angle(ang);
  }

  auto &update_color(const Color col) {
    colorTint = col;
    return *this;
  }

  float angle() const { return transform.angle; }

  Rectangle destination() const {
    return Rectangle{
        transform.center().x,
        transform.center().y,
        frame.width * scale,
        frame.height * scale,
    };
  }
};

struct HasAnimation : BaseComponent {
  TransformData transform;

  Vector2Type start_position;
  Vector2Type cur_frame_position;
  int total_frames = 10;
  float frame_dur = 1.f / 20.f;
  float frame_time = 1.f / 20.f;
  bool once = false;
  float scale = 1.f;

  int cur_frame = 0;
  float rotation = 0;
  Color colorTint;

  HasAnimation(const Vector2Type position, const Vector2Type size,
               const float angle, const Vector2Type start_position_,
               const int total_frames_, const float frame_dur_,
               const bool once_, const float scale_, const int cur_frame_,
               const float rot, const Color tint)
      : transform(position, size, angle), start_position(start_position_),
        cur_frame_position(start_position_), total_frames(total_frames_),
        frame_dur(frame_dur_), frame_time(frame_dur_), once(once_),
        scale(scale_), cur_frame(cur_frame_), rotation(rot), colorTint(tint) {}

  auto &update_transform(const Vector2Type &pos, const Vector2Type &size_,
                         const float &ang) {
    return transform.update_position(pos).update_size(size_).update_angle(ang);
  }

  float angle() const { return transform.angle; }
};

struct AnimationUpdateCurrentFrame : System<HasAnimation> {

  virtual void for_each_with(Entity &entity, HasAnimation &hasAnimation,
                             const float dt) override {
    hasAnimation.frame_time -= dt;
    if (hasAnimation.frame_time > 0) {
      return;
    }
    hasAnimation.frame_time = hasAnimation.frame_dur;

    if (hasAnimation.cur_frame >= hasAnimation.total_frames) {
      if (hasAnimation.once) {
        entity.cleanup = true;
        return;
      }
      hasAnimation.cur_frame = 0;
    }

    const auto [i, j] =
        idx_to_next_sprite_location((int)hasAnimation.cur_frame_position.x,
                                    (int)hasAnimation.cur_frame_position.y);

    hasAnimation.cur_frame_position = Vector2Type{(float)i, (float)j};
    hasAnimation.cur_frame++;
  }
};

struct RenderSprites : System<HasSprite> {

  mutable Texture sheet;

  virtual void once(const float) const override {
    sheet = EntityHelper::get_singleton_cmp<HasSpritesheet>()->texture;
  }

  virtual void for_each_with(const Entity &, const HasSprite &hasSprite,
                             const float) const override {

    draw_texture_pro(sheet, hasSprite.frame, hasSprite.destination(),
                     Vector2Type{hasSprite.transform.size.x / 2.f,
                                 hasSprite.transform.size.y / 2.f},
                     hasSprite.angle(), hasSprite.colorTint);
  }
};

struct RenderAnimation : System<HasAnimation> {

  mutable Texture sheet;

  virtual void once(const float) const override {
    sheet = EntityHelper::get_singleton_cmp<HasSpritesheet>()->texture;
  }

  virtual void for_each_with(const Entity &, const HasAnimation &hasAnimation,
                             const float) const override {

    const auto [i, j] = hasAnimation.cur_frame_position;
    const Rectangle frame = idx_to_sprite_frame((int)i, (int)j);

    draw_texture_pro(sheet, frame,
                     Rectangle{
                         hasAnimation.transform.center().x,
                         hasAnimation.transform.center().y,
                         frame.width * hasAnimation.scale,
                         frame.height * hasAnimation.scale,
                     },
                     Vector2Type{frame.width / 2.f,
                                 frame.height / 2.f}, // transform.center(),
                     hasAnimation.angle() + hasAnimation.rotation,
                     hasAnimation.colorTint);
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

static void register_update_systems(SystemManager &sm) {
  sm.register_update_system(std::make_unique<AnimationUpdateCurrentFrame>());
}

static void register_render_systems(SystemManager &sm) {
  sm.register_render_system(std::make_unique<RenderSprites>());
  sm.register_render_system(std::make_unique<RenderAnimation>());
}

} // namespace texture_manager
} // namespace afterhours
