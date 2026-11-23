// Compile-out proof for the e2e volume commands: WITHOUT
// AFTER_HOURS_ENABLE_E2E_TESTING, the sound_system bus-enumeration helpers
// behind set_volume/expect_volume must not exist, while the always-available
// audio API keeps working. (The command handlers themselves are proven
// absent by the symbol check in the PR: no HandleSetVolumeCommand /
// HandleExpectVolumeCommand / known_buses symbols in this binary.)
// Modeled on profiling_disabled_test.cpp.
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) && \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif

#include <afterhours/src/plugins/e2e_testing/audio_commands.h>

using namespace afterhours;
namespace audio = afterhours::testing::audio_commands;

// Absence probes go through concepts: in a dependent context an invalid
// expression makes the concept false instead of a hard error.
template <typename T>
concept HasKnownBuses = requires { T::known_buses(); };
template <typename T>
concept HasHasBus = requires { T::has_bus("effects"); };

// The PR #449 helpers are gone without the e2e macro.
static_assert(!HasKnownBuses<sound_system>);
static_assert(!HasHasBus<sound_system>);
static_assert(!HasKnownBuses<sound_system::SoundLibrary>);
static_assert(!HasKnownBuses<sound_system::MusicLibrary>);

// The pre-existing, non-e2e audio surface is untouched.
static_assert(requires { sound_system::get_bus_volume("effects"); });
static_assert(requires { sound_system::set_bus_volume("effects", 0.5f); });
static_assert(requires { typename audio::HandleExpectSoundPlayedCommand; });

int main() {
    // register_audio_commands still links and runs; it simply has no
    // volume handlers to register in this build.
    SystemManager sm;
    audio::register_audio_commands(sm);
    sound_system::set_bus_volume("effects", 0.5f);
    return sound_system::get_bus_volume("effects") == 0.5f ? 0 : 1;
}
