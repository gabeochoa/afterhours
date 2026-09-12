#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/sound_system.h>

#include <cmath>
#include <cstdio>
#include <map>

namespace {
std::map<unsigned int, float> sound_gains;
std::map<unsigned int, float> music_gains;
unsigned int next_id = 1;
int checks = 0;
int failures = 0;

void check_gain(float actual, float expected, const char *description) {
    ++checks;
    if (std::abs(actual - expected) < 0.00001f) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s: expected %.3f, got %.3f\n", description,
                 expected, actual);
}
}  // namespace

namespace raylib {
Sound LoadSound(const char *) {
    Sound sound{};
    sound.frameCount = next_id++;
    sound_gains[sound.frameCount] = 1.f;
    return sound;
}
void SetSoundVolume(Sound sound, float volume) {
    sound_gains.at(sound.frameCount) = volume;
}
void UnloadSound(Sound) {}

Music LoadMusicStream(const char *) {
    Music music{};
    music.frameCount = next_id++;
    music_gains[music.frameCount] = 1.f;
    return music;
}
void SetMusicVolume(Music music, float volume) {
    music_gains.at(music.frameCount) = volume;
}
void UnloadMusicStream(Music) {}
}  // namespace raylib

int main() {
    using Audio = afterhours::sound_system;
    check_gain(Audio::get_master_volume(), 1.f, "default master preference");
    auto &sounds = Audio::SoundLibrary::get();
    auto &music = Audio::MusicLibrary::get();
    sounds.load("effect.wav", "effect");
    music.load("song.ogg", "song");

    auto check_loaded = [&](float effect_gain, float music_gain) {
        check_gain(sound_gains.at(sounds.get("effect").frameCount), effect_gain,
                   "loaded effect gain");
        check_gain(music_gains.at(music.get("song").frameCount), music_gain,
                   "loaded music gain");
    };

    for (bool master_first : {false, true}) {
        Audio::set_master_volume(1.f);
        Audio::set_sound_volume(1.f);
        Audio::set_music_volume(1.f);
        if (master_first) Audio::set_master_volume(0.5f);
        Audio::set_sound_volume(0.8f);
        Audio::set_music_volume(0.2f);
        if (!master_first) Audio::set_master_volume(0.5f);

        check_gain(Audio::get_sound_volume(), 0.8f, "effects preference");
        check_gain(Audio::get_music_volume(), 0.2f, "music preference");
        check_gain(Audio::get_master_volume(), 0.5f, "master preference");
        check_loaded(0.4f, 0.1f);

        Audio::set_master_volume(0.f);
        sounds.update_volume(0.6f);
        music.update_volume(0.3f);
        check_loaded(0.f, 0.f);
        check_gain(Audio::get_sound_volume(), 0.6f, "muted effects preference");
        check_gain(Audio::get_music_volume(), 0.3f, "muted music preference");
        check_gain(Audio::get_master_volume(), 0.f, "master stays muted");

        Audio::set_master_volume(0.5f);
        check_loaded(0.3f, 0.15f);
        Audio::set_master_volume(1.f);
        check_loaded(0.6f, 0.3f);
    }

    Audio::set_master_volume(0.25f);
    sounds.load("late.wav", "late");
    music.load("late.ogg", "late");
    check_gain(sound_gains.at(sounds.get("late").frameCount), 0.15f,
               "late effect inherits combined gain");
    check_gain(music_gains.at(music.get("late").frameCount), 0.075f,
               "late music inherits combined gain");

    Audio::set_master_volume(0.f);
    sounds.load("muted.wav", "muted");
    music.load("muted.ogg", "muted");
    check_gain(sound_gains.at(sounds.get("muted").frameCount), 0.f,
               "effect loaded while muted");
    check_gain(music_gains.at(music.get("muted").frameCount), 0.f,
               "music loaded while muted");

    Audio::set_master_volume(0.5f);
    for (const auto &[id, gain] : sound_gains) {
        (void)id;
        check_gain(gain, 0.3f, "all effects restore combined gain after mute");
    }
    for (const auto &[id, gain] : music_gains) {
        (void)id;
        check_gain(gain, 0.15f, "all music restores combined gain after mute");
    }

    sounds.unload_all();
    music.unload_all();
    std::printf("%d/%d sound volume checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
