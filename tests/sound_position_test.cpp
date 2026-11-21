#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cmath>
#include <cstdio>

namespace {
std::set<unsigned int> playing;
unsigned int next_id = 1;
int checks = 0;
int failures = 0;
double fake_now = 0.0;

void check(bool cond, const char *what) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}
void check_near(double actual, double expected, const char *what) {
    ++checks;
    if (std::abs(actual - expected) > 1e-9) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s: expected %.6f got %.6f\n", what,
                     expected, actual);
    }
}
}  // namespace

namespace raylib {
Sound LoadSound(const char *) {
    Sound s{};
    s.frameCount = next_id++;
    return s;
}
Sound LoadSoundFromWave(Wave) {
    Sound s{};
    s.frameCount = next_id++;
    return s;
}
Sound LoadSoundAlias(Sound) {
    Sound s{};
    s.frameCount = next_id++;
    return s;
}
void UnloadSoundAlias(Sound) {}
void UnloadSound(Sound) {}
void SetSoundVolume(Sound, float) {}
void SetSoundPan(Sound, float) {}
void SetSoundPitch(Sound, float) {}
void PlaySound(Sound s) { playing.insert(s.frameCount); }
void StopSound(Sound s) { playing.erase(s.frameCount); }
void PauseSound(Sound s) { playing.erase(s.frameCount); }
void ResumeSound(Sound s) { playing.insert(s.frameCount); }
bool IsSoundPlaying(Sound s) { return playing.contains(s.frameCount); }

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
    Audio::set_playback_clock([] { return fake_now; });
    auto &sounds = Audio::SoundLibrary::get();
    auto &music = Audio::MusicLibrary::get();

    sounds.load("bird.wav", "bird");
    music.load("song.ogg", "song");

    check_near(sounds.playback_position("bird"), 0.0,
               "never-played sound is at 0");
    check_near(sounds.playback_position("missing"), 0.0,
               "unknown sound is at 0");

    fake_now = 10.0;
    sounds.play("bird");
    check_near(sounds.playback_position("bird"), 0.0, "starts at 0");
    fake_now = 12.5;
    check_near(sounds.playback_position("bird"), 2.5,
               "position advances with the injected clock");

    sounds.pause("bird");
    check_near(sounds.playback_position("bird"), 2.5, "pause freezes");
    fake_now = 20.0;
    check_near(sounds.playback_position("bird"), 2.5,
               "paused position ignores clock");

    sounds.resume("bird");
    fake_now = 21.0;
    check_near(sounds.playback_position("bird"), 3.5,
               "resume continues from the frozen point");

    sounds.stop("bird");
    check_near(sounds.playback_position("bird"), 0.0, "stop resets");
    fake_now = 30.0;
    check_near(sounds.playback_position("bird"), 0.0,
               "stopped position stays at 0");

    Audio::set_listener(Audio::Listener{});
    sounds.set_range("bird", 10.f);
    fake_now = 100.0;
    check(Audio::play_at("bird", {0.f, 0.f, -2.f}, 1.f, 50.f, 2.f),
          "play_at with pitch 2 plays");
    fake_now = 101.0;
    check_near(sounds.playback_position("bird"), 2.0,
               "pitch 2 advances position twice as fast");

    fake_now = 200.0;
    music.play("song");
    fake_now = 203.0;
    check_near(music.playback_position("song"), 3.0,
               "music position advances with the injected clock");
    music.stop("song");
    check_near(music.playback_position("song"), 0.0, "music stop resets");

    sounds.unload_all();
    music.unload_all();
    std::printf("%d/%d sound position checks passed\n", checks - failures,
                checks);
    return failures ? 1 : 0;
}
