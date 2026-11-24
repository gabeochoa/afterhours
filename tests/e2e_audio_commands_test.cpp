#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) && \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif

#include <cstdio>
#include <string>

#include <afterhours/src/plugins/e2e_testing/audio_commands.h>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;
namespace audio = afterhours::testing::audio_commands;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("  FAIL: %s\n", what);
    }
}

static void seed(const char *name, float volume, float pan = 0.5f) {
    sound_system::record_play(
        sound_system::PlayRecord{name, "effects", volume, pan, 1.f, std::nullopt});
}

template<typename Handler>
static PendingE2ECommand run(const char *cmd_name,
                             std::initializer_list<std::string> args) {
    EntityCollection coll;
    Entity &entity = coll.createEntity();
    PendingE2ECommand cmd;
    cmd.name = cmd_name;
    cmd.args.assign(args.begin(), args.end());
    Handler{}.for_each_with(entity, cmd, 0.f);
    return cmd;
}

static PendingE2ECommand run_level(std::initializer_list<std::string> args) {
    return run<audio::HandleExpectSoundLevelCommand>("expect_sound_level",
                                                     args);
}

static PendingE2ECommand run_pan(std::initializer_list<std::string> args) {
    return run<audio::HandleExpectSoundPanCommand>("expect_sound_pan", args);
}

int main() {
    sound_system::set_recording(true);

    sound_system::clear_play_log();
    seed("blip", 0.4f, 0.5f);
    check(run_level({"blip", "0.3", "0.5"}).succeeded(),
          "in-range level consumes");
    check(run_pan({"blip", "0.4", "0.6"}).succeeded(), "in-range pan consumes");

    sound_system::clear_play_log();
    seed("blip", 0.4f, 0.5f);
    auto low = run_level({"blip", "0.5", "0.9"});
    check(low.is_retry(), "below-min level retries");
    check(low.last_mismatch.find("0.400") != std::string::npos,
          "level retry reports the measured volume");
    auto high = run_level({"blip", "0.1", "0.2"});
    check(high.is_retry(), "above-max level retries");
    auto off_pan = run_pan({"blip", "0.7", "0.9"});
    check(off_pan.is_retry(), "out-of-range pan retries");
    check(off_pan.last_mismatch.find("0.500") != std::string::npos,
          "pan retry reports the measured pan");

    sound_system::clear_play_log();
    auto silent = run_level({"blip", "0.0", "1.0"});
    check(silent.is_retry(), "level with no play retries");
    check(silent.last_mismatch.find("was not played") != std::string::npos,
          "level no-play retry names the sound");
    auto silent_pan = run_pan({"blip", "0.0", "1.0"});
    check(silent_pan.is_retry(), "pan with no play retries");

    sound_system::clear_play_log();
    seed("blip", 0.1f, 0.2f);
    seed("blip", 0.8f, 0.9f);
    check(run_level({"blip", "0.7", "0.9"}).succeeded(),
          "level reads the last play");
    check(run_pan({"blip", "0.8", "1.0"}).succeeded(),
          "pan reads the last play");
    check(run_level({"blip", "0.0", "0.2"}).is_retry(),
          "stale first play does not satisfy level");

    sound_system::clear_play_log();
    seed("blip", 0.4f, 0.5f);
    check(run_level({"blip", "0.3"}).succeeded(),
          "level without max passes above min");
    check(run_level({"blip", "0.5"}).is_retry(),
          "level without max retries below min");
    check(run_pan({"blip", "0.4"}).succeeded(),
          "pan without max passes above min");

    {
        EntityCollection coll;
        Entity &entity = coll.createEntity();
        PendingE2ECommand cmd;
        cmd.name = "expect_sound_level";
        cmd.args = {"blip", "0.9", "1.0"};
        audio::HandleExpectSoundLevelCommand handler;
        handler.for_each_with(entity, cmd, 0.f);
        check(cmd.is_retry(), "late play starts as retry");
        cmd.reset_retry();
        seed("blip", 0.95f);
        handler.for_each_with(entity, cmd, 0.f);
        check(cmd.is_consumed() && cmd.succeeded(),
              "level passes once the play lands");
    }

    check(!run_level({"blip"}).succeeded(), "level without range fails");
    check(run_level({"blip"}).error_message.find("requires") !=
              std::string::npos,
          "level without range names usage");
    check(!run_level({"blip", "loud", "1.0"}).succeeded(),
          "level with bad min fails immediately");
    check(!run_level({"blip", "0.1", "loud"}).succeeded(),
          "level with bad max fails immediately");
    check(!run_level({"blip", "0.8", "0.2"}).succeeded(),
          "level with min above max fails immediately");
    check(!run_pan({"blip", "0.0", "1.5"}).succeeded(),
          "pan with max above 1 fails immediately");
    check(!run_pan({"blip", "-0.5", "0.5"}).succeeded(),
          "pan with negative min fails immediately");
    check(!run_level({"blip", "0.1", "0.2", "extra"}).succeeded(),
          "level with extra args fails");

    {
        SystemManager sm;
        audio::register_audio_commands(sm);
        sound_system::clear_play_log();
        seed("blip", 0.4f, 0.5f);
        EntityCollection coll;
        EntityHelper::set_default_collection(&coll);
        Entity &level_entity = coll.createEntity();
        auto &level_cmd = level_entity.addComponent<PendingE2ECommand>();
        level_cmd.name = "expect_sound_level";
        level_cmd.args = {"blip", "0.3", "0.5"};
        Entity &pan_entity = coll.createEntity();
        auto &pan_cmd = pan_entity.addComponent<PendingE2ECommand>();
        pan_cmd.name = "expect_sound_pan";
        pan_cmd.args = {"blip", "0.4", "0.6"};
        coll.merge_entity_arrays();
        sm.tick(EntityHelper::get_entities_for_mod(), 0.f);
        check(level_cmd.is_consumed() && level_cmd.succeeded(),
              "registered level handler consumes through SystemManager");
        check(pan_cmd.is_consumed() && pan_cmd.succeeded(),
              "registered pan handler consumes through SystemManager");
        EntityHelper::set_default_collection(nullptr);
    }

    sound_system::clear_play_log();
    sound_system::set_recording(false);

    if (failures == 0) std::printf("All %d checks passed\n", checks);
    return failures == 0 ? 0 : 1;
}
