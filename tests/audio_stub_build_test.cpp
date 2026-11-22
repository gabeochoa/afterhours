// The library builds on the non-raylib (sokol/none) backend without the
// stub audio functions: sound_system guards its audio calls, so the stub
// types alone let the headers parse and the libraries run. The public
// stub functions are absent on this backend.

#define AFTER_HOURS_USE_NONE
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cstdio>
#include <type_traits>

namespace ah = afterhours;
using SoundLibrary = ah::sound_system::SoundLibrary;
using MusicLibrary = ah::sound_system::MusicLibrary;
using Playback = ah::sound_system::SoundPlaybackSystem;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

int main() {
  static_assert(std::is_same_v<ah::SoundType, ah::SoundStub>,
                "non-raylib SoundType is the stub type");
  static_assert(std::is_default_constructible_v<ah::MusicType>,
                "MusicType default-constructs");
  static_assert(std::is_default_constructible_v<ah::WaveType>,
                "WaveType default-constructs");

  auto &sounds = SoundLibrary::get();
  sounds.load("click.wav", "click");
  check(sounds.contains("click"), "loaded sound is present");
  sounds.play("click");
  sounds.play("missing");
  sounds.play_random_match("cli");
  sounds.play_first_available_match("cli");
  sounds.play_if_none_playing("cli");
  sounds.update_volume(0.5f);
  check(sounds.get_volume() == 0.5f, "sound volume round-trips");

  auto &music = MusicLibrary::get();
  music.load("theme.ogg", "theme");
  check(music.contains("theme"), "loaded music is present");
  music.play("theme");
  music.play("missing");
  music.update();
  music.unload_all();

  check(!Playback::library_contains("missing"),
        "playback system contains check works");

  sounds.unload_all();
  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
