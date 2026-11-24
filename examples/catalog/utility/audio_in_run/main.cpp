#include <cstdio>

#include "../../../../src/graphics.h"
#include "../../../../src/plugins/audio_helpers.h"

namespace graphics = afterhours::graphics;

int main() {
  bool played = false;

  graphics::RunConfig config;
  config.display = graphics::DisplayMode::Headless;
  config.audio = true;
  config.frame = [&] {
    afterhours::SoundType blip = afterhours::synth::beep(880.f, 0.05f);
    raylib::SetMasterVolume(0.f);
    afterhours::PlaySound(blip);
    played = afterhours::IsSoundPlaying(blip);
    afterhours::UnloadSound(blip);
    graphics::request_quit();
  };
  graphics::run(config);

  std::printf("run opened the audio device: sound playing = %d\n", played);
  return played ? 0 : 1;
}
