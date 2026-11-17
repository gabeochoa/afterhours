#include <cstdio>

#include "../../../../src/graphics_common.h"
#include "../../../../src/plugins/audio_helpers.h"

namespace audio = afterhours;

int main() {
  audio::InitAudioDevice();
  if (!audio::IsAudioDeviceReady()) {
    std::printf("no audio device; nothing to show\n");
    return 0;
  }
  audio::SetMasterVolume(0.f);

  audio::SoundType line = audio::synth::beep(220.f, 3.f);
  audio::PlaySound(line);
  audio::SetSoundPitch(line, 1.25f);
  audio::SetSoundPan(line, 0.3f);
  std::printf("playing: %d\n", audio::IsSoundPlaying(line));

  audio::PauseSound(line);
  std::printf("paused:  %d\n", audio::IsSoundPlaying(line));
  audio::ResumeSound(line);
  std::printf("resumed: %d\n", audio::IsSoundPlaying(line));

  audio::StopSound(line);
  const bool stopped = !audio::IsSoundPlaying(line);
  std::printf("skipped: %d\n", stopped);

  audio::UnloadSound(line);
  audio::CloseAudioDevice();
  return stopped ? 0 : 1;
}
