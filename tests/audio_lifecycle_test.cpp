#define AFTER_HOURS_USE_RAYLIB

#include <afterhours/src/graphics.h>

#include <cstdio>

namespace g = afterhours::graphics;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

int main() {
  {
    g::Config cfg;
    cfg.display = g::DisplayMode::Headless;
    cfg.width = 64;
    cfg.height = 48;
    if (!g::init(cfg)) {
      std::fprintf(stderr, "  FAIL: headless init\n");
      return 1;
    }
    check(!raylib::IsAudioDeviceReady(), "audio stays closed by default");
    g::shutdown();
  }

  raylib::InitAudioDevice();
  const bool machine_has_audio = raylib::IsAudioDeviceReady();
  raylib::CloseAudioDevice();
  if (!machine_has_audio) {
    std::printf("no audio device on this machine; skipping device checks\n");
  } else {
    {
      g::Config cfg;
      cfg.display = g::DisplayMode::Headless;
      cfg.width = 64;
      cfg.height = 48;
      cfg.audio = true;
      g::init(cfg);
      check(raylib::IsAudioDeviceReady(), "init opens the device when asked");
      g::shutdown();
      check(!raylib::IsAudioDeviceReady(), "shutdown closes it");
    }
    {
      bool ready_in_frame = false;
      g::RunConfig cfg;
      cfg.display = g::DisplayMode::Headless;
      cfg.audio = true;
      cfg.frame = [&] {
        ready_in_frame = raylib::IsAudioDeviceReady();
        g::request_quit();
      };
      g::run(cfg);
      check(ready_in_frame, "run opens the device for its frames");
      check(!raylib::IsAudioDeviceReady(), "run closes it on the way out");
    }
  }

  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
