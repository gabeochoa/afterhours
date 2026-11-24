#pragma once

#include <cmath>

#include "../../ecs.h"
#include "../input_system.h"

namespace afterhours {

namespace ui {

struct MousePointerState {
  // pos and delta are in the same space as UIComponent::rect() -- letterbox
  // and resolution scaling are already applied. Callers reaching for the
  // backend's raw mouse instead have to redo that conversion by hand.
  input::MousePosition pos{-1.f, -1.f};

  // Movement since the previous frame. Derived from pos rather than the
  // backend's mouse delta, which is raw window pixels. Zero when there is no
  // cursor, and no dead zone, unlike moved_this_frame below.
  input::MousePosition delta{};

  bool left_down = false;
  bool just_pressed = false;
  bool just_released = false;

  // Secondary button. Kept separate rather than generalising to an array: the
  // left button drives hot/active/drag and the right one only ever asks "was
  // there a click on this, and where", so sharing the machinery would mean
  // teaching all of it which button it is talking about.
  bool right_down = false;
  bool right_just_pressed = false;
  bool right_just_released = false;

  bool middle_down = false;
  bool middle_just_pressed = false;
  bool middle_just_released = false;

  int click_count = 0;
  float clock_s = 0.f;
  float last_click_time = -1.f;
  input::MousePosition last_click_pos{};
  EntityID last_click_target = -1;

  static constexpr float multi_click_time_s = 0.4f;

  [[nodiscard]] int note_click(float now, EntityID target) {
    const float dx = pos.x - last_click_pos.x;
    const float dy = pos.y - last_click_pos.y;
    const bool within = last_click_time >= 0.f &&
                        (now - last_click_time) < multi_click_time_s &&
                        (dx * dx + dy * dy) <=
                            multi_click_dist_px * multi_click_dist_px &&
                        target == last_click_target;
    click_count = within ? (click_count >= 3 ? 1 : click_count + 1) : 1;
    last_click_time = now;
    last_click_pos = pos;
    last_click_target = target;
    return click_count;
  }

  void reset_click_count() {
    click_count = 0;
    last_click_time = -1.f;
    last_click_target = -1;
  }

  input::MousePosition press_pos{};
  bool press_moved = false;
  bool moved_this_frame = false; // pos changed since last frame (any button)

  static constexpr float press_drag_threshold_px = 6.0f;
  static constexpr float multi_click_dist_px = press_drag_threshold_px;

  // Below this, the cursor is parked. An exact compare made sub-pixel jitter
  // (a hi-DPI position rescaled to render space flickers in the last mantissa
  // bit) read as intent, which is enough to yank focus away from the keyboard.
  static constexpr float move_dead_zone_px = 2.0f;

  // A distance test rather than `!=`, because the exact compare counted
  // sub-pixel jitter as intent. The two non-finite cases have to be handled
  // explicitly or NaN decides by accident: with no cursor at all (headless)
  // `!=` reported movement on every frame forever, while a plain distance test
  // would let one NaN frame poison `prev` and report *no* movement forever.
  [[nodiscard]] bool moved_since(const input::MousePosition &prev) const {
    const bool now_ok = std::isfinite(pos.x) && std::isfinite(pos.y);
    if (!now_ok)
      return false; // no cursor: not moving
    if (!std::isfinite(prev.x) || !std::isfinite(prev.y))
      return true; // a cursor just appeared
    const float dx = pos.x - prev.x, dy = pos.y - prev.y;
    return dx * dx + dy * dy > move_dead_zone_px * move_dead_zone_px;
  }

  void update(float dt, EntityID hot_id) {
    const auto prev_pos = pos;
    pos = input::get_mouse_position();
    // A backend with no cursor yet (pre-init sokol) can report NaN; pin
    // it off-screen so every read below sees the pre-event position.
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) {
      pos = input::MousePosition{-1.f, -1.f};
    }
    moved_this_frame = moved_since(prev_pos);
    // No dead zone here, unlike moved_this_frame: a drag has to track every
    // pixel. Both ends must be finite or the subtraction yields NaN, which a
    // caller would then accumulate into its own state permanently.
    const bool have_both = std::isfinite(pos.x) && std::isfinite(pos.y) &&
                           std::isfinite(prev_pos.x) &&
                           std::isfinite(prev_pos.y);
    delta = have_both ? input::MousePosition{pos.x - prev_pos.x,
                                             pos.y - prev_pos.y}
                      : input::MousePosition{0.f, 0.f};

    const bool prev_left_down = left_down;
    left_down = input::is_mouse_button_down(0);
    just_pressed = !prev_left_down && left_down;
    just_released = prev_left_down && !left_down;

    const bool prev_right_down = right_down;
    right_down = input::is_mouse_button_down(1);
    right_just_pressed = !prev_right_down && right_down;
    right_just_released = prev_right_down && !right_down;

    const bool prev_middle_down = middle_down;
    middle_down = input::is_mouse_button_down(2);
    middle_just_pressed = !prev_middle_down && middle_down;
    middle_just_released = prev_middle_down && !middle_down;

    clock_s += dt;
    if (just_released) {
      if (press_moved) {
        reset_click_count();
      } else {
        note_click(clock_s, hot_id);
      }
    }

    if (just_pressed) {
      press_pos = pos;
      press_moved = false;
    }

    if (!left_down) {
      press_moved = false;
    } else if (!press_moved) {
      const float dx = pos.x - press_pos.x;
      const float dy = pos.y - press_pos.y;
      const float dist_sq = (dx * dx) + (dy * dy);
      if (dist_sq > (press_drag_threshold_px * press_drag_threshold_px)) {
        press_moved = true;
      }
    }
  }
};

} // namespace ui

} // namespace afterhours
