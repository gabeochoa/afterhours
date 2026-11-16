#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

using namespace afterhours;
using namespace afterhours::testing;

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

int main() { return ui_test::run_registered_tests("E2E register_command"); }
