// Stub audio backend (sokol/none) warns once when sound is silently dropped
// (decide-sokol-audio Option 3). Compiled without AFTER_HOURS_USE_RAYLIB so
// audio_helpers.h takes the stub branch.

#include <afterhours/src/plugins/audio_helpers.h>

#include <cstdio>
#include <string>

using afterhours::CloseAudioDevice;
using afterhours::InitAudioDevice;
using afterhours::IsAudioDeviceReady;
using afterhours::PlaySound;
using afterhours::SoundType;

static int checks_run = 0;
static int checks_passed = 0;
static std::string captured;
static int warn_count = 0;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

static void capture_sink(const char *level, const char *message) {
  captured += message;
  captured += "\n";
  if (std::string(level) == "[WARN]" &&
      std::string(message).find("silent") != std::string::npos)
    warn_count++;
}

int main() {
  const auto previous = log_sink_fn;
  log_sink_fn = capture_sink;

  InitAudioDevice();
  check(warn_count == 1, "first InitAudioDevice warns once");
  InitAudioDevice();
  check(warn_count == 1, "second InitAudioDevice does not warn again");

  PlaySound(SoundType{});
  check(warn_count == 1, "PlaySound after InitAudioDevice adds no second warning");

  // Reset the once-flag and confirm PlaySound alone also warns exactly once.
  afterhours::audio_stub_detail::warned() = false;
  PlaySound(SoundType{});
  check(warn_count == 2, "PlaySound warns once when it is the first drop");
  PlaySound(SoundType{});
  check(warn_count == 2, "second PlaySound does not warn again");

  check(captured.find("no audio device") != std::string::npos,
        "warning says there is no audio device");
  check(captured.find("silent") != std::string::npos,
        "warning says sound is silent");

  check(!IsAudioDeviceReady(), "stub still reports no device");
  CloseAudioDevice();

  log_sink_fn = previous;

  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
