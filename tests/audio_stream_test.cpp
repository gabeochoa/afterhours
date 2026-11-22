#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool cond, const char *what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what);
  }
}
std::map<void *, afterhours::StreamFillCallback> callbacks;
std::set<void *> playing;
int unloaded = 0;
unsigned int next_id = 1;

std::vector<float> recorded;
unsigned int recorded_frames = 0;
void fill_sine(void *buffer, unsigned int frames) {
  recorded_frames = frames;
  recorded.assign(static_cast<float *>(buffer),
                  static_cast<float *>(buffer) + frames);
  auto *out = static_cast<float *>(buffer);
  for (unsigned int i = 0; i < frames; ++i)
    out[i] = std::sin(2.f * 3.14159265f * 440.f * static_cast<float>(i) /
                      44100.f);
  recorded.assign(out, out + frames);
}
}  // namespace

namespace raylib {
// Minimal sound/music mocks so sound_system.h links headless.
Sound LoadSound(const char *) { return Sound{.frameCount = next_id++}; }
Sound LoadSoundFromWave(Wave) { return Sound{.frameCount = next_id++}; }
Sound LoadSoundAlias(Sound) { return Sound{.frameCount = next_id++}; }
void UnloadSoundAlias(Sound) {}
void UnloadSound(Sound) {}
void SetSoundVolume(Sound, float) {}
void SetSoundPan(Sound, float) {}
void SetSoundPitch(Sound, float) {}
void PlaySound(Sound) {}
void StopSound(Sound) {}
void PauseSound(Sound) {}
void ResumeSound(Sound) {}
bool IsSoundPlaying(Sound) { return false; }
Music LoadMusicStream(const char *) { return Music{.frameCount = next_id++}; }
void SetMusicVolume(Music, float) {}
void UnloadMusicStream(Music) {}
void PlayMusicStream(Music) {}
void StopMusicStream(Music) {}
void UpdateMusicStream(Music) {}

AudioStream LoadAudioStream(unsigned int rate, unsigned int size,
                            unsigned int channels) {
  AudioStream s{};
  s.buffer = reinterpret_cast<rAudioBuffer *>(static_cast<uintptr_t>(next_id++));
  s.sampleRate = rate;
  s.sampleSize = size;
  s.channels = channels;
  return s;
}
bool IsAudioStreamValid(AudioStream s) { return s.buffer != nullptr; }
void UnloadAudioStream(AudioStream s) {
  callbacks.erase(s.buffer);
  playing.erase(s.buffer);
  ++unloaded;
}
void SetAudioStreamCallback(AudioStream s, AudioCallback cb) {
  callbacks[s.buffer] = cb;
}
void PlayAudioStream(AudioStream s) { playing.insert(s.buffer); }
void PauseAudioStream(AudioStream s) { playing.erase(s.buffer); }
void ResumeAudioStream(AudioStream s) { playing.insert(s.buffer); }
void StopAudioStream(AudioStream s) { playing.erase(s.buffer); }
bool IsAudioStreamPlaying(AudioStream s) { return playing.contains(s.buffer); }
}  // namespace raylib

int main() {
  using Stream = afterhours::AudioStream;

  Stream bad;
  check(!bad.open(0, 1, fill_sine), "invalid sample rate rejected");
  check(!bad.open(44100, 0, fill_sine), "invalid channels rejected");
  check(!bad.open(44100, 1, nullptr), "null callback rejected");
  check(!bad.valid(), "rejected stream is invalid");

  Stream stream;
  check(stream.open(44100, 1, fill_sine), "stream opens");
  check(stream.valid(), "opened stream is valid");
  check(stream.sample_rate() == 44100 && stream.channels() == 1,
        "format fixed at creation");
  check(callbacks.contains(stream.native().buffer), "fill callback attached");

  // Simulate the audio thread asking for 512 frames.
  std::vector<float> buffer(512, 0.f);
  callbacks[stream.native().buffer](buffer.data(), 512);
  check(recorded_frames == 512, "callback received requested frame count");
  check(recorded.size() == 512, "callback filled every requested frame");
  bool sine_props = recorded[0] == 0.f && recorded[25] > 0.99f &&
                    std::abs(recorded[50]) < 0.1f && recorded[75] < -0.99f;
  check(sine_props, "filled buffer has sine quarter-period properties");
  float energy = 0.f;
  for (float v : recorded) energy += v * v;
  check(energy > 200.f && energy < 312.f, "sine energy near frames/2");

  check(!stream.is_playing(), "not playing before start");
  stream.start();
  check(stream.is_playing(), "playing after start");
  stream.pause();
  check(!stream.is_playing(), "not playing after pause");
  stream.resume();
  check(stream.is_playing(), "playing after resume");
  stream.stop();
  check(!stream.is_playing(), "not playing after stop");

  const int before = unloaded;
  stream.close();
  check(unloaded == before + 1, "close unloads the stream");
  check(!stream.valid(), "closed stream is invalid");
  stream.start();
  check(!stream.is_playing(), "start on closed stream is a no-op");

  std::printf("%d/%d audio stream checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
