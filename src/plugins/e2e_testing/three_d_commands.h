#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <string>

#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace three_d_commands {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
  bool operator==(const Vec3 &) const = default;
};

struct CameraPose {
  Vec3 position{};
  Vec3 target{};
  Vec3 up{0.f, 1.f, 0.f};
  float fovy = 45.f;
};

struct RayHit {
  bool hit = false;
  Vec3 point{};
  Vec3 normal{};
  float distance = 0.f;
  std::string name;
};

struct Providers {
  std::function<CameraPose()> get_camera;
  std::function<void(const CameraPose &)> set_camera;
  std::function<Vec3()> get_player_pos;
  std::function<void(const Vec3 &)> set_player_pos;
  std::function<RayHit(Vec3 origin, Vec3 direction)> raycast;
};

inline Providers &providers() {
  static Providers p;
  return p;
}
inline std::optional<RayHit> &last_hit() {
  static std::optional<RayHit> h;
  return h;
}
inline void clear_providers() {
  providers() = Providers{};
  last_hit().reset();
}
inline void register_camera(std::function<CameraPose()> get,
                            std::function<void(const CameraPose &)> set) {
  providers().get_camera = std::move(get);
  providers().set_camera = std::move(set);
}
inline void register_player(std::function<Vec3()> get,
                            std::function<void(const Vec3 &)> set) {
  providers().get_player_pos = std::move(get);
  providers().set_player_pos = std::move(set);
}
inline void register_raycast(
    std::function<RayHit(Vec3, Vec3)> raycast) {
  providers().raycast = std::move(raycast);
}

inline bool near(Vec3 a, Vec3 b) {
  return std::fabs(a.x - b.x) <= 1e-3f && std::fabs(a.y - b.y) <= 1e-3f &&
         std::fabs(a.z - b.z) <= 1e-3f;
}
inline std::string str(Vec3 v) {
  return std::format("({:.3f},{:.3f},{:.3f})", v.x, v.y, v.z);
}
inline bool parse_vec(PendingE2ECommand &cmd, size_t off, const char *what,
                      Vec3 &out) {
  try {
    size_t used = 0;
    out.x = std::stof(cmd.args.at(off), &used);
    if (used != cmd.args[off].size()) throw std::invalid_argument("x");
    out.y = std::stof(cmd.args.at(off + 1), &used);
    if (used != cmd.args[off + 1].size()) throw std::invalid_argument("y");
    out.z = std::stof(cmd.args.at(off + 2), &used);
    if (used != cmd.args[off + 2].size()) throw std::invalid_argument("z");
    return true;
  } catch (...) {
    cmd.fail(std::format("{}: expected numeric x y z", what));
    return false;
  }
}

struct HandleSetCameraCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("set_camera")) return;
    if (!cmd.has_args(6)) {
      cmd.fail("set_camera requires: px py pz tx ty tz");
      return;
    }
    Vec3 pos, tgt;
    if (!parse_vec(cmd, 0, "set_camera", pos) ||
        !parse_vec(cmd, 3, "set_camera", tgt))
      return;
    if (!providers().set_camera) {
      cmd.fail("set_camera: no camera provider registered");
      return;
    }
    CameraPose pose = providers().get_camera ? providers().get_camera()
                                             : CameraPose{};
    pose.position = pos;
    pose.target = tgt;
    providers().set_camera(pose);
    cmd.consume();
  }
};

struct HandleExpectCameraCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_camera")) return;
    if (!cmd.has_args(6)) {
      cmd.fail("expect_camera requires: px py pz tx ty tz");
      return;
    }
    Vec3 pos, tgt;
    if (!parse_vec(cmd, 0, "expect_camera", pos) ||
        !parse_vec(cmd, 3, "expect_camera", tgt))
      return;
    if (!providers().get_camera) {
      cmd.fail("expect_camera: no camera provider registered");
      return;
    }
    CameraPose got = providers().get_camera();
    cmd.expect(near(got.position, pos) && near(got.target, tgt),
               std::format("expect_camera: camera is pos={} target={}, "
                           "expected pos={} target={}",
                           str(got.position), str(got.target), str(pos),
                           str(tgt)));
  }
};

struct HandleSetPlayerPosCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("set_player_pos")) return;
    if (!cmd.has_args(3)) {
      cmd.fail("set_player_pos requires: x y z");
      return;
    }
    Vec3 pos;
    if (!parse_vec(cmd, 0, "set_player_pos", pos)) return;
    if (!providers().set_player_pos) {
      cmd.fail("set_player_pos: no player provider registered");
      return;
    }
    providers().set_player_pos(pos);
    cmd.consume();
  }
};

struct HandleExpectPlayerPosCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_player_pos")) return;
    if (!cmd.has_args(3)) {
      cmd.fail("expect_player_pos requires: x y z");
      return;
    }
    Vec3 pos;
    if (!parse_vec(cmd, 0, "expect_player_pos", pos)) return;
    if (!providers().get_player_pos) {
      cmd.fail("expect_player_pos: no player provider registered");
      return;
    }
    Vec3 got = providers().get_player_pos();
    cmd.expect(near(got, pos),
               std::format("expect_player_pos: player is {}, expected {}",
                           str(got), str(pos)));
  }
};

inline bool run_raycast(PendingE2ECommand &cmd, const char *what) {
  if (!cmd.has_args(6)) {
    cmd.fail(std::format("{} requires: ox oy oz dx dy dz", what));
    return false;
  }
  Vec3 origin, dir;
  if (!parse_vec(cmd, 0, what, origin) || !parse_vec(cmd, 3, what, dir))
    return false;
  if (!providers().raycast) {
    cmd.fail(std::format("{}: no raycast provider registered", what));
    return false;
  }
  last_hit() = providers().raycast(origin, dir);
  return true;
}

struct HandleRaycastCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("raycast")) return;
    if (run_raycast(cmd, "raycast")) cmd.consume();
  }
};

struct HandleExpectHitCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_hit")) return;
    if (cmd.args.size() == 6) {
      if (!run_raycast(cmd, "expect_hit")) return;
    } else if (cmd.args.size() > 1) {
      cmd.fail("expect_hit requires: [name] or ox oy oz dx dy dz");
      return;
    } else if (!last_hit()) {
      cmd.fail("expect_hit: no raycast has run yet");
      return;
    }
    const RayHit &h = *last_hit();
    bool ok = h.hit && (cmd.args.empty() || cmd.args.size() == 6 ||
                        cmd.args[0] == h.name);
    cmd.expect(ok, h.hit ? std::format("expect_hit: hit '{}' at {}, expected '{}'",
                                       h.name, str(h.point),
                                       cmd.has_args(1) ? cmd.args[0] : h.name)
                         : std::string("expect_hit: ray missed"));
  }
};

struct HandleExpectNoHitCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_no_hit")) return;
    if (cmd.args.size() == 6) {
      if (!run_raycast(cmd, "expect_no_hit")) return;
    } else if (!cmd.args.empty()) {
      cmd.fail("expect_no_hit requires: [] or ox oy oz dx dy dz");
      return;
    } else if (!last_hit()) {
      cmd.fail("expect_no_hit: no raycast has run yet");
      return;
    }
    const RayHit &h = *last_hit();
    cmd.expect(!h.hit, std::format("expect_no_hit: hit '{}' at {}", h.name,
                                   str(h.point)));
  }
};

inline void register_three_d_commands(SystemManager &sm) {
  sm.register_update_system(std::make_unique<HandleSetCameraCommand>());
  sm.register_update_system(std::make_unique<HandleExpectCameraCommand>());
  sm.register_update_system(std::make_unique<HandleSetPlayerPosCommand>());
  sm.register_update_system(std::make_unique<HandleExpectPlayerPosCommand>());
  sm.register_update_system(std::make_unique<HandleRaycastCommand>());
  sm.register_update_system(std::make_unique<HandleExpectHitCommand>());
  sm.register_update_system(std::make_unique<HandleExpectNoHitCommand>());
}

}
}
}
