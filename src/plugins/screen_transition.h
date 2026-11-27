#pragma once

// Screen transitions. The mechanism is shared for every effect: on a screen
// change, snapshot the outgoing frame into an offscreen target, then draw an
// effect over the (fully rendered) incoming frame for its duration. Any
// fresh press snaps it to done via `tick(dt, snap = true)`. Only the draw
// (and per-effect duration) differs per kind.
//
// Render textures are stored bottom-up, so snapshot/overlay blits use a
// negative source height — the same single flip presenting uses.
//
// The caller owns when transitions run (e.g. skip them in deterministic
// test mode) and which edges dissolve: `begin()` the transition on the
// edge, `tick()` every frame, `draw(dest)` after presenting, with `dest`
// the same letterboxed rect `present_render_texture()` blits into.

#include <algorithm>
#include <cstdint>

#include "../drawing_helpers.h"
#include "../graphics.h"
#include "color.h"

namespace afterhours {

enum struct TransitionKind {
  CrossDissolve, // outgoing fades out over incoming
  FadeToBlack,   // outgoing -> black -> incoming (a "dip")
  WipeLeft,      // incoming wipes in from the left
  COUNT,
};

class ScreenTransition {
public:
  void init(int w, int h) {
    if (loaded_) shutdown();
    snapshot_ = load_render_texture(w, h);
    width_ = w;
    height_ = h;
    loaded_ = true;
  }

  void shutdown() {
    if (!loaded_) return;
    unload_render_texture(snapshot_);
    loaded_ = false;
  }

  [[nodiscard]] bool active() const { return remaining_ > 0.f; }

  // Per-effect timing: the dip-to-black plays two halves (out then in) and
  // the wipe reads better slower; the cross-dissolve stays snappy.
  static float duration_for(TransitionKind kind) {
    switch (kind) {
    case TransitionKind::FadeToBlack:
      return 0.40f;
    case TransitionKind::WipeLeft:
      return 0.30f;
    default:
      return 0.18f; // CrossDissolve
    }
  }

  void set_kind(TransitionKind kind) { kind_ = kind; }
  [[nodiscard]] TransitionKind kind() const { return kind_; }
  void cycle_kind() {
    kind_ = static_cast<TransitionKind>(
        (static_cast<int>(kind_) + 1) %
        static_cast<int>(TransitionKind::COUNT));
  }
  [[nodiscard]] const char *kind_name() const {
    switch (kind_) {
    case TransitionKind::FadeToBlack:
      return "FadeToBlack";
    case TransitionKind::WipeLeft:
      return "WipeLeft";
    default:
      return "CrossDissolve";
    }
  }

  // Snapshot the outgoing screen (the render texture as it stands now)
  // and start the transition.
  void begin(TextureType outgoing) {
    if (!loaded_) return;
    duration_ = duration_for(kind_);
    remaining_ = duration_;
    const RectangleType dst{0.f, 0.f, static_cast<float>(width_),
                            static_cast<float>(height_)};
    begin_texture_mode(snapshot_);
    graphics::clear_background(Color{0, 0, 0, 255});
    draw_texture_pro(outgoing,
                     flip_src(static_cast<float>(outgoing.width),
                              static_cast<float>(outgoing.height)),
                     dst, Vector2Type{0.f, 0.f}, 0.f, colors::WHITE);
    end_texture_mode();
  }

  void tick(float dt, bool snap = false) {
    if (!active()) return;
    remaining_ = snap ? 0.f : std::max(0.f, remaining_ - dt);
  }

  // Draw the current effect over the incoming frame.
  void draw(RectangleType dest) const {
    if (!active() || !loaded_) return;
    const float k = duration_ > 0.f
                        ? std::clamp(remaining_ / duration_, 0.f, 1.f)
                        : 0.f; // 1 -> 0
    const float t = 1.f - k;   // 0 -> 1
    switch (kind_) {
    case TransitionKind::CrossDissolve:
      blit_prev(dest, k);
      break;
    case TransitionKind::FadeToBlack:
      draw_fade_to_black(dest, t);
      break;
    case TransitionKind::WipeLeft:
      draw_wipe_left(dest, t);
      break;
    default:
      break;
    }
  }

private:
  // Full-texture source rect with the bottom-up flip baked in.
  static RectangleType flip_src(float w, float h) {
    return RectangleType{0.f, 0.f, w, -h};
  }
  static std::uint8_t u8(float a) {
    return static_cast<std::uint8_t>(
        255.f * std::clamp(a, 0.f, 1.f));
  }

  // Blit the whole snapshot over dest at the given alpha (0..1).
  void blit_prev(RectangleType dest, float alpha) const {
    draw_texture_pro(detail::snapshot_texture(snapshot_),
                     flip_src(static_cast<float>(width_),
                              static_cast<float>(height_)),
                     dest, Vector2Type{0.f, 0.f}, 0.f,
                     Color{255, 255, 255, u8(alpha)});
  }

  // Outgoing shown, ramps to black by the midpoint, then black ramps off
  // to reveal the incoming frame (which is already the base layer).
  void draw_fade_to_black(RectangleType dest, float t) const {
    if (t < 0.5f) {
      blit_prev(dest, 1.f);
      draw_rectangle(dest, Color{0, 0, 0, u8(t * 2.f)});
    } else {
      draw_rectangle(dest, Color{0, 0, 0, u8((1.f - t) * 2.f)});
    }
  }

  // Outgoing retreats to the right, revealing the incoming frame from
  // the left. Crop both src and dest to the un-wiped right portion.
  void draw_wipe_left(RectangleType dest, float t) const {
    const float keep = 1.f - t; // fraction of outgoing still shown (right)
    if (keep <= 0.f) return;
    const float tw = static_cast<float>(width_);
    const float th = static_cast<float>(height_);
    const RectangleType src{t * tw, 0.f, keep * tw, -th};
    const RectangleType dst{dest.x + t * dest.width, dest.y,
                            keep * dest.width, dest.height};
    draw_texture_pro(detail::snapshot_texture(snapshot_), src, dst,
                     Vector2Type{0.f, 0.f}, 0.f, colors::WHITE);
  }

  struct detail {
    static TextureType snapshot_texture(const graphics::RenderTextureType &rt) {
#if defined(AFTER_HOURS_USE_RAYLIB)
      return rt.texture;
#elif defined(AFTER_HOURS_SOKOL)
      return TextureType{static_cast<float>(rt.width),
                         static_cast<float>(rt.height), rt.color_img_id,
                         rt.tex_view_id, rt.sampler_id};
#else
      (void)rt;
      return TextureType{};
#endif
    }
  };

  graphics::RenderTextureType snapshot_{};
  int width_ = 0;
  int height_ = 0;
  float remaining_ = 0.f;   // seconds left in the current transition
  float duration_ = 0.18f;  // total for the in-flight transition
  bool loaded_ = false;
  TransitionKind kind_ = TransitionKind::CrossDissolve;
};

} // namespace afterhours
