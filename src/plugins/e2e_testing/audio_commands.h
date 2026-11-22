// Opt-in: register_audio_commands(sm) plus sound_system::set_recording(true).
#pragma once

#include "../../logging.h"
#include "../sound_system.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace audio_commands {

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

inline void register_audio_commands(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<HandleExpectSoundPlayedCommand>());
    sm.register_update_system(
        std::make_unique<HandleExpectNoSoundPlayedCommand>());
    sm.register_update_system(std::make_unique<HandleClearSoundLogCommand>());
}

}  // namespace audio_commands
}  // namespace testing
}  // namespace afterhours
