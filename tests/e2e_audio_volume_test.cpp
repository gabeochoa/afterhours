#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/audio_commands.h>

using namespace afterhours;
using namespace afterhours::testing;
using namespace afterhours::testing::audio_commands;

struct VolumeFixture {
  EntityCollection collection;
  Entity &entity = collection.createEntity();

  VolumeFixture() {
    EntityHelper::set_default_collection(&collection);
    collection.merge_entity_arrays();
    sound_system::set_master_volume(1.f);
    sound_system::set_bus_volume("effects", 1.f);
    sound_system::set_bus_volume("music", 1.f);
  }
  ~VolumeFixture() {
    sound_system::set_master_volume(1.f);
    sound_system::set_bus_volume("effects", 1.f);
    sound_system::set_bus_volume("music", 1.f);
    EntityHelper::set_default_collection(nullptr);
  }
  PendingE2ECommand run(const std::string &name,
                        std::vector<std::string> args) {
    PendingE2ECommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    HandleSetVolumeCommand{}.for_each_with(entity, cmd, 0.f);
    HandleExpectVolumeCommand{}.for_each_with(entity, cmd, 0.f);
    return cmd;
  }
};

TEST(set_then_expect_bus_round_trip) {
  VolumeFixture f;
  CHECK(sound_system::SoundLibrary::get().add("beep", SoundType{}, "ambience"));
  auto set = f.run("set_volume", {"ambience", "0.5"});
  CHECK(set.is_consumed());
  CHECK(set.succeeded());
  auto expect = f.run("expect_volume", {"ambience", "0.5"});
  CHECK(expect.is_consumed());
  CHECK(expect.succeeded());
  auto ranged = f.run("expect_volume", {"ambience", "0.4", "0.6"});
  CHECK(ranged.is_consumed());
  CHECK(ranged.succeeded());
  sound_system::set_bus_volume("ambience", 1.f);
  sound_system::SoundLibrary::get().unload_all();
}

TEST(set_then_expect_master_round_trip) {
  VolumeFixture f;
  auto set = f.run("set_volume", {"master", "0.25"});
  CHECK(set.is_consumed());
  CHECK(set.succeeded());
  CHECK(sound_system::get_master_volume() == 0.25f);
  auto expect = f.run("expect_volume", {"master", "0.25"});
  CHECK(expect.is_consumed());
  CHECK(expect.succeeded());
}

TEST(unknown_bus_fails_clean) {
  VolumeFixture f;
  auto set = f.run("set_volume", {"no_such_bus", "0.5"});
  CHECK(set.is_consumed());
  CHECK(!set.succeeded());
  CHECK(set.error_message.find("unknown bus") != std::string::npos);
  auto expect = f.run("expect_volume", {"no_such_bus", "0", "1"});
  CHECK(expect.is_consumed());
  CHECK(!expect.succeeded());
  CHECK(expect.error_message.find("unknown bus") != std::string::npos);
}

TEST(out_of_range_values_clamp) {
  VolumeFixture f;
  auto high = f.run("set_volume", {"effects", "1.5"});
  CHECK(high.is_consumed());
  CHECK(high.succeeded());
  CHECK(sound_system::get_bus_volume("effects") == 1.f);
  auto low = f.run("set_volume", {"master", "-0.5"});
  CHECK(low.is_consumed());
  CHECK(low.succeeded());
  CHECK(sound_system::get_master_volume() == 0.f);
}

TEST(expect_range_mismatch_and_malformed_args_fail) {
  VolumeFixture f;
  f.run("set_volume", {"music", "0.5"});
  auto mismatch = f.run("expect_volume", {"music", "0.9"});
  CHECK(mismatch.is_consumed());
  CHECK(!mismatch.succeeded());
  auto bad_min = f.run("expect_volume", {"music", "loud"});
  CHECK(bad_min.is_consumed());
  CHECK(!bad_min.succeeded());
  auto bad_max = f.run("expect_volume", {"music", "0.4", "loud"});
  CHECK(bad_max.is_consumed());
  CHECK(!bad_max.succeeded());
  auto bad_set = f.run("set_volume", {"music", "loud"});
  CHECK(bad_set.is_consumed());
  CHECK(!bad_set.succeeded());
  auto inverted = f.run("expect_volume", {"music", "0.8", "0.2"});
  CHECK(inverted.is_consumed());
  CHECK(!inverted.succeeded());
  auto short_set = f.run("set_volume", {"music"});
  CHECK(short_set.is_consumed());
  CHECK(!short_set.succeeded());
  auto short_expect = f.run("expect_volume", {"music"});
  CHECK(short_expect.is_consumed());
  CHECK(!short_expect.succeeded());
}

int main() { return ui_test::run_registered_tests("E2E audio volume"); }
