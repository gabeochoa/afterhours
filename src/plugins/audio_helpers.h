#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "../logging.h"

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using SoundType = raylib::Sound;
using MusicType = raylib::Music;
using WaveType = raylib::Wave;

inline void InitAudioDevice() { raylib::InitAudioDevice(); }
inline void CloseAudioDevice() { raylib::CloseAudioDevice(); }

inline void PlaySound(raylib::Sound sound) { raylib::PlaySound(sound); }
inline bool IsSoundPlaying(raylib::Sound sound) {
  return raylib::IsSoundPlaying(sound);
}
inline void SetSoundVolume(raylib::Sound sound, float volume) {
  raylib::SetSoundVolume(sound, volume);
}
inline void UnloadSound(raylib::Sound sound) { raylib::UnloadSound(sound); }
inline raylib::Sound LoadSound(const char *filename) {
  return raylib::LoadSound(filename);
}
inline raylib::Sound LoadSoundFromWave(raylib::Wave wave) {
  return raylib::LoadSoundFromWave(wave);
}

inline void UnloadWave(raylib::Wave wave) { raylib::UnloadWave(wave); }
inline bool ExportWave(raylib::Wave wave, const char *path) {
  return raylib::ExportWave(wave, path);
}

inline void PlayMusicStream(raylib::Music music) {
  raylib::PlayMusicStream(music);
}
inline void StopMusicStream(raylib::Music music) {
  raylib::StopMusicStream(music);
}
inline void UpdateMusicStream(raylib::Music music) {
  raylib::UpdateMusicStream(music);
}
inline void SetMusicVolume(raylib::Music music, float volume) {
  raylib::SetMusicVolume(music, volume);
}
inline void UnloadMusicStream(raylib::Music music) {
  raylib::UnloadMusicStream(music);
}
inline raylib::Music LoadMusicStream(const char *filename) {
  return raylib::LoadMusicStream(filename);
}
#else
struct SoundStub {};
struct MusicStub {
  bool looping = false;
};
struct WaveStub {
  unsigned int frameCount;
  unsigned int sampleRate;
  unsigned int sampleSize;
  unsigned int channels;
  void *data;
};
using SoundType = SoundStub;
using MusicType = MusicStub;
using WaveType = WaveStub;

inline void InitAudioDevice() {}
inline void CloseAudioDevice() {}

inline void PlaySound(SoundStub) {}
inline bool IsSoundPlaying(SoundStub) { return false; }
inline void SetSoundVolume(SoundStub, float) {}
inline void UnloadSound(SoundStub) {}
inline SoundStub LoadSound(const char *) { return SoundStub{}; }
inline SoundStub LoadSoundFromWave(WaveStub) { return SoundStub{}; }

inline void UnloadWave(WaveStub w) { std::free(w.data); }
inline bool ExportWave(WaveStub, const char *) { return false; }

inline void PlayMusicStream(MusicStub) {}
inline void StopMusicStream(MusicStub) {}
inline void UpdateMusicStream(MusicStub) {}
inline void SetMusicVolume(MusicStub, float) {}
inline void UnloadMusicStream(MusicStub) {}
inline MusicStub LoadMusicStream(const char *) { return MusicStub{}; }
#endif

namespace synth {

// Synthesized beep: a sine at freq Hz for seconds, with a short attack and
// release envelope so it clicks less. Returns a SoundType the caller owns
// (UnloadSound when done); the audio device must already be initialised.
// Non-positive inputs return an empty sound without touching audio.
inline SoundType beep(float freq, float seconds) {
#ifdef AFTER_HOURS_USE_RAYLIB
  if (!(freq > 0.f) || !(seconds > 0.f))
    return SoundType{};
  constexpr int sample_rate = 44100;
  const int frames = static_cast<int>(sample_rate * seconds);
  std::vector<short> samples(static_cast<size_t>(frames));
  const int attack = sample_rate / 200;
  const int release = sample_rate / 80;
  for (int i = 0; i < frames; i++) {
    const float t = static_cast<float>(i) / static_cast<float>(sample_rate);
    const float env =
        std::min(1.f, static_cast<float>(i) / static_cast<float>(attack)) *
        std::min(1.f, static_cast<float>(frames - i) /
                           static_cast<float>(release));
    samples[static_cast<size_t>(i)] = static_cast<short>(
        sinf(2.f * 3.14159265f * freq * t) * env * 8000.f);
  }
  WaveType wave{.frameCount = static_cast<unsigned>(frames),
                .sampleRate = static_cast<unsigned>(sample_rate),
                .sampleSize = 16,
                .channels = 1,
                .data = samples.data()};
  return ::afterhours::LoadSoundFromWave(wave);
#else
  (void)freq;
  (void)seconds;
  return SoundType{}; // no audio here; playing it is already a no-op
#endif
}

} // namespace synth

} // namespace afterhours
