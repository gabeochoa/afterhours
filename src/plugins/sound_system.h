#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"
#include "../library.h"
#include "../singleton.h"

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using SoundType = raylib::Sound;
using MusicType = raylib::Music;

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

inline void PlayMusicStream(raylib::Music music) {
  raylib::PlayMusicStream(music);
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
// Stubs for non-raylib builds
struct SoundStub {};
struct MusicStub {};
using SoundType = SoundStub;
using MusicType = MusicStub;

inline void PlaySound(SoundStub) {}
inline bool IsSoundPlaying(SoundStub) { return false; }
inline void SetSoundVolume(SoundStub, float) {}
inline void UnloadSound(SoundStub) {}
inline SoundStub LoadSound(const char *) { return SoundStub{}; }

inline void PlayMusicStream(MusicStub) {}
inline void UpdateMusicStream(MusicStub) {}
inline void SetMusicVolume(MusicStub, float) {}
inline void UnloadMusicStream(MusicStub) {}
inline MusicStub LoadMusicStream(const char *) { return MusicStub{}; }
#endif

struct sound_system : developer::Plugin {
  // Games should define their own SoundFile enum and provide a conversion
  // function Example:
  //   enum struct MySoundFile { UI_Click, Jump, Shoot };
  //   const char* my_sound_to_str(MySoundFile sf) { ... }
  // Then use: sound_system::play_sound<MySoundFile>(MySoundFile::UI_Click,
  // my_sound_to_str);

  SINGLETON_FWD(SoundLibrary)
  struct SoundLibrary {
    SINGLETON(SoundLibrary)

    [[nodiscard]] SoundType &get(const std::string &name) {
      return impl.get(name);
    }
    [[nodiscard]] const SoundType &get(const std::string &name) const {
      return impl.get(name);
    }
    [[nodiscard]] bool contains(const std::string &name) const {
      return impl.contains(name);
    }
    void load(const char *filename, const char *name) {
      impl.load(filename, name);
    }

    void play(const char *const name) { ::afterhours::PlaySound(get(name)); }

    void play_random_match(const std::string &prefix) {
      impl.get_random_match(prefix).transform(::afterhours::PlaySound);
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
      impl.update_volume(new_v);
      current_volume = new_v;
    }

    void unload_all() { impl.unload_all(); }

    [[nodiscard]] float get_volume() const { return current_volume; }

  private:
    float current_volume = 1.f;

    struct SoundLibraryImpl : Library<SoundType> {
      virtual SoundType
      convert_filename_to_object(const char *, const char *filename) override {
        return ::afterhours::LoadSound(filename);
      }
      virtual void unload(SoundType sound) override {
        ::afterhours::UnloadSound(sound);
      }

      void update_volume(const float new_v) {
        for (const auto &kv : storage) {
          log_trace("updating sound volume for {} to {}", kv.first, new_v);
          ::afterhours::SetSoundVolume(kv.second, new_v);
        }
      }
    } impl;
  };

  // Music library for streaming music
  SINGLETON_FWD(MusicLibrary)
  struct MusicLibrary {
    SINGLETON(MusicLibrary)

    [[nodiscard]] MusicType &get(const std::string &name) {
      return impl.get(name);
    }
    [[nodiscard]] const MusicType &get(const std::string &name) const {
      return impl.get(name);
    }
    [[nodiscard]] bool contains(const std::string &name) const {
      return impl.contains(name);
    }
    void load(const char *filename, const char *name) {
      impl.load(filename, name);
      // Update volume for newly loaded music
      update_volume(current_volume);
    }

    void play(const std::string &name) {
      auto &music = get(name);
      ::afterhours::PlayMusicStream(music);
    }

    void update_volume(const float new_v) {
      impl.update_volume(new_v);
      current_volume = new_v;
    }

    void unload_all() { impl.unload_all(); }

    [[nodiscard]] float get_volume() const { return current_volume; }

    void update() {
      // Update all music streams (required for streaming music)
      for (const auto &kv : impl.storage) {
        ::afterhours::UpdateMusicStream(kv.second);
      }
    }

  private:
    float current_volume = 1.f;

    struct MusicLibraryImpl : Library<MusicType> {
      virtual MusicType
      convert_filename_to_object(const char *, const char *filename) override {
        return ::afterhours::LoadMusicStream(filename);
      }
      virtual void unload(MusicType music) override {
        ::afterhours::UnloadMusicStream(music);
      }

      void update_volume(const float new_v) {
        for (const auto &kv : storage) {
          log_trace("updating music volume for {} to {}", kv.first, new_v);
          ::afterhours::SetMusicVolume(kv.second, new_v);
        }
      }
    } impl;
  };

  struct SoundEmitter : BaseComponent {
    int default_alias_copies = 4;
    std::map<std::string, std::vector<std::string>> alias_names_by_base;
    std::map<std::string, size_t> next_alias_index_by_base;
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
      SoundEmitter *emitter = EntityHelper::get_singleton_cmp<SoundEmitter>();
      switch (req.policy) {
      case PlaySoundRequest::Policy::Name: {
        play_with_alias_or_name(emitter, req.name.c_str(), req.prefer_alias);
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
      if (!emitter)
        return;
      if (emitter->alias_names_by_base.contains(base))
        return;
      std::vector<std::string> names;
      names.reserve(static_cast<size_t>(copies));
      for (int i = 0; i < copies; ++i) {
        names.push_back(base + std::string("_a") + std::to_string(i));
      }
      emitter->alias_names_by_base[base] = std::move(names);
      emitter->next_alias_index_by_base[base] = 0;
    }

    static bool library_contains(const std::string &name) {
      return SoundLibrary::get().contains(name);
    }

    static void play_with_alias_or_name(SoundEmitter *emitter, const char *name,
                                        bool prefer_alias) {
      const std::string base{name};

      if (prefer_alias && emitter) {
        ensure_alias_names(emitter, base, emitter->default_alias_copies);
        auto &names = emitter->alias_names_by_base[base];
        auto &idx = emitter->next_alias_index_by_base[base];

        for (size_t i = 0; i < names.size(); ++i) {
          size_t try_index = (idx + i) % names.size();
          const std::string &alias_name = names[try_index];
          if (library_contains(alias_name)) {
            auto &s = SoundLibrary::get().get(alias_name);
            if (!::afterhours::IsSoundPlaying(s)) {
              ::afterhours::PlaySound(s);
              idx = (try_index + 1) % names.size();
              return;
            }
          }
        }
        try {
          auto &s = SoundLibrary::get().get(base);
          ::afterhours::PlaySound(s);
        } catch (...) {
          SoundLibrary::get().play(name);
        }
        idx = (idx + 1) % names.size();
        return;
      }

      try {
        auto &s = SoundLibrary::get().get(base);
        ::afterhours::PlaySound(s);
      } catch (...) {
        SoundLibrary::get().play(name);
      }
    }
  };

  // System to update music streams
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

  // Helper function to play a sound by name
  template <typename SoundFileEnum>
  static void play_sound(SoundFileEnum sound_file,
                         const char *(*sound_to_str)(SoundFileEnum)) {
    const char *name = sound_to_str(sound_file);
    SoundLibrary::get().play(name);
  }

  // Helper function to request a sound via component
  template <typename SoundFileEnum>
  static void request_sound(Entity &entity, SoundFileEnum sound_file,
                            const char *(*sound_to_str)(SoundFileEnum)) {
    const char *name = sound_to_str(sound_file);
    entity.addComponent<PlaySoundRequest>(name);
  }

  // Volume management helpers
  static void set_master_volume(float volume) {
    // Master volume affects both sounds and music
    SoundLibrary::get().update_volume(volume);
    MusicLibrary::get().update_volume(volume);
  }

  static void set_sound_volume(float volume) {
    SoundLibrary::get().update_volume(volume);
  }

  static void set_music_volume(float volume) {
    MusicLibrary::get().update_volume(volume);
  }

  static float get_sound_volume() { return SoundLibrary::get().get_volume(); }

  static float get_music_volume() { return MusicLibrary::get().get_volume(); }
};

} // namespace afterhours
