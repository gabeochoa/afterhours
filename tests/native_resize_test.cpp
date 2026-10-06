// native_resize_test.cpp
// `native_resize w h` resizes the real window and holds the script until the
// backend reports that size and a UI render has followed; headless gets an
// explicit "unsupported" instead of a pass on a size nobody drew.
//
// No args: the headless check only, safe for `make test`.
// --window: also opens a hidden raylib (GLFW/Cocoa) window and drives the
// resize for real. Needs a window server, so it runs from
// `make native-window-test`, not from `make test`.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace afterhours;
using namespace afterhours::testing;

namespace {

bool g_window = false;

PendingE2ECommand resize_once(const std::vector<std::string> &args) {
  PendingE2ECommand cmd;
  cmd.name = "native_resize";
  cmd.args = args;
  Entity dummy;
  HandleNativeResizeCommand{}.for_each_with(dummy, cmd, 0.f);
  return cmd;
}

} // namespace

TEST(headless_reports_unsupported) {
  graphics::raylib_backend::ensure_registered();
  graphics::raylib_backend::storage() = graphics::RaylibHeadless{};
  CHECK(graphics::is_headless());
  auto cmd = resize_once({"640", "480"});
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("unsupported in headless mode") !=
        std::string::npos);
  graphics::raylib_backend::storage() = std::monostate{};
}

TEST(window_resize_waits_for_the_drawable) {
  if (!g_window) {
    printf("    (skipped: pass --window; needs a window server)\n");
    return;
  }
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(400, 300, "native_resize_test");
  CHECK(raylib::IsWindowReady());
  if (!raylib::IsWindowReady()) return;

  EntityCollection coll;
  EntityHelper::set_default_collection(&coll);
  reset_command_error_count();
  const auto script =
      std::filesystem::temp_directory_path() / "afh_native_resize.e2e";
  std::ofstream(script) << "native_resize 640 480 5\nmouse_move 1 1\n";
  E2ERunner runner;
  runner.load_script(script.string());

  HandleNativeResizeCommand resize;
  HandleMouseMoveCommand move;
  E2ECommandCleanupSystem cleanup;
  int renders_before_done = 0;
  bool resize_done = false, move_seen_before_done = false;
  const auto start = std::chrono::steady_clock::now();
  for (int frame = 0; frame < 600 && !runner.is_finished(); ++frame) {
    runner.tick(1.f / 60.f);
    coll.merge_entity_arrays();
    for (const auto &e : coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &cmd = e->get<PendingE2ECommand>();
      if (cmd.is("mouse_move") && !resize_done) move_seen_before_done = true;
      resize.for_each_with(*e, cmd, 0.f);
      if (cmd.is("native_resize") && cmd.is_consumed())
        resize_done = cmd.succeeded();
      move.for_each_with(*e, cmd, 0.f);
      cleanup.for_each_with(*e, cmd, 0.f);
    }
    coll.cleanup();
    // The UI render pass: draw, present, then the registry generation the
    // barrier counts.
    raylib::BeginDrawing();
    raylib::ClearBackground(raylib::DARKGRAY);
    raylib::EndDrawing();
    VisibleTextRegistry::instance().clear();
    if (!resize_done) ++renders_before_done;
  }
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  printf("    window %dx%d (render %dx%d), %d renders, %.1f ms\n",
         raylib::GetScreenWidth(), raylib::GetScreenHeight(),
         raylib::GetRenderWidth(), raylib::GetRenderHeight(),
         renders_before_done, ms);
  CHECK(resize_done);
  CHECK(runner.is_finished() && !runner.has_failed());
  CHECK(raylib::GetScreenWidth() == 640 && raylib::GetScreenHeight() == 480);
  CHECK(!move_seen_before_done);  // the script was held behind the resize
  std::filesystem::remove(script);
  EntityHelper::set_default_collection(nullptr);
  raylib::CloseWindow();
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (std::string(argv[i]) == "--window") g_window = true;
  return ui_test::run_registered_tests("native_resize");
}
