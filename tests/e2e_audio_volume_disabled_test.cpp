// Compile-out proof for the e2e volume commands: WITHOUT
// AFTER_HOURS_ENABLE_E2E_TESTING, the set_volume/expect_volume handlers and
// the sound_system bus-enumeration helpers must not exist, while the
// always-available audio API keeps working. Modeled on
// profiling_disabled_test.cpp.
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) && \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif

#include <afterhours/src/plugins/e2e_testing/audio_commands.h>

using namespace afterhours;
namespace audio = afterhours::testing::audio_commands;

// The PR #449 additions are gone without the e2e macro.
static_assert(!requires { sound_system::known_buses(); });
static_assert(!requires { sound_system::has_bus("effects"); });
static_assert(!requires { typename audio::HandleSetVolumeCommand; });
static_assert(!requires { typename audio::HandleExpectVolumeCommand; });
static_assert(!requires { audio::known_buses_string(); });

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
