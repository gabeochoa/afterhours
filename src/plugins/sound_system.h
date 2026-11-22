#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"
#include "../library.h"
#include "../singleton.h"
#include "audio_helpers.h"

namespace afterhours {

struct sound_system : developer::Plugin {
    struct PlayRecord {
        std::string name;
        std::string bus;
        float volume = 1.f;
        float pan = 0.5f;
        float pitch = 1.f;
        std::optional<Vector3Type> position;
    };

    struct Listener {
        Vector3Type position;
        Vector3Type forward;
        Vector3Type up;
        Listener()
            : position{0.f, 0.f, 0.f}, forward{0.f, 0.f, -1.f}, up{0.f, 1.f, 0.f} {}
        Listener(Vector3Type pos, Vector3Type fwd,
                 Vector3Type up_vec = Vector3Type{0.f, 1.f, 0.f})
            : position(pos), forward(fwd), up(up_vec) {}
    };

    struct SpatialGain {
        float volume = 0.f;
        float pan = 0.5f;
        float distance = 0.f;
    };

    static SpatialGain spatial_gain(const Listener &listener,
                                    const Vector3Type &source,
                                    float max_distance,
                                    float reference_distance = 1.f) {
        SpatialGain out;
        const float dx = source.x - listener.position.x;
        const float dy = source.y - listener.position.y;
        const float dz = source.z - listener.position.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        out.distance = dist;
        if (max_distance <= 0.f || dist >= max_distance) {
            out.volume = 0.f;
        } else if (dist <= reference_distance) {
            out.volume = 1.f;
        } else {
            out.volume = reference_distance / dist;
        }
        float rx = listener.forward.y * listener.up.z -
                   listener.forward.z * listener.up.y;
        float ry = listener.forward.z * listener.up.x -
                   listener.forward.x * listener.up.z;
        float rz = listener.forward.x * listener.up.y -
                   listener.forward.y * listener.up.x;
        const float rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (dist > 0.f && rlen > 0.f) {
            rx /= rlen; ry /= rlen; rz /= rlen;
            const float side = (dx * rx + dy * ry + dz * rz) / dist;
            out.pan = std::clamp(0.5f + 0.5f * side, 0.f, 1.f);
        }
        return out;
    }

    SINGLETON_CLASS_FWD(SoundLibrary)
    struct SoundLibrary {
        SINGLETON(SoundLibrary)

        [[nodiscard]] SoundType &get(const std::string &name) {
            return impl.get(name);
        }
        [[nodiscard]] const SoundType &get(const std::string &name) const {
            return impl.get(name);
        }
        using Handle = Library<SoundType>::Handle;
        [[nodiscard]] Handle intern(const std::string &name) {
            return impl.intern(name);
        }
        [[nodiscard]] SoundType &get(Handle handle) {
            return impl.get(handle);
        }
        [[nodiscard]] const SoundType &get(Handle handle) const {
            return impl.get(handle);
        }
        [[nodiscard]] bool contains(Handle handle) const {
            return impl.contains(handle);
        }
        [[nodiscard]] bool contains(const std::string &name) const {
            return impl.contains(name);
        }

        [[nodiscard]] std::string bus_of(const std::string &name) const {
            auto it = bus_by_name.find(name);
            return it == bus_by_name.end() ? std::string{"effects"}
                                           : it->second;
        }
        void set_bus(const std::string &name, const std::string &bus) {
            bus_by_name[name] = bus;
            if (contains(name)) apply_volume(name);
        }
        void set_bus_volume(const std::string &bus, float volume) {
            bus_volumes[bus] = std::max(0.f, volume);
            for (const auto &[name, b] : bus_by_name)
                if (b == bus && contains(name)) apply_volume(name);
            if (bus == "effects") {
                for (const auto &kv : impl.storage)
                    if (!bus_by_name.contains(kv.first)) apply_volume(kv.first);
            }
        }
        [[nodiscard]] float get_bus_volume(const std::string &bus) const {
            auto it = bus_volumes.find(bus);
            return it == bus_volumes.end() ? 1.f : it->second;
        }
        [[nodiscard]] float effective_volume(const std::string &name) const {
            return current_volume * get_master_volume() *
                   get_bus_volume(bus_of(name));
        }
        void apply_volume(const std::string &name) {
            ::afterhours::SetSoundVolume(get(name), effective_volume(name));
        }

        void load(const char *filename, const char *name) {
            load_on_bus(filename, name, bus_of(name));
        }
        void load_on_bus(const char *filename, const char *name,
                         const std::string &bus) {
            const bool was_present = contains(name);
            impl.load(filename, name);
            if (!was_present && contains(name)) bus_by_name[name] = bus;
            if (contains(name)) apply_volume(name);
        }
        bool add(const std::string &name, SoundType sound,
                 const std::string &bus = "effects") {
            auto res = impl.add(name.c_str(), sound);
            if (!res) {
                log_warn("SoundLibrary::add: '{}' is already registered",
                         name.c_str());
                return false;
            }
            bus_by_name[name] = bus;
            apply_volume(name);
            return true;
        }
        bool add_samples(const std::string &name,
                         std::span<const float> samples, int sample_rate,
                         int channels = 1,
                         const std::string &bus = "effects") {
            return add(name,
                       load_sound_from_samples(samples, sample_rate, channels),
                       bus);
        }
        void load_pool(const char *filename, const char *name, int copies,
                       const std::string &bus = "effects") {
            load_on_bus(filename, name, bus);
            if (!contains(name)) return;
            for (int i = 0; i < copies; ++i) {
                const std::string alias =
                    std::string{name} + "_a" + std::to_string(i);
                if (contains(alias)) continue;
                auto res = impl.add(alias.c_str(),
                                    ::afterhours::LoadSoundAlias(get(name)));
                if (res) {
                    bus_by_name[alias] = bus;
                    apply_volume(alias);
                }
            }
        }

        void set_range(const std::string &name, float max_distance) {
            range_by_name[name] = max_distance;
        }
        [[nodiscard]] float range_of(const std::string &name,
                                     float fallback) const {
            auto it = range_by_name.find(name);
            return it == range_by_name.end() ? fallback : it->second;
        }

        void play(const char *const name) { play_loaded(name); }

        void play_loaded(const std::string &name, float volume_scale = 1.f,
                         float pan = 0.5f, float pitch = 1.f,
                         bool positional = false,
                         Vector3Type position = Vector3Type{0.f, 0.f, 0.f}) {
            if (!contains(name)) {
                log_warn("SoundLibrary::play: no sound loaded named '{}'",
                         name.c_str());
                return;
            }
            auto &sound = get(name);
            if (positional) {
                ::afterhours::SetSoundVolume(
                    sound, effective_volume(name) * volume_scale);
                ::afterhours::SetSoundPan(sound, pan);
                ::afterhours::SetSoundPitch(sound, pitch);
            }
            ::afterhours::PlaySound(sound);
            record_play(PlayRecord{name, bus_of(name),
                                   effective_volume(name) * volume_scale, pan,
                                   pitch,
                                   positional ? std::optional<Vector3Type>(position)
                                              : std::nullopt});
        }

        void stop(const std::string &name) {
            if (contains(name)) ::afterhours::StopSound(get(name));
        }
        void pause(const std::string &name) {
            if (contains(name)) ::afterhours::PauseSound(get(name));
        }
        void resume(const std::string &name) {
            if (contains(name)) ::afterhours::ResumeSound(get(name));
        }
        [[nodiscard]] bool is_playing(const std::string &name) {
            return contains(name) && ::afterhours::IsSoundPlaying(get(name));
        }

        void play_random_match(const std::string &prefix) {
            auto match = impl.get_random_match(prefix);
            if (match.has_value()) {
                ::afterhours::PlaySound(match.value());
            }
        }

        void play_if_none_playing(const std::string &prefix) {
            auto matches = impl.lookup(prefix);
            if (matches.first == matches.second) {
                log_warn("got no matches for your prefix search: {}", prefix);
                return;
            }
            for (auto it = matches.first; it != matches.second; ++it) {
                if (::afterhours::IsSoundPlaying(it->second)) {
                    return;
                }
            }
            ::afterhours::PlaySound(matches.first->second);
        }

        void play_first_available_match(const std::string &prefix) {
            auto matches = impl.lookup(prefix);
            if (matches.first == matches.second) {
                log_warn("got no matches for your prefix search: {}", prefix);
                return;
            }
            for (auto it = matches.first; it != matches.second; ++it) {
                if (!::afterhours::IsSoundPlaying(it->second)) {
                    ::afterhours::PlaySound(it->second);
                    return;
                }
            }
            ::afterhours::PlaySound(matches.first->second);
        }

        void update_volume(const float new_v) {
            current_volume = new_v;
            for (const auto &kv : impl.storage) apply_volume(kv.first);
        }

        void unload_all() {
            impl.unload_all();
            bus_by_name.clear();
            range_by_name.clear();
        }

        [[nodiscard]] float get_volume() const { return current_volume; }

       private:
        float current_volume = 1.f;
        std::map<std::string, std::string> bus_by_name;
        std::map<std::string, float> bus_volumes;
        std::map<std::string, float> range_by_name;

        struct SoundLibraryImpl : Library<SoundType> {
            virtual SoundType convert_filename_to_object(
                const char *, const char *filename) override {
                return ::afterhours::LoadSound(filename);
            }
            virtual void unload(SoundType sound) override {
                ::afterhours::UnloadSound(sound);
            }
        } impl;
    };

    SINGLETON_CLASS_FWD(MusicLibrary)
    struct MusicLibrary {
        SINGLETON(MusicLibrary)

        [[nodiscard]] MusicType &get(const std::string &name) {
            return impl.get(name);
        }
        [[nodiscard]] const MusicType &get(const std::string &name) const {
            return impl.get(name);
        }
        using Handle = Library<MusicType>::Handle;
        [[nodiscard]] Handle intern(const std::string &name) {
            return impl.intern(name);
        }
        [[nodiscard]] MusicType &get(Handle handle) {
            return impl.get(handle);
        }
        [[nodiscard]] const MusicType &get(Handle handle) const {
            return impl.get(handle);
        }
        [[nodiscard]] bool contains(Handle handle) const {
            return impl.contains(handle);
        }
        [[nodiscard]] bool contains(const std::string &name) const {
            return impl.contains(name);
        }
        [[nodiscard]] std::string bus_of(const std::string &name) const {
            auto it = bus_by_name.find(name);
            return it == bus_by_name.end() ? std::string{"music"} : it->second;
        }
        void set_bus_volume(const std::string &bus, float volume) {
            bus_volumes[bus] = std::max(0.f, volume);
            for (const auto &kv : impl.storage)
                if (bus_of(kv.first) == bus) apply_volume(kv.first);
        }
        [[nodiscard]] float get_bus_volume(const std::string &bus) const {
            auto it = bus_volumes.find(bus);
            return it == bus_volumes.end() ? 1.f : it->second;
        }
        [[nodiscard]] float effective_volume(const std::string &name) const {
            return current_volume * get_master_volume() *
                   get_bus_volume(bus_of(name));
        }
        void apply_volume(const std::string &name) {
            ::afterhours::SetMusicVolume(get(name), effective_volume(name));
        }
        void load(const char *filename, const char *name) {
            load_on_bus(filename, name, bus_of(name));
        }
        void load_on_bus(const char *filename, const char *name,
                         const std::string &bus) {
            const bool was_present = contains(name);
            impl.load(filename, name);
            if (!was_present && contains(name)) bus_by_name[name] = bus;
            if (contains(name)) apply_volume(name);
        }
        bool add(const std::string &name, MusicType music,
                 const std::string &bus = "music") {
            auto res = impl.add(name.c_str(), music);
            if (!res) {
                log_warn("MusicLibrary::add: '{}' is already registered",
                         name.c_str());
                return false;
            }
            bus_by_name[name] = bus;
            apply_volume(name);
            return true;
        }

        void play(const std::string &name) {
            if (!contains(name)) {
                log_warn("MusicLibrary::play: no music loaded named '{}'",
                         name.c_str());
                return;
            }
            auto &music = get(name);
            ::afterhours::PlayMusicStream(music);
            record_play(PlayRecord{name, bus_of(name), effective_volume(name),
                                   0.5f, 1.f, std::nullopt});
        }
        void stop(const std::string &name) {
            if (contains(name)) ::afterhours::StopMusicStream(get(name));
        }

        void update_volume(const float new_v) {
            current_volume = new_v;
            for (const auto &kv : impl.storage) apply_volume(kv.first);
        }

        void unload_all() {
            impl.unload_all();
            bus_by_name.clear();
        }

        [[nodiscard]] float get_volume() const { return current_volume; }

        void update() {
            for (const auto &kv : impl.storage) {
                ::afterhours::UpdateMusicStream(kv.second);
            }
        }

       private:
        float current_volume = 1.f;
        std::map<std::string, std::string> bus_by_name;
        std::map<std::string, float> bus_volumes;

        struct MusicLibraryImpl : Library<MusicType> {
            virtual MusicType convert_filename_to_object(
                const char *, const char *filename) override {
                return ::afterhours::LoadMusicStream(filename);
            }
            virtual void unload(MusicType music) override {
                ::afterhours::UnloadMusicStream(music);
            }
        } impl;
    };

    struct SoundEmitter : BaseComponent {
        int default_alias_copies = 4;
        struct AliasState {
            std::vector<std::string> names;
            size_t next_index = 0;
        };
        std::map<std::string, AliasState> alias_state_by_base;
    };

    struct PlaySoundRequest : BaseComponent {
        enum class Policy {
            Name,
            PrefixRandom,
            PrefixFirstAvailable,
            PrefixIfNonePlaying
        };

        Policy policy{Policy::Name};
        std::string name;
        std::string prefix;
        bool prefer_alias{true};

        PlaySoundRequest() = default;
        explicit PlaySoundRequest(const std::string &sound_name)
            : policy(Policy::Name), name(sound_name), prefer_alias(true) {}
        explicit PlaySoundRequest(const std::string &prefix, Policy policy_type)
            : policy(policy_type), prefix(prefix), prefer_alias(true) {}
    };

    struct SoundPlaybackSystem : System<PlaySoundRequest> {
        virtual void for_each_with(Entity &entity, PlaySoundRequest &req,
                                   float) override {
            SoundEmitter *emitter =
                EntityHelper::get_singleton_cmp<SoundEmitter>();
            switch (req.policy) {
                case PlaySoundRequest::Policy::Name: {
                    play_with_alias_or_name(emitter, req.name.c_str(),
                                            req.prefer_alias);
                } break;
                case PlaySoundRequest::Policy::PrefixRandom: {
                    SoundLibrary::get().play_random_match(req.prefix);
                } break;
                case PlaySoundRequest::Policy::PrefixFirstAvailable: {
                    SoundLibrary::get().play_first_available_match(req.prefix);
                } break;
                case PlaySoundRequest::Policy::PrefixIfNonePlaying: {
                    SoundLibrary::get().play_if_none_playing(req.prefix);
                } break;
            }

            entity.removeComponent<PlaySoundRequest>();
        }

        static void ensure_alias_names(SoundEmitter *emitter,
                                       const std::string &base, int copies) {
            if (!emitter) return;
            if (emitter->alias_state_by_base.contains(base)) return;
            SoundEmitter::AliasState state;
            state.names.reserve(static_cast<size_t>(copies));
            for (int i = 0; i < copies; ++i) {
                state.names.push_back(base + std::string("_a") +
                                      std::to_string(i));
            }
            emitter->alias_state_by_base[base] = std::move(state);
        }

        static bool library_contains(const std::string &name) {
            return SoundLibrary::get().contains(name);
        }

        static void play_with_alias_or_name(SoundEmitter *emitter,
                                            const char *name,
                                            bool prefer_alias) {
            SoundLibrary &lib = SoundLibrary::get();
            const std::string base{name};

            if (prefer_alias && emitter) {
                ensure_alias_names(emitter, base,
                                   emitter->default_alias_copies);
                SoundEmitter::AliasState &state =
                    emitter->alias_state_by_base[base];
                auto &names = state.names;
                auto &idx = state.next_index;

                for (size_t i = 0; i < names.size(); ++i) {
                    size_t try_index = (idx + i) % names.size();
                    const std::string &alias_name = names[try_index];
                    if (lib.contains(alias_name)) {
                        auto &s = lib.get(alias_name);
                        if (!::afterhours::IsSoundPlaying(s)) {
                            lib.play_loaded(alias_name);
                            idx = (try_index + 1) % names.size();
                            return;
                        }
                    }
                }
                if (lib.contains(base)) {
                    lib.play_loaded(base);
                } else {
                    lib.play(name);
                }
                idx = (idx + 1) % names.size();
                return;
            }

            if (lib.contains(base)) {
                lib.play_loaded(base);
            } else {
                lib.play(name);
            }
        }
    };

    struct MusicUpdateSystem : System<> {
        virtual void for_each_with(Entity &, float) override {
            MusicLibrary::get().update();
        }
    };

    static void add_singleton_components(Entity &entity) {
        entity.addComponent<SoundEmitter>();
        EntityHelper::registerSingleton<SoundEmitter>(entity);
    }

    static void enforce_singletons(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<developer::EnforceSingleton<SoundEmitter>>());
    }

    static void register_update_systems(SystemManager &sm) {
        sm.register_update_system(std::make_unique<SoundPlaybackSystem>());
        sm.register_update_system(std::make_unique<MusicUpdateSystem>());
    }

    template<typename SoundFileEnum>
    static void play_sound(SoundFileEnum sound_file,
                           const char *(*sound_to_str)(SoundFileEnum)) {
        const char *name = sound_to_str(sound_file);
        SoundLibrary::get().play(name);
    }

    template<typename SoundFileEnum>
    static void request_sound(Entity &entity, SoundFileEnum sound_file,
                              const char *(*sound_to_str)(SoundFileEnum)) {
        const char *name = sound_to_str(sound_file);
        entity.addComponent<PlaySoundRequest>(name);
    }

    static void set_listener(const Listener &listener) { listener_ = listener; }
    static void set_listener_position(Vector3Type position,
                                      Vector3Type forward = Vector3Type{0.f, 0.f, -1.f}) {
        listener_.position = position;
        listener_.forward = forward;
    }
    [[nodiscard]] static const Listener &listener() { return listener_; }

    static bool play_at(const char *name, Vector3Type position,
                        float base_volume = 1.f,
                        float default_max_distance = 50.f,
                        float pitch = 1.f) {
        auto &lib = SoundLibrary::get();
        if (!lib.contains(name)) {
            log_warn("sound_system::play_at: no sound loaded named '{}'", name);
            return false;
        }
        const float max_dist = lib.range_of(name, default_max_distance);
        const SpatialGain g = spatial_gain(listener_, position, max_dist);
        if (g.volume <= 0.f) return false;
        lib.play_loaded(name, base_volume * g.volume, g.pan, pitch, true,
                        position);
        return true;
    }

    static void set_bus_volume(const std::string &bus, float volume) {
        SoundLibrary::get().set_bus_volume(bus, volume);
        MusicLibrary::get().set_bus_volume(bus, volume);
    }
    [[nodiscard]] static float get_bus_volume(const std::string &bus) {
        return SoundLibrary::get().get_bus_volume(bus);
    }

    static void set_recording(bool on) { recording_ = on; }
    [[nodiscard]] static bool recording() { return recording_; }
    [[nodiscard]] static const std::vector<PlayRecord> &play_log() {
        return play_log_;
    }
    static void clear_play_log() { play_log_.clear(); }
    [[nodiscard]] static int play_count(const std::string &name) {
        int n = 0;
        for (const auto &r : play_log_)
            if (r.name == name) ++n;
        return n;
    }
    [[nodiscard]] static bool expect_sound_played(const std::string &name) {
        return play_count(name) > 0;
    }
    static void record_play(const PlayRecord &rec) {
        if (recording_) play_log_.push_back(rec);
    }

    // Volumes are independent preferences; backend gains are master * category.
    static void set_master_volume(float volume) {
        master_volume = volume;
        auto &sounds = SoundLibrary::get();
        auto &music = MusicLibrary::get();
        sounds.update_volume(sounds.get_volume());
        music.update_volume(music.get_volume());
    }

    static void set_sound_volume(float volume) {
        SoundLibrary::get().update_volume(volume);
    }

    static void set_music_volume(float volume) {
        MusicLibrary::get().update_volume(volume);
    }

    static float get_sound_volume() { return SoundLibrary::get().get_volume(); }

    static float get_music_volume() { return MusicLibrary::get().get_volume(); }

    static float get_master_volume() { return master_volume; }

   private:
    inline static float master_volume = 1.f;
    inline static Listener listener_{};
    inline static bool recording_ = false;
    inline static std::vector<PlayRecord> play_log_{};
};

// Compile-time verification that sound_system satisfies the PluginCore concept
static_assert(developer::PluginCore<sound_system>,
              "sound_system must implement the core plugin interface");

}  // namespace afterhours
