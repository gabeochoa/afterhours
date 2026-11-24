// Opt-in: register_audio_commands(sm) plus sound_system::set_recording(true).
// set_volume/expect_volume additionally need AFTER_HOURS_ENABLE_E2E_TESTING;
// without it they (and sound_system::known_buses/has_bus) compile out.
#pragma once

#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
#include <algorithm>
#include <vector>
#endif

#include "../../logging.h"
#include "../sound_system.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace audio_commands {

inline std::optional<float> last_played_volume(const std::string &name) {
    const auto &log = sound_system::play_log();
    for (auto it = log.rbegin(); it != log.rend(); ++it) {
        if (it->name == name) return it->volume;
    }
    return std::nullopt;
}

inline std::optional<float> last_played_pan(const std::string &name) {
    const auto &log = sound_system::play_log();
    for (auto it = log.rbegin(); it != log.rend(); ++it) {
        if (it->name == name) return it->pan;
    }
    return std::nullopt;
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

#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
inline std::string known_buses_string() {
    std::string out;
    for (const auto &bus : sound_system::known_buses()) {
        if (!out.empty()) out += ", ";
        out += bus;
    }
    return out;
}

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
#endif  // AFTER_HOURS_ENABLE_E2E_TESTING

struct HandleExpectSoundLevelCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_sound_level")) return;
        if (cmd.args.size() < 2 || cmd.args.size() > 3) {
            cmd.fail("expect_sound_level requires: <sound-name> <min> [max]");
            return;
        }
        const std::string name = cmd.arg(0);
        const auto min_vol = cmd.maybe_arg_as<float>(1);
        if (!min_vol) {
            cmd.fail(std::format(
                "expect_sound_level: cannot parse min '{}'", cmd.arg(1)));
            return;
        }
        float max_vol = std::numeric_limits<float>::infinity();
        if (cmd.args.size() == 3) {
            const auto parsed = cmd.maybe_arg_as<float>(2);
            if (!parsed || !std::isfinite(*parsed)) {
                cmd.fail(std::format(
                    "expect_sound_level: cannot parse max '{}'", cmd.arg(2)));
                return;
            }
            max_vol = *parsed;
        }
        if (!std::isfinite(*min_vol) || *min_vol < 0.f || *min_vol > max_vol) {
            cmd.fail(std::format("expect_sound_level: invalid range [{:.3f}, {:.3f}]",
                                 *min_vol, max_vol));
            return;
        }
        const auto got = last_played_volume(name);
        if (!got) {
            cmd.retry_with(std::format(
                "expect_sound_level: '{}' was not played", name));
            return;
        }
        cmd.expect(*got >= *min_vol && *got <= max_vol,
                   std::format("expect_sound_level: '{}' last played at {:.3f}, "
                               "expected [{:.3f}, {:.3f}]",
                               name, *got, *min_vol, max_vol));
        if (cmd.is_consumed() && cmd.succeeded()) {
            log_info("expect_sound_level: '{}' at {:.3f} in [{:.3f}, {:.3f}] "
                     "(PASS)",
                     name.c_str(), *got, *min_vol, max_vol);
        }
    }
};

struct HandleExpectSoundPanCommand : System<PendingE2ECommand> {
    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_sound_pan")) return;
        if (cmd.args.size() < 2 || cmd.args.size() > 3) {
            cmd.fail("expect_sound_pan requires: <sound-name> <min> [max]");
            return;
        }
        const std::string name = cmd.arg(0);
        const auto min_pan = cmd.maybe_arg_as<float>(1);
        if (!min_pan) {
            cmd.fail(std::format("expect_sound_pan: cannot parse min '{}'",
                                 cmd.arg(1)));
            return;
        }
        float max_pan = 1.f;
        if (cmd.args.size() == 3) {
            const auto parsed = cmd.maybe_arg_as<float>(2);
            if (!parsed || !std::isfinite(*parsed)) {
                cmd.fail(std::format("expect_sound_pan: cannot parse max '{}'",
                                     cmd.arg(2)));
                return;
            }
            max_pan = *parsed;
        }
        if (!std::isfinite(*min_pan) || *min_pan < 0.f ||
            !std::isfinite(max_pan) || max_pan > 1.f || *min_pan > max_pan) {
            cmd.fail(std::format("expect_sound_pan: invalid range [{:.3f}, {:.3f}] "
                                 "(pan is 0..1)",
                                 *min_pan, max_pan));
            return;
        }
        const auto got = last_played_pan(name);
        if (!got) {
            cmd.retry_with(
                std::format("expect_sound_pan: '{}' was not played", name));
            return;
        }
        cmd.expect(*got >= *min_pan && *got <= max_pan,
                   std::format("expect_sound_pan: '{}' last panned at {:.3f}, "
                               "expected [{:.3f}, {:.3f}]",
                               name, *got, *min_pan, max_pan));
        if (cmd.is_consumed() && cmd.succeeded()) {
            log_info("expect_sound_pan: '{}' at {:.3f} in [{:.3f}, {:.3f}] "
                     "(PASS)",
                     name.c_str(), *got, *min_pan, max_pan);
        }
    }
};

inline void register_audio_commands(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<HandleExpectSoundPlayedCommand>());
    sm.register_update_system(
        std::make_unique<HandleExpectNoSoundPlayedCommand>());
    sm.register_update_system(std::make_unique<HandleClearSoundLogCommand>());
    sm.register_update_system(
        std::make_unique<HandleExpectSoundLevelCommand>());
    sm.register_update_system(
        std::make_unique<HandleExpectSoundPanCommand>());
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
    sm.register_update_system(std::make_unique<HandleSetVolumeCommand>());
    sm.register_update_system(std::make_unique<HandleExpectVolumeCommand>());
#endif
}

}  // namespace audio_commands
}  // namespace testing
}  // namespace afterhours
