#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) && \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif
#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/e2e_testing/three_d_commands.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;
namespace td = afterhours::testing::three_d_commands;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}
static Entity dummy_entity;
template <typename H>
static PendingE2ECommand run(const char *name, std::vector<std::string> args) {
  PendingE2ECommand cmd;
  cmd.name = name;
  cmd.args = std::move(args);
  H{}.for_each_with(dummy_entity, cmd, 0.f);
  return cmd;
}

int main() {
  td::clear_providers();
  td::CameraPose cam{{0, 2, 6}, {0, 0, 0}, {0, 1, 0}, 45.f};
  td::Vec3 player{0, 0, 0};
  td::register_camera([&] { return cam; },
                      [&](const td::CameraPose &p) { cam = p; });
  td::register_player([&] { return player; },
                      [&](const td::Vec3 &p) { player = p; });
  td::register_raycast([](td::Vec3 o, td::Vec3 d) {
    td::RayHit h;
    if (d.x > 0.f && o.x < 5.f) {
      float t = (5.f - o.x) / d.x;
      h = {true, {5.f, o.y + d.y * t, o.z + d.z * t}, {-1, 0, 0}, t, "wall"};
    }
    return h;
  });

  auto set_cam = run<td::HandleSetCameraCommand>(
      "set_camera", {"1", "2", "3", "4", "5", "6"});
  check(set_cam.succeeded(), "set_camera ok");
  check(cam.position == td::Vec3{1, 2, 3} && cam.target == td::Vec3{4, 5, 6},
        "set_camera wrote pose");
  check(run<td::HandleExpectCameraCommand>("expect_camera",
                                           {"1", "2", "3", "4", "5", "6"})
            .succeeded(),
        "expect_camera round-trip passes");
  {
    auto c = run<td::HandleExpectCameraCommand>("expect_camera",
                                                {"9", "2", "3", "4", "5", "6"});
    check(c.is_retry() && c.last_mismatch.find("(1.000") != std::string::npos,
          "expect_camera mismatch retries naming actual");
  }

  check(run<td::HandleSetPlayerPosCommand>("set_player_pos", {"7", "1", "-2"})
            .succeeded(),
        "set_player_pos ok");
  check(player == td::Vec3{7, 1, -2}, "player teleported");
  check(run<td::HandleExpectPlayerPosCommand>("expect_player_pos",
                                              {"7", "1", "-2"})
            .succeeded(),
        "expect_player_pos passes");
  {
    auto c = run<td::HandleExpectPlayerPosCommand>("expect_player_pos",
                                                   {"0", "0", "0"});
    check(c.is_retry(), "expect_player_pos mismatch retries");
  }

  check(run<td::HandleRaycastCommand>("raycast", {"0", "0", "0", "1", "0", "0"})
            .succeeded(),
        "raycast ok");
  check(run<td::HandleExpectHitCommand>("expect_hit", {}).succeeded(),
        "expect_hit after raycast passes");
  check(run<td::HandleExpectHitCommand>("expect_hit", {"wall"}).succeeded(),
        "expect_hit with name passes");
  check(run<td::HandleExpectHitCommand>("expect_hit",
                                        {"0", "0", "0", "1", "0", "0"})
            .succeeded(),
        "expect_hit inline ray passes");
  {
    auto c = run<td::HandleExpectHitCommand>("expect_hit", {"door"});
    check(c.is_retry() && c.last_mismatch.find("wall") != std::string::npos,
          "expect_hit wrong name retries naming actual");
  }
  check(run<td::HandleExpectNoHitCommand>("expect_no_hit",
                                          {"0", "0", "0", "-1", "0", "0"})
            .succeeded(),
        "expect_no_hit inline miss passes");
  {
    auto c = run<td::HandleExpectHitCommand>("expect_hit",
                                             {"0", "0", "0", "-1", "0", "0"});
    check(c.is_retry(), "expect_hit on miss retries");
  }

  td::clear_providers();
  {
    auto c = run<td::HandleSetCameraCommand>("set_camera",
                                             {"1", "2", "3", "4", "5", "6"});
    check(!c.succeeded() &&
              c.error_message.find("no camera provider") != std::string::npos,
          "set_camera without provider fails by name");
    auto r = run<td::HandleRaycastCommand>("raycast",
                                           {"0", "0", "0", "1", "0", "0"});
    check(!r.succeeded() &&
              r.error_message.find("no raycast provider") != std::string::npos,
          "raycast without provider fails by name");
    auto m = run<td::HandleSetPlayerPosCommand>("set_player_pos", {"x", "0", "0"});
    check(!m.succeeded() && m.error_message.find("numeric") != std::string::npos,
          "set_player_pos non-numeric fails");
  }

  {
    const char *path = "/tmp/afterhours_three_d_commands_test.e2e";
    FILE *f = fopen(path, "w");
    fputs("set_camera 1 2 3 4 5 6\nexpect_camera 1 2 3 4 5 6\n"
          "set_player_pos 7 1 -2\nraycast 0 0 0 1 0 0\nexpect_hit wall\n",
          f);
    fclose(f);
    auto cmds = testing::parse_script(path);
    check(cmds.size() == 5, "3d script parses five commands");
    if (cmds.size() == 5)
      check(cmds[0].args.size() == 6 && cmds[4].args.size() == 1,
            "3d script args split as expected");
    remove(path);
  }

  td::clear_providers();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) return 1;
  printf("All checks passed!\n");
  return 0;
}
