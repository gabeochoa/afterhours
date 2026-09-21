#pragma once

#include "animation.h"
#include "ui/motion_config.h"

namespace afterhours {
namespace presets {

inline ui::MotionRule fade_up(float distance = 12.f, float seconds = 0.5f, float delay = 0.f) {
  return {ui::MotionTrigger::Appear,
          {.translate_y = {distance, 0.f}, .opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad},
          delay};
}

inline ui::MotionRule fade_in(float seconds = 0.25f, float delay = 0.f) {
  return {ui::MotionTrigger::Appear, {.opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad}, delay};
}

inline ui::MotionRule pop_in(float from_scale = 0.3f, motion::Spring spring = motion::Spring::bouncy()) {
  return {ui::MotionTrigger::Appear, {.scale = {from_scale, 1.f}, .opacity = {0.f, 1.f}}, spring};
}

inline ui::MotionRule hover_lift(float pixels = 4.f, float scale = 1.05f,
                                 motion::Spring spring = motion::Spring::snappy()) {
  return {ui::MotionTrigger::Hover, {.scale = scale, .translate_y = -pixels}, spring};
}

inline ui::MotionRule press_squash(float scale = 0.92f, motion::Spring spring = motion::Spring::snappy()) {
  return {ui::MotionTrigger::Press, {.scale = scale}, spring};
}

inline ui::MotionRule slide_in(float from_x, float seconds = 0.25f, float delay = 0.f) {
  return {ui::MotionTrigger::Appear, {.translate_x = {from_x, 0.f}, .opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad}, delay};
}

inline motion::Timeline shake(float seconds = 0.28f) {
  return {.keys = {{0.f, 0.f}, {seconds * 0.2857f, 1.f}, {seconds * 0.5714f, -1.f}, {seconds * 0.7857f, 0.667f}, {seconds, 0.f}},
          .curve = motion::curves::ease_out_quad};
}

inline motion::Timeline spin(float seconds = 0.9f) {
  return {.keys = {{0.f, 0.f}, {seconds, 1.f}}, .repeat = motion::Timeline::Repeat::Loop};
}

inline motion::Timeline pulse(float seconds = 0.5f) {
  return {.keys = {{0.f, 0.f}, {seconds, 1.f}}, .repeat = motion::Timeline::Repeat::PingPong,
          .curve = motion::curves::ease_in_out_quad};
}

} // namespace presets
} // namespace afterhours
