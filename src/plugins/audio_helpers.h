#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

#include "../logging.h"
#include "../warn_once.h"

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using SoundType = raylib::Sound;
using MusicType = raylib::Music;
using WaveType = raylib::Wave;

inline void InitAudioDevice() { raylib::InitAudioDevice(); }
inline void CloseAudioDevice() { raylib::CloseAudioDevice(); }
inline bool IsAudioDeviceReady() { return raylib::IsAudioDeviceReady(); }
inline void SetMasterVolume(float volume) { raylib::SetMasterVolume(volume); }
inline float GetMasterVolume() { return raylib::GetMasterVolume(); }
inline void SetAudioStreamBufferSizeDefault(int size) {
  raylib::SetAudioStreamBufferSizeDefault(size);
}

inline void PlaySound(raylib::Sound sound) { raylib::PlaySound(sound); }
inline void StopSound(raylib::Sound sound) { raylib::StopSound(sound); }
inline void PauseSound(raylib::Sound sound) { raylib::PauseSound(sound); }
inline void ResumeSound(raylib::Sound sound) { raylib::ResumeSound(sound); }
inline void SetSoundPitch(raylib::Sound sound, float pitch) {
  raylib::SetSoundPitch(sound, pitch);
}
inline void SetSoundPan(raylib::Sound sound, float pan) {
  raylib::SetSoundPan(sound, pan);
}
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
inline SoundType LoadSoundAlias(SoundType source) {
  return raylib::LoadSoundAlias(source);
}
inline void UnloadSoundAlias(SoundType alias) {
  raylib::UnloadSoundAlias(alias);
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
inline bool IsMusicStreamPlaying(raylib::Music music) {
  return raylib::IsMusicStreamPlaying(music);
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

using AudioStreamType = raylib::AudioStream;
// Fill callback: buffer is interleaved 32-bit float, frames*channels long.
using StreamFillCallback = raylib::AudioCallback;

inline AudioStreamType LoadAudioStream(unsigned int sample_rate,
                                       unsigned int sample_size,
                                       unsigned int channels) {
  return raylib::LoadAudioStream(sample_rate, sample_size, channels);
}
inline bool IsAudioStreamValid(AudioStreamType stream) {
  return raylib::IsAudioStreamValid(stream);
}
inline void UnloadAudioStream(AudioStreamType stream) {
  raylib::UnloadAudioStream(stream);
}
inline void PlayAudioStream(AudioStreamType stream) {
  raylib::PlayAudioStream(stream);
}
inline void PauseAudioStream(AudioStreamType stream) {
  raylib::PauseAudioStream(stream);
}
inline void ResumeAudioStream(AudioStreamType stream) {
  raylib::ResumeAudioStream(stream);
}
inline void StopAudioStream(AudioStreamType stream) {
  raylib::StopAudioStream(stream);
}
inline bool IsAudioStreamPlaying(AudioStreamType stream) {
  return raylib::IsAudioStreamPlaying(stream);
}
inline void SetAudioStreamCallback(AudioStreamType stream,
                                   StreamFillCallback callback) {
  raylib::SetAudioStreamCallback(stream, callback);
}

// RAII raw stream; format is fixed at creation. No non-raylib version:
// raylib has no userdata for the callback, so it must be a plain function.
class AudioStream {
 public:
  AudioStream() = default;
  AudioStream(int sample_rate, int channels, StreamFillCallback fill) {
    open(sample_rate, channels, fill);
  }
  ~AudioStream() { close(); }
  AudioStream(const AudioStream &) = delete;
  AudioStream &operator=(const AudioStream &) = delete;
  AudioStream(AudioStream &&other) noexcept : stream_(other.stream_) {
    other.stream_ = AudioStreamType{};
  }
  AudioStream &operator=(AudioStream &&other) noexcept {
    if (this != &other) {
      close();
      stream_ = other.stream_;
      other.stream_ = AudioStreamType{};
    }
    return *this;
  }

  bool open(int sample_rate, int channels, StreamFillCallback fill) {
    close();
    if (sample_rate <= 0 || channels <= 0 || fill == nullptr) return false;
    stream_ = raylib::LoadAudioStream(
        static_cast<unsigned int>(sample_rate), 32,
        static_cast<unsigned int>(channels));
    if (!raylib::IsAudioStreamValid(stream_)) {
      stream_ = AudioStreamType{};
      return false;
    }
    raylib::SetAudioStreamCallback(stream_, fill);
    return true;
  }
  void close() {
    if (!valid()) return;
    raylib::StopAudioStream(stream_);
    raylib::UnloadAudioStream(stream_);
    stream_ = AudioStreamType{};
  }
  [[nodiscard]] bool valid() const {
    return stream_.buffer != nullptr && raylib::IsAudioStreamValid(stream_);
  }
  void start() {
    if (valid()) raylib::PlayAudioStream(stream_);
  }
  void pause() {
    if (valid()) raylib::PauseAudioStream(stream_);
  }
  void resume() {
    if (valid()) raylib::ResumeAudioStream(stream_);
  }
  void stop() {
    if (valid()) raylib::StopAudioStream(stream_);
  }
  [[nodiscard]] bool is_playing() const {
    return valid() && raylib::IsAudioStreamPlaying(stream_);
  }
  [[nodiscard]] int sample_rate() const {
    return static_cast<int>(stream_.sampleRate);
  }
  [[nodiscard]] int channels() const {
    return static_cast<int>(stream_.channels);
  }
  [[nodiscard]] AudioStreamType native() const { return stream_; }

 private:
  AudioStreamType stream_{};
};
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
inline bool IsAudioDeviceReady() { return false; }
inline void SetMasterVolume(float) {}
inline float GetMasterVolume() { return 0.f; }
inline void SetAudioStreamBufferSizeDefault(int) {}

inline void PlaySound(SoundStub) {}
inline void StopSound(SoundStub) {}
inline void PauseSound(SoundStub) {}
inline void ResumeSound(SoundStub) {}
inline void SetSoundPitch(SoundStub, float) {}
inline void SetSoundPan(SoundStub, float) {}
inline bool IsSoundPlaying(SoundStub) { return false; }
inline void SetSoundVolume(SoundStub, float) {}
inline void UnloadSound(SoundStub) {}
inline SoundStub LoadSound(const char *) { return SoundStub{}; }
inline SoundStub LoadSoundFromWave(WaveStub) { return SoundStub{}; }
inline SoundStub LoadSoundAlias(SoundStub) { return SoundStub{}; }
inline void UnloadSoundAlias(SoundStub) {}

inline void UnloadWave(WaveStub w) { std::free(w.data); }
inline bool ExportWave(WaveStub, const char *) { return false; }

inline void PlayMusicStream(MusicStub) {}
inline void StopMusicStream(MusicStub) {}
inline bool IsMusicStreamPlaying(MusicStub) { return false; }
inline void UpdateMusicStream(MusicStub) {}
inline void SetMusicVolume(MusicStub, float) {}
inline void UnloadMusicStream(MusicStub) {}
inline MusicStub LoadMusicStream(const char *) { return MusicStub{}; }
#endif

inline SoundType load_sound_from_samples(std::span<const float> samples,
                                         int sample_rate, int channels = 1) {
  if (samples.empty() || sample_rate <= 0 || channels <= 0) return SoundType{};
#ifdef AFTER_HOURS_USE_RAYLIB
  const auto frames =
      static_cast<unsigned int>(samples.size() / static_cast<size_t>(channels));
  if (frames == 0) return SoundType{};
  float *copy = static_cast<float *>(
      std::malloc(samples.size() * sizeof(float)));
  if (!copy) return SoundType{};
  std::memcpy(copy, samples.data(), samples.size() * sizeof(float));
  WaveType wave{.frameCount = frames,
                .sampleRate = static_cast<unsigned int>(sample_rate),
                .sampleSize = 32,
                .channels = static_cast<unsigned int>(channels),
                .data = copy};
  SoundType sound = raylib::LoadSoundFromWave(wave);
  std::free(copy);
  return sound;
#else
  warn_once(0, "@notimplemented load_sound_from_samples on this backend");
  return SoundType{};
#endif
}

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
  log_error("@notimplemented synth::beep");
  return SoundType{};
#endif
}

} // namespace synth

} // namespace afterhours
