#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

static size_t count_of(const std::string &haystack, const std::string &needle) {
  size_t count = 0;
  for (size_t pos = haystack.find(needle); pos != std::string::npos;
       pos = haystack.find(needle, pos + 1))
    ++count;
  return count;
}

struct CommandFixture {
  EntityCollection collection;
  SystemManager systems;

  CommandFixture() { EntityHelper::set_default_collection(&collection); }
  ~CommandFixture() { EntityHelper::set_default_collection(nullptr); }

  PendingE2ECommand &run(std::string name, std::vector<std::string> args) {
    Entity &entity = collection.createEntity();
    auto &cmd = entity.addComponent<PendingE2ECommand>();
    cmd.name = std::move(name);
    cmd.args = std::move(args);
    collection.merge_entity_arrays();
    systems.run(1.f / 60.f);
    return cmd;
  }
};

TEST(lambda_sees_only_its_own_unconsumed_command) {
  CommandFixture f;
  int calls = 0;
  register_command(f.systems, "serve_now", [&](PendingE2ECommand &cmd) {
    ++calls;
    cmd.consume();
  });
  register_command(f.systems, "serve_now", [&](PendingE2ECommand &) { ++calls; });
  CHECK(f.run("serve_now", {}).is_consumed());
  CHECK(calls == 1);
  CHECK(!f.run("other", {}).is_consumed());
  CHECK(calls == 1);
}

TEST(typed_arguments_arrive_parsed) {
  CommandFixture f;
  std::pair<int, float> score;
  std::string label;
  register_command<int, float, std::string>(
      f.systems, "set_score",
      [&](PendingE2ECommand &cmd, int left, float right, std::string text) {
        score = {left, right};
        label = text;
        cmd.consume();
      });
  auto &cmd = f.run("set_score", {"3", "4.5", "deuce"});
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK(score.first == 3 && score.second == 4.5f);
  CHECK(label == "deuce");
}

TEST(wrong_arity_fails_without_calling) {
  CommandFixture f;
  bool called = false;
  register_command<int, int>(f.systems, "set_score",
                             [&](PendingE2ECommand &, int, int) { called = true; });
  auto &few = f.run("set_score", {"3"});
  CHECK(few.is_consumed() && !few.succeeded());
  CHECK(few.error_message == "set_score takes 2 arguments, got 1");
  auto &many = f.run("set_score", {"3", "4", "5"});
  CHECK(many.error_message == "set_score takes 2 arguments, got 3");
  CHECK(!called);
}

TEST(unparseable_argument_names_itself) {
  CommandFixture f;
  bool called = false;
  register_command<int, int>(f.systems, "set_score",
                             [&](PendingE2ECommand &, int, int) { called = true; });
  auto &cmd = f.run("set_score", {"3", "abc"});
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message == "set_score: cannot parse argument 2 'abc'");
  CHECK(!called);
}

TEST(expect_consumes_or_retries) {
  CommandFixture f;
  int score = 0;
  register_command<int>(f.systems, "expect_score",
                        [&](PendingE2ECommand &cmd, int want) {
                          cmd.expect(score == want,
                                     std::format("score is {}", score));
                        });
  auto &miss = f.run("expect_score", {"2"});
  CHECK(miss.is_retry());
  CHECK(miss.last_mismatch == "score is 0");
  score = 2;
  auto &hit = f.run("expect_score", {"2"});
  CHECK(hit.is_consumed() && hit.succeeded());
}

TEST(a_builtin_taking_the_name_warns_once) {
  static std::string captured;
  captured.clear();
  const auto previous = log_sink_fn;
  log_sink_fn = [](const char *, const char *message) { captured += message; };
  CommandFixture f;
  bool ours_ran = false;
  register_builtin_handlers(f.systems);
  register_command(f.systems, "disable_animations",
                   [&](PendingE2ECommand &cmd) {
                     ours_ran = true;
                     cmd.consume();
                   });
  CHECK(f.run("disable_animations", {}).is_consumed());
  f.run("disable_animations", {});
  CHECK(!ours_ran);
  CHECK(count_of(captured, "'disable_animations' was consumed") == 1);

  CommandFixture first;
  captured.clear();
  register_command(first.systems, "disable_animations",
                   [&](PendingE2ECommand &cmd) {
                     ours_ran = true;
                     cmd.consume();
                   });
  register_builtin_handlers(first.systems);
  first.run("disable_animations", {});
  CHECK(ours_ran);
  CHECK(captured.find("was consumed") == std::string::npos);
  log_sink_fn = previous;
  animation::set_instant(false);
}

TEST(first_registered_handler_sees_a_command_on_its_dispatch_frame) {
  const auto path = std::filesystem::temp_directory_path() /
      ("afterhours-first-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".e2e");
  std::ofstream(path) << "disable_animations\n";
  CommandFixture f;
  bool ours_ran = false;
  register_command(f.systems, "disable_animations",
                   [&](PendingE2ECommand &cmd) {
                     ours_ran = true;
                     cmd.consume();
                   });
  register_builtin_handlers(f.systems);
  register_cleanup(f.systems);
  E2ERunner runner;
  runner.load_script(path.string());
  runner.tick(1.f / 60.f);
  f.systems.run(1.f / 60.f);
  CHECK(ours_ran);
  CHECK(!animation::is_instant());
  std::filesystem::remove(path);
}

int main() { return ui_test::run_registered_tests("E2E register_command"); }
