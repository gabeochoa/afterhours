#pragma once

#include <functional>

#include "graphics.h"

namespace afterhours::game {

// One-call runner for a raw-draw game: window, frame loop, fixed-step
// updates, clear + draw, and quit are owned here. ECS games tick their
// SystemManager from `update`; there is deliberately no subclass to
// override and no UI setup (that is ui::run's job).
//
//     return game::run({.run = {.title = "pong"},
//                       .setup = [&] { /* create entities */ },
//                       .update = [&](float dt) { systems.run(dt); },
//                       .draw = [&] { /* draw the world */ }});
struct Config {
  graphics::RunConfig run;
  std::function<void()> setup = nullptr;
  // Called once per sim step; RunConfig::fixed_dt/sim_steps pick the step.
  std::function<void(float)> update = nullptr;
  // Called once per frame inside begin_drawing/end_drawing, after the clear.
  std::function<void()> draw = nullptr;
  std::function<void()> cleanup = nullptr;
  graphics::PlatformAPI::color_type clear_color{0, 0, 0, 255};
  bool clear = true;
  // 0 runs until the window closes or graphics::request_quit(); >0 also
  // stops after that many frames, which is how headless tests drive it.
  int max_frames = 0;
};

inline int run(Config cfg) {
  int frames = 0;
  graphics::RunConfig rc = cfg.run;
  rc.init = [cfg] {
    if (cfg.setup) cfg.setup();
  };
  rc.cleanup = cfg.cleanup;
  rc.frame = [cfg, rc, &frames] {
    const float dt = graphics::get_frame_time();
    for (int step = 0; step < rc.step_count(); ++step)
      if (cfg.update) cfg.update(rc.step_dt(dt));

    // Direct-to-window bracket both backends expose; the offscreen
    // begin_frame/end_frame pair belongs to graphics::init apps.
    graphics::begin_drawing();
    if (cfg.clear) graphics::clear_background(cfg.clear_color);
    if (cfg.draw) cfg.draw();
    graphics::end_drawing();

    if (cfg.max_frames > 0 && ++frames >= cfg.max_frames)
      graphics::request_quit();
  };
  graphics::run(rc);
  return 0;
}

} // namespace afterhours::game
