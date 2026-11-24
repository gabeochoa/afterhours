#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cstdio>
#include <map>
#include <string>

namespace {
std::map<unsigned int, float> sound_gains;
unsigned int next_id = 1;
int checks = 0;
int failures = 0;
float fake_now = 0.f;

void check(bool cond, const char *what) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}
}  // namespace

namespace raylib {
Sound LoadSound(const char *) {
    Sound s{};
    s.frameCount = next_id++;
    return s;
}
void SetSoundVolume(Sound s, float v) { sound_gains[s.frameCount] = v; }
void SetSoundPan(Sound, float) {}
void SetSoundPitch(Sound, float) {}
void UnloadSound(Sound) {}
void PlaySound(Sound) {}
bool IsSoundPlaying(Sound) { return false; }

Music LoadMusicStream(const char *) {
    Music m{};
    m.frameCount = next_id++;
    return m;
}
void SetMusicVolume(Music, float) {}
void UnloadMusicStream(Music) {}
void PlayMusicStream(Music) {}
void StopMusicStream(Music) {}
void UpdateMusicStream(Music) {}
}  // namespace raylib

int main() {
    using Audio = afterhours::sound_system;
    auto &sounds = Audio::SoundLibrary::get();
    sounds.set_caption_clock([] { return fake_now; });
    sounds.load("bird.wav", "bird");
    sounds.load("wind.wav", "wind");
    sounds.set_caption("bird", "[bird chirps]", 3.f);

    check(!sounds.last_caption().has_value(), "no caption before any play");
    sounds.play("wind");
    check(!sounds.last_caption().has_value(),
          "uncaptioned sound produces no caption");

    fake_now = 100.f;
    sounds.play("bird");
    auto cap = sounds.last_caption();
    check(cap.has_value(), "caption queryable after play");
    if (cap) {
        check(cap->text == "[bird chirps]", "caption text matches");
        check(cap->started_at == 100.f, "caption stamped with injected clock");
        check(cap->duration == 3.f, "caption duration kept");
    }

    fake_now = 102.9f;
    check(sounds.last_caption().has_value(), "caption alive before expiry");
    fake_now = 103.f;
    check(!sounds.last_caption().has_value(), "caption expires at duration");

    sounds.clear_caption("bird");
    check(!sounds.has_caption("bird"), "clear_caption drops the mapping");
    fake_now = 200.f;
    sounds.play("bird");
    check(!sounds.last_caption().has_value(),
          "cleared caption stays silent on play");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
