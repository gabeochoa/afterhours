// E2EApp: the create() gate, custom handlers running, and `screenshot`
// queueing a name for post_render.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/app.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace afterhours;
using namespace afterhours::testing;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static int lambda_arg = 0;

struct MyCommand : System<PendingE2ECommand> {
  inline static int seen = 0;
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("my_cmd"))
      return;
    seen++;
    cmd.consume();
  }
};

int main() {
  printf("=== E2EApp ===\n\n");
  EntityCollection collection;
  EntityHelper::set_default_collection(&collection);

  {
    char a0[] = "game";
    char *argv[] = {a0};
    check(E2EApp::create(1, argv) == nullptr, "no --e2e, no app");
  }

  {
    char a0[] = "game", a1[] = "--test-script", a2[] = "/nonexistent.e2e";
    char *argv[] = {a0, a1, a2};
    check(E2EApp::create(3, argv) == nullptr,
          "--e2e with no scripts is nullptr, so the game can exit 1");
  }

  const auto script =
      std::filesystem::temp_directory_path() / "e2e_app_test.e2e";
  {
    std::ofstream(script) << "my_cmd\nlambda_cmd 7\nscreenshot shot_a\n";
    std::string path = script.string();
    char a0[] = "game", a1[] = "--test-script";
    char *argv[] = {a0, a1, path.data()};
    const std::uint32_t before = RandomEngine::get().get_seed();
    auto app = E2EApp::create(
        3, argv,
        [](SystemManager &sm) {
          sm.register_update_system(std::make_unique<MyCommand>());
          register_command(sm, "lambda_cmd", [](PendingE2ECommand &cmd) {
            lambda_arg = cmd.maybe_arg_as<int>(0).value_or(0);
            cmd.consume();
          });
        },
        "shots");
    check(app != nullptr, "a script makes an app");
    check(app && app->default_screenshot_dir == "shots",
          "default screenshot dir is kept");
    check(app && RandomEngine::get().get_seed() == before,
          "create() leaves RandomEngine random");
    if (app)
      app->set_seed(42);
    check(RandomEngine::get().get_seed() == 42u, "set_seed() seeds it");
    bool queued = false;
    for (int i = 0; app && i < 20 && !app->finished(); i++) {
      app->tick(1.f / 60.f);
      queued = queued || app->pending_screenshot == "shot_a";
    }
    check(MyCommand::seen == 1, "custom handler consumed its command");
    check(lambda_arg == 7, "register_command lambda ran with its args");
    check(queued, "screenshot queues its name for post_render");
    check(app && app->finished(), "runner finishes");
    check(app && app->finish() == 0, "a passing run exits 0");
  }
  std::filesystem::remove(script);
  EntityHelper::set_default_collection(nullptr);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
