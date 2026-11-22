#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/settings_table.h>

using namespace afterhours;
using namespace afterhours::testing;

struct GameSettings {
  bool fullscreen = false;
  int width = 1280;
  float volume = 0.5f;
  std::string player_name = "player";
};

static const auto kFields = settings_fields<GameSettings>(
    setting_field("fullscreen", &GameSettings::fullscreen),
    setting_field("width", &GameSettings::width),
    setting_field("volume", &GameSettings::volume),
    setting_field("player_name", &GameSettings::player_name));

TEST(json_round_trip) {
  GameSettings in{true, 1920, 0.25f, "gabe"};
  json j = settings_to_json(in, kFields);
  CHECK(j["fullscreen"] == true);
  CHECK(j["width"] == 1920);
  GameSettings out;
  std::string err;
  CHECK(settings_from_json(out, kFields, j, &err));
  CHECK(out.fullscreen && out.width == 1920 && out.volume == 0.25f);
  CHECK(out.player_name == "gabe");
}

TEST(unknown_ignored_missing_kept_and_merge_preserves_unknown) {
  json j = json::parse(
      R"({"width": 800, "future_option": {"x": 1}})");
  GameSettings out{true, 1280, 0.75f, "keep"};
  CHECK(settings_from_json(out, kFields, j));
  CHECK(out.width == 800 && out.fullscreen && out.volume == 0.75f);
  CHECK(out.player_name == "keep");
  j.update(settings_to_json(out, kFields));
  CHECK(j["future_option"]["x"] == 1);
}

TEST(type_mismatch_is_typed_error_and_changes_nothing) {
  GameSettings out{false, 1280, 0.5f, "player"};
  std::string err;
  json j = json::parse(R"({"width": "wide", "fullscreen": true})");
  CHECK(!settings_from_json(out, kFields, j, &err));
  CHECK(err == "setting 'width': expected integer, got string");
  CHECK(!out.fullscreen && out.width == 1280);
  CHECK(!settings_from_json(out, kFields, json::parse("[1]"), &err));
  CHECK(err == "settings: expected object, got array");
}

struct CommandFixture {
  EntityCollection collection;
  SystemManager systems;
  GameSettings settings;

  CommandFixture() {
    EntityHelper::set_default_collection(&collection);
    register_setting_commands(systems, kFields, settings);
  }
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

TEST(set_and_expect_setting_scripts) {
  CommandFixture f;
  auto &set = f.run("set_setting", {"volume", "0.25"});
  CHECK(set.is_consumed() && set.succeeded());
  CHECK(f.settings.volume == 0.25f);
  auto &hit = f.run("expect_setting", {"volume", "0.250"});
  CHECK(hit.is_consumed() && hit.succeeded());
  auto &name = f.run("set_setting", {"player_name", "ada", "lovelace"});
  CHECK(name.succeeded() && f.settings.player_name == "ada lovelace");
  auto &miss = f.run("expect_setting", {"width", "999"});
  CHECK(miss.is_retry());
  CHECK(miss.last_mismatch == "setting 'width' is 1280, expected 999");
}

TEST(set_setting_rejects_unknown_and_bad_values) {
  CommandFixture f;
  auto &unknown = f.run("set_setting", {"nope", "1"});
  CHECK(unknown.is_consumed() && !unknown.succeeded());
  CHECK(unknown.error_message == "set_setting: unknown setting 'nope'");
  auto &bad = f.run("set_setting", {"width", "wide"});
  CHECK(!bad.succeeded());
  CHECK(bad.error_message == "setting 'width': cannot parse 'wide'");
  CHECK(f.settings.width == 1280);
  auto &few = f.run("expect_setting", {"width"});
  CHECK(few.error_message == "expect_setting requires: <name> <value>");
}

int main() { return ui_test::run_registered_tests("settings_table_test"); }
