#define AFTER_HOURS_USE_NONE
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/audio_helpers.h>

#include <cstdio>

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
  afterhours::MusicType music{};

  afterhours::PauseMusicStream(music);
  check(true, "PauseMusicStream compiles and returns on none backend");

  afterhours::ResumeMusicStream(music);
  check(true, "ResumeMusicStream compiles and returns on none backend");

  afterhours::SetMusicPitch(music, 1.5f);
  check(true, "SetMusicPitch compiles and returns on none backend");

  afterhours::SetMusicPan(music, 0.25f);
  check(true, "SetMusicPan compiles and returns on none backend");

  check(afterhours::GetMusicTimePlayed(music) == 0.f,
        "GetMusicTimePlayed returns 0 on none backend");

  afterhours::SeekMusicStream(music, 2.5f);
  check(true, "SeekMusicStream compiles and returns on none backend");

  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
