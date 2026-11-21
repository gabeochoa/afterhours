#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace {
std::map<unsigned int, float> sound_gains;
std::map<unsigned int, float> sound_pans;
std::map<unsigned int, float> sound_pitches;
std::map<unsigned int, float> music_gains;
std::set<unsigned int> playing;
unsigned int next_id = 1;
int checks = 0;
int failures = 0;

void check(bool cond, const char *what) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}
void check_near(float actual, float expected, const char *what) {
    ++checks;
    if (std::abs(actual - expected) > 0.0001f) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s: expected %.4f got %.4f\n", what,
                     expected, actual);
    }
}
}

namespace raylib {
Sound LoadSound(const char *) {
    Sound s{};
    s.frameCount = next_id++;
    sound_gains[s.frameCount] = 1.f;
    return s;
}
Sound LoadSoundFromWave(Wave) {
    Sound s{};
    s.frameCount = next_id++;
    sound_gains[s.frameCount] = 1.f;
    return s;
}
Sound LoadSoundAlias(Sound src) {
    Sound s{};
    s.frameCount = next_id++;
    sound_gains[s.frameCount] = sound_gains[src.frameCount];
    return s;
}
void UnloadSoundAlias(Sound) {}
void UnloadSound(Sound) {}
void SetSoundVolume(Sound s, float v) { sound_gains[s.frameCount] = v; }
void SetSoundPan(Sound s, float p) { sound_pans[s.frameCount] = p; }
void SetSoundPitch(Sound s, float p) { sound_pitches[s.frameCount] = p; }
void PlaySound(Sound s) { playing.insert(s.frameCount); }
void StopSound(Sound s) { playing.erase(s.frameCount); }
void PauseSound(Sound s) { playing.erase(s.frameCount); }
void ResumeSound(Sound s) { playing.insert(s.frameCount); }
bool IsSoundPlaying(Sound s) { return playing.contains(s.frameCount); }

Music LoadMusicStream(const char *) {
    Music m{};
    m.frameCount = next_id++;
    music_gains[m.frameCount] = 1.f;
    return m;
}
void SetMusicVolume(Music m, float v) { music_gains[m.frameCount] = v; }
void UnloadMusicStream(Music) {}
void PlayMusicStream(Music) {}
void StopMusicStream(Music) {}
void UpdateMusicStream(Music) {}
}

int main() {
    using Audio = afterhours::sound_system;
    auto &sounds = Audio::SoundLibrary::get();
    auto &music = Audio::MusicLibrary::get();

    Audio::set_master_volume(1.f);
    Audio::set_sound_volume(1.f);
    Audio::set_music_volume(1.f);

    sounds.load_on_bus("bird.wav", "bird", "ambience");
    music.load_on_bus("song.ogg", "song", "music");
    Audio::set_master_volume(0.5f);
    Audio::set_sound_volume(0.8f);
    Audio::set_bus_volume("ambience", 0.5f);
    check_near(sound_gains.at(sounds.get("bird").frameCount), 0.2f,
               "bus gain = master 0.5 * effects 0.8 * ambience 0.5");
    check_near(Audio::get_bus_volume("ambience"), 0.5f, "bus volume reads back");
    check_near(music_gains.at(music.get("song").frameCount), 0.5f,
               "music unaffected by ambience bus (master only)");
    Audio::set_bus_volume("music", 0.25f);
    check_near(music_gains.at(music.get("song").frameCount), 0.125f,
               "music bus composes with master");
    Audio::set_master_volume(1.f);
    Audio::set_bus_volume("ambience", 1.f);
    Audio::set_bus_volume("music", 1.f);

    Audio::Listener listener;
    listener.position = {0.f, 0.f, 0.f};
    listener.forward = {0.f, 0.f, -1.f};
    listener.up = {0.f, 1.f, 0.f};
    auto ahead = Audio::spatial_gain(listener, {0.f, 0.f, -2.f}, 10.f);
    check_near(ahead.volume, 0.5f, "inverse falloff at 2m (ref 1m)");
    check_near(ahead.pan, 0.5f, "ahead pans centre");
    auto right = Audio::spatial_gain(listener, {5.f, 0.f, 0.f}, 10.f);
    check_near(right.volume, 0.2f, "inverse falloff at 5m");
    check_near(right.pan, 1.f, "hard right pans to 1");
    auto left = Audio::spatial_gain(listener, {-5.f, 0.f, 0.f}, 10.f);
    check_near(left.pan, 0.f, "hard left pans to 0");
    auto far = Audio::spatial_gain(listener, {0.f, 0.f, -20.f}, 10.f);
    check_near(far.volume, 0.f, "silent beyond max distance");
    auto near = Audio::spatial_gain(listener, {0.f, 0.f, -0.5f}, 10.f);
    check_near(near.volume, 1.f, "full volume inside reference distance");

    Audio::set_listener(listener);
    sounds.set_range("bird", 10.f);
    check(Audio::play_at("bird", {0.f, 0.f, -2.f}), "play_at in range plays");
    check_near(sound_gains.at(sounds.get("bird").frameCount), 0.4f,
               "play_at volume = effects 0.8 * falloff 0.5");
    check_near(sound_pans.at(sounds.get("bird").frameCount), 0.5f,
               "backend received pan");
    check(sounds.is_playing("bird"), "play_at sound is playing");
    check(!Audio::play_at("bird", {0.f, 0.f, -20.f}),
          "play_at out of range plays nothing");
    check(!Audio::play_at("missing", {0.f, 0.f, 0.f}),
          "play_at unknown sound returns false");

    sounds.load_pool("chirp.wav", "chirp", 3, "ambience");
    check(sounds.contains("chirp"), "pool base registered");
    check(sounds.contains("chirp_a0") && sounds.contains("chirp_a1") &&
              sounds.contains("chirp_a2"),
          "pool aliases registered");
    sounds.play_loaded("chirp_a1");
    check(sounds.is_playing("chirp_a1"), "pool alias play is playing");

    const std::vector<float> samples(441, 0.25f);
    check(sounds.add_samples("beep_gen", samples, 44100, 1, "effects"),
          "add_samples registers generated sound");
    check(sounds.contains("beep_gen"), "generated sound is in the library");
    check(!sounds.add_samples("beep_gen", samples, 44100),
          "duplicate generated registration rejected");
    check_near(sound_gains.at(sounds.get("beep_gen").frameCount), 0.8f,
               "generated sound got category gain at registration");
    auto direct = afterhours::load_sound_from_samples(samples, 44100, 1);
    check(sounds.add("beep_direct", direct), "load_sound_from_samples + add");
    check(afterhours::load_sound_from_samples({}, 44100).frameCount == 0,
          "empty samples return an empty sound");
    sounds.play("beep_gen");
    check(sounds.is_playing("beep_gen"), "generated sound play is playing");

    check(sounds.is_playing("beep_gen"), "played generated sound is playing");
    sounds.pause("beep_gen");
    check(!sounds.is_playing("beep_gen"), "pause stops playback state");
    sounds.resume("beep_gen");
    check(sounds.is_playing("beep_gen"), "resume restarts playback state");
    sounds.stop("beep_gen");
    check(!sounds.is_playing("beep_gen"), "stop ends playback state");

    sounds.unload_all();
    music.unload_all();
    std::printf("%d/%d audio plugin checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
