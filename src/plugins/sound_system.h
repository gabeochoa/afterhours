#pragma once

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"
#include "../library.h"
#include "../singleton.h"
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <string>
#include <vector>

namespace afterhours {

struct sound_system : developer::Plugin {

  enum struct SoundFile {
    UI_Select,
    UI_Move,
    Engine_Idle_Short,
    Round_Start,
    Tiny_Gears_Sequence_045,
    Weapon_Sniper_Shot,
    Weapon_Canon_Shot,
    Weapon_Shotgun_Shot,
  };

  static constexpr const char *sound_file_to_str(SoundFile sf) {
    switch (sf) {
    case SoundFile::UI_Select:
      return "UISelect";
    case SoundFile::UI_Move:
      return "WaterDripSingle";
    case SoundFile::Engine_Idle_Short:
      return "EngineIdleShort";
    case SoundFile::Round_Start:
      return "RoundStart";
    case SoundFile::Tiny_Gears_Sequence_045:
      return "TinyGearsSequence045";
    case SoundFile::Weapon_Sniper_Shot:
      return "WeaponSniperShot";
    case SoundFile::Weapon_Canon_Shot:
      return "WeaponCanonShot";
    case SoundFile::Weapon_Shotgun_Shot:
      return "WeaponShotgunShot";
    }
    return "";
  }

  SINGLETON_FWD(SoundLibrary)
  struct SoundLibrary {
    SINGLETON(SoundLibrary)

    [[nodiscard]] raylib::Sound &get(const std::string &name) {
      return impl.get(name);
    }
    [[nodiscard]] const raylib::Sound &get(const std::string &name) const {
      return impl.get(name);
    }
    [[nodiscard]] bool contains(const std::string &name) const {
      return impl.contains(name);
    }
    void load(const char *filename, const char *name) {
      impl.load(filename, name);
    }

    void play(SoundFile file) { play(sound_file_to_str(file)); }
    void play(const char *const name) { PlaySound(get(name)); }

    void play_random_match(const std::string &prefix) {
      impl.get_random_match(prefix).transform(raylib::PlaySound);
    }

    void play_if_none_playing(const std::string &prefix) {
      auto matches = impl.lookup(prefix);
      if (matches.first == matches.second) {
        log_warn("got no matches for your prefix search: {}", prefix);
        return;
      }
      for (auto it = matches.first; it != matches.second; ++it) {
        if (raylib::IsSoundPlaying(it->second)) {
          return;
        }
      }
      raylib::PlaySound(matches.first->second);
    }

    void play_first_available_match(const std::string &prefix) {
      auto matches = impl.lookup(prefix);
      if (matches.first == matches.second) {
        log_warn("got no matches for your prefix search: {}", prefix);
        return;
      }
      for (auto it = matches.first; it != matches.second; ++it) {
        if (!raylib::IsSoundPlaying(it->second)) {
          raylib::PlaySound(it->second);
          return;
        }
      }
      raylib::PlaySound(matches.first->second);
    }

    void update_volume(const float new_v) {
      impl.update_volume(new_v);
      current_volume = new_v;
    }

    void unload_all() { impl.unload_all(); }

  private:
    float current_volume = 1.f;

    struct SoundLibraryImpl : Library<raylib::Sound> {
      virtual raylib::Sound
      convert_filename_to_object(const char *, const char *filename) override {
        return raylib::LoadSound(filename);
      }
      virtual void unload(raylib::Sound sound) override {
        raylib::UnloadSound(sound);
      }

      void update_volume(const float new_v) {
        for (const auto &kv : storage) {
          log_trace("updating sound volume for {} to {}", kv.first, new_v);
          raylib::SetSoundVolume(kv.second, new_v);
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
      Enum,
      PrefixRandom,
      PrefixFirstAvailable,
      PrefixIfNonePlaying
    };

    Policy policy{Policy::Name};
    std::string name;
    SoundFile file{SoundFile::UI_Move};
    std::string prefix;
    bool prefer_alias{true};

    PlaySoundRequest() = default;
    explicit PlaySoundRequest(SoundFile f)
        : policy(Policy::Enum), file(f), prefer_alias(true) {}
  };

  struct SoundPlaybackSystem : System<PlaySoundRequest> {
    virtual void for_each_with(Entity &entity, PlaySoundRequest &req,
                               float) override {
      SoundEmitter *emitter = EntityHelper::get_singleton_cmp<SoundEmitter>();
      switch (req.policy) {
      case PlaySoundRequest::Policy::Enum: {
        const char *name = sound_file_to_str(req.file);
        play_with_alias_or_name(emitter, name, req.prefer_alias);
      } break;
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

    static void ensure_alias_names(SoundEmitter *emitter, const std::string &base,
                                   int copies) {
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
            if (!raylib::IsSoundPlaying(s)) {
              raylib::PlaySound(s);
              idx = (try_index + 1) % names.size();
              return;
            }
          }
        }
        try {
          auto &s = SoundLibrary::get().get(base);
          raylib::PlaySound(s);
        } catch (...) {
          SoundLibrary::get().play(name);
        }
        idx = (idx + 1) % names.size();
        return;
      }

      try {
        auto &s = SoundLibrary::get().get(base);
        raylib::PlaySound(s);
      } catch (...) {
        SoundLibrary::get().play(name);
      }
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
  }
};

} // namespace afterhours

