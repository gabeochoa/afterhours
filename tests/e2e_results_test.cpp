#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/harness.h>

#include <chrono>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

struct ScriptDir {
  std::filesystem::path directory = std::filesystem::temp_directory_path() /
      ("afterhours-results-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  ScriptDir() { std::filesystem::create_directory(directory); }
  ~ScriptDir() { std::filesystem::remove_all(directory); }
  std::string add(const std::string &name, const std::string &text) {
    auto path = directory / (name + ".e2e");
    std::ofstream(path) << text;
    return path.string();
  }
  std::string read(const std::string &name) {
    std::ifstream in(directory / name);
    return {std::istreambuf_iterator<char>(in), {}};
  }
};

static void run(E2ERunner &runner) {
  auto &coll = EntityHelper::get_default_collection();
  E2ECommandCleanupSystem cleanup;
  for (int frame = 0; frame < 200 && !runner.is_finished(); ++frame) {
    runner.tick(0.1f);
    coll.merge_entity_arrays();
    for (auto &e : coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &command = e->get<PendingE2ECommand>();
      if (command.is("ok")) command.consume();
      if (command.is("broken")) command.fail("broken on purpose");
      cleanup.for_each_with(*e, command, 0.f);
    }
    coll.cleanup();
  }
}

struct Captured {
  static inline std::string text;
  decltype(log_sink_fn) previous = log_sink_fn;
  Captured() {
    text.clear();
    log_sink_fn = [](const char *, const char *message) {
      text += message;
      text += '\n';
    };
  }
  ~Captured() { log_sink_fn = previous; }
};

TEST(single_script_prints_its_verdict_and_writes_json) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  E2ERunner runner;
  runner.load_script(dir.add("passes", "ok\nok\n"));
  runner.set_results_path((dir.directory / "results.json").string());
  run(runner);
  Captured log;
  runner.print_results();
  CHECK(!runner.has_failed());
  CHECK(log.text.find("[PASS] passes") != std::string::npos);
  const auto results = runner.results();
  CHECK(results.size() == 1 && results[0].passed && results[0].name == "passes");
  const std::string json = dir.read("results.json");
  CHECK(json.find("\"name\": \"passes\"") != std::string::npos);
  CHECK(json.find("\"success\": true") != std::string::npos);
  CHECK(json.find("\"complete\": true") != std::string::npos);
  CHECK(!std::filesystem::exists(dir.directory / "results.json.tmp"));
}

TEST(single_failing_script_says_fail) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  E2ERunner runner;
  runner.load_script(dir.add("breaks", "ok\nbroken\n"));
  run(runner);
  Captured log;
  runner.print_results();
  CHECK(runner.has_failed());
  CHECK(log.text.find("[FAIL] breaks (1 error)") != std::string::npos);
  CHECK(runner.results().size() == 1 && !runner.results()[0].passed);
}

TEST(batch_json_counts_every_script) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  dir.add("a_good", "ok\n");
  dir.add("b_bad", "broken\n");
  E2ERunner runner;
  runner.load_scripts_from_directory(dir.directory.string());
  run(runner);
  CHECK(runner.write_results_json((dir.directory / "out.json").string()));
  const std::string json = dir.read("out.json");
  CHECK(json.find("\"run\": 2") != std::string::npos);
  CHECK(json.find("\"passed\": 1,") != std::string::npos);
  CHECK(json.find("\"failed\": 1") != std::string::npos);
  CHECK(json.find("\"success\": false") != std::string::npos);
  CHECK(json.find("\"name\": \"b_bad\", \"path\"") != std::string::npos);
}

TEST(empty_discovery_and_missing_script_fail) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  E2ERunner empty;
  empty.load_scripts_from_directory(dir.directory.string());
  CHECK(empty.is_finished());
  CHECK(empty.has_failed());

  E2ERunner missing;
  missing.load_script((dir.directory / "nope.e2e").string());
  CHECK(missing.is_finished());
  CHECK(missing.has_failed());
}

TEST(results_json_flag_reaches_the_runner) {
  char a0[] = "game", a1[] = "--results-json", a2[] = "/tmp/r.json";
  char *argv[] = {a0, a1, a2};
  CHECK(parse_e2e_args(3, argv).results_json == "/tmp/r.json");
}

int main() { return ui_test::run_registered_tests("E2E results"); }
