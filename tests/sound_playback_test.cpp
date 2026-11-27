#define AFTER_HOURS_USE_RAYLIB

#include <afterhours/src/graphics_common.h>
#include <afterhours/src/plugins/audio_helpers.h>

#include <cmath>
#include <cstdio>
#include <vector>

using afterhours::SoundType;
using afterhours::InitAudioDevice;
using afterhours::CloseAudioDevice;
using afterhours::IsAudioDeviceReady;
using afterhours::SetMasterVolume;
using afterhours::GetMasterVolume;
namespace synth = afterhours::synth;

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
  InitAudioDevice();
  if (!IsAudioDeviceReady()) {
    std::printf("no audio device on this machine; skipping playback checks\n");
    return 0;
  }

  SetMasterVolume(0.f);
  check(std::fabs(GetMasterVolume()) < 1e-6f, "master volume round-trips");

  SoundType tone = synth::beep(440.f, 2.f);
  afterhours::PlaySound(tone);
  check(afterhours::IsSoundPlaying(tone), "a played sound is playing");
  afterhours::PauseSound(tone);
  check(!afterhours::IsSoundPlaying(tone), "a paused sound is not playing");
  afterhours::ResumeSound(tone);
  check(afterhours::IsSoundPlaying(tone), "a resumed sound plays again");
  afterhours::SetSoundPitch(tone, 1.5f);
  afterhours::SetSoundPan(tone, 0.25f);
  check(afterhours::IsSoundPlaying(tone), "pitch and pan do not stop it");
  afterhours::StopSound(tone);
  check(!afterhours::IsSoundPlaying(tone), "a stopped sound is not playing");

  const char *music_path = "/tmp/afterhours_music_stream_wrappers_test.wav";
  std::vector<float> music_samples(44100, 0.1f);
  check(afterhours::export_wav(music_path, music_samples, 44100),
        "export_wav writes a music fixture");
  afterhours::MusicType music = afterhours::LoadMusicStream(music_path);
  afterhours::PlayMusicStream(music);
  check(afterhours::IsMusicStreamPlaying(music),
        "a played music stream is playing");
  afterhours::PauseMusicStream(music);
  check(!afterhours::IsMusicStreamPlaying(music),
        "a paused music stream is not playing");
  afterhours::ResumeMusicStream(music);
  check(afterhours::IsMusicStreamPlaying(music),
        "a resumed music stream plays again");
  afterhours::SetMusicPitch(music, 1.25f);
  afterhours::SetMusicPan(music, 0.5f);
  check(afterhours::IsMusicStreamPlaying(music),
        "music pitch and pan do not stop it");
  afterhours::SeekMusicStream(music, 0.f);
  check(afterhours::GetMusicTimePlayed(music) >= 0.f,
        "music time played is non-negative after seek");
  afterhours::StopMusicStream(music);
  check(!afterhours::IsMusicStreamPlaying(music),
        "a stopped music stream is not playing");
  afterhours::UnloadMusicStream(music);
  std::remove(music_path);

  afterhours::UnloadSound(tone);
  CloseAudioDevice();
  check(!IsAudioDeviceReady(), "closing the device is visible");

  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
