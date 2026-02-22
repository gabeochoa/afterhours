#pragma once

#include <cstdlib>

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
struct MusicStub {};
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

} // namespace afterhours
