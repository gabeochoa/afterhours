// Opt-in: register_audio_commands(sm) plus sound_system::set_recording(true).
#pragma once

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "../../logging.h"
#include "../sound_system.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace audio_commands {

inline std::string known_buses_string() {
    std::string out;
    for (const auto &bus : sound_system::known_buses()) {
        if (!out.empty()) out += ", ";
        out += bus;
    }
    return out;
}

struct HandleExpectSoundPlayedCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_sound_played")) return;
        if (!cmd.has_args(1)) {
            cmd.fail("expect_sound_played requires: <sound-name>");
            return;
        }
        const std::string name = cmd.arg(0);
        if (!sound_system::expect_sound_played(name)) {
            cmd.fail("expect_sound_played: '" + name + "' was not played");
            return;
        }
        log_info("expect_sound_played: '{}' played {} time(s) (PASS)",
                 name.c_str(), sound_system::play_count(name));
        cmd.consume();
    }
};

struct HandleExpectNoSoundPlayedCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_no_sound_played")) return;
        if (!cmd.has_args(1)) {
            cmd.fail("expect_no_sound_played requires: <sound-name>");
            return;
        }
        const std::string name = cmd.arg(0);
        if (sound_system::expect_sound_played(name)) {
            cmd.fail("expect_no_sound_played: '" + name + "' was played");
            return;
        }
        cmd.consume();
    }
};

struct HandleClearSoundLogCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("clear_sound_log")) return;
        sound_system::clear_play_log();
        cmd.consume();
    }
};

struct HandleSetVolumeCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("set_volume")) return;
        if (!cmd.has_args(2)) {
            cmd.fail("set_volume requires: <bus|master> <0..1>");
            return;
        }
        const std::string target = cmd.arg(0);
        const auto volume = cmd.maybe_arg_as<float>(1);
        if (!volume) {
            cmd.fail(std::format("set_volume: volume must be a number 0..1, "
                                 "got '{}'",
                                 cmd.arg(1)));
            return;
        }
        const float clamped = std::clamp(*volume, 0.f, 1.f);
        if (target == "master") {
            sound_system::set_master_volume(clamped);
            cmd.consume();
            return;
        }
        if (!sound_system::has_bus(target)) {
            cmd.fail(std::format("set_volume: unknown bus '{}' (known: {})",
                                 target, known_buses_string()));
            return;
        }
        sound_system::set_bus_volume(target, clamped);
        cmd.consume();
    }
};

struct HandleExpectVolumeCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_volume")) return;
        if (!cmd.has_args(2)) {
            cmd.fail("expect_volume requires: <bus|master> <min> [max]");
            return;
        }
        const std::string target = cmd.arg(0);
        const auto min_volume = cmd.maybe_arg_as<float>(1);
        if (!min_volume) {
            cmd.fail(std::format("expect_volume: min must be a number, got '{}'",
                                 cmd.arg(1)));
            return;
        }
        float max_volume = *min_volume;
        if (cmd.args.size() >= 3) {
            const auto parsed = cmd.maybe_arg_as<float>(2);
            if (!parsed) {
                cmd.fail(std::format(
                    "expect_volume: max must be a number, got '{}'", cmd.arg(2)));
                return;
            }
            max_volume = *parsed;
        }
        if (max_volume < *min_volume) {
            cmd.fail(std::format("expect_volume: min {} is greater than max {}",
                                 *min_volume, max_volume));
            return;
        }
        float got = 0.f;
        if (target == "master") {
            got = sound_system::get_master_volume();
        } else {
            if (!sound_system::has_bus(target)) {
                cmd.fail(std::format("expect_volume: unknown bus '{}' (known: {})",
                                     target, known_buses_string()));
                return;
            }
            got = sound_system::get_bus_volume(target);
        }
        if (got < *min_volume || got > max_volume) {
            cmd.fail(std::format("expect_volume: '{}' volume is {}, expected {}..{}",
                                 target, got, *min_volume, max_volume));
            return;
        }
        cmd.consume();
    }
};

inline void register_audio_commands(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<HandleExpectSoundPlayedCommand>());
    sm.register_update_system(
        std::make_unique<HandleExpectNoSoundPlayedCommand>());
    sm.register_update_system(std::make_unique<HandleClearSoundLogCommand>());
    sm.register_update_system(std::make_unique<HandleSetVolumeCommand>());
    sm.register_update_system(std::make_unique<HandleExpectVolumeCommand>());
}

}  // namespace audio_commands
}  // namespace testing
}  // namespace afterhours
