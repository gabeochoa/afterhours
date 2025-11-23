#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"
#include "files.h"

// Compile-time check: ensure settings.cpp is compiled in your project
// If you see a linker error, make sure to compile vendor/afterhours/src/plugins/settings.cpp
#ifndef AFTERHOURS_SETTINGS_CPP_COMPILED
// This will cause a linker error if settings.cpp is not compiled
// The error message will guide you to compile the .cpp file
namespace afterhours {
namespace settings_plugin_internal {
void _require_settings_cpp_compiled();
}  // namespace settings_plugin_internal
}  // namespace afterhours
#endif

// Settings serialization format selection
// Define one of:
//   #define AFTERHOURS_SETTINGS_OUTPUT_JSON
//   #define AFTERHOURS_SETTINGS_OUTPUT_BITSERY
//   #define AFTERHOURS_SETTINGS_OUTPUT_RAW_STRING
// Default is JSON if none specified

// JSON library - games should include nlohmann/json.hpp before this file
// or define their own JSON type
#if defined(AFTERHOURS_SETTINGS_OUTPUT_JSON) ||      \
    (!defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY) && \
     !defined(AFTERHOURS_SETTINGS_OUTPUT_RAW_STRING))
#ifndef JSON_TYPE
#include "../../../nlohmann/json.hpp"
using json = nlohmann::json;
#else
using json = JSON_TYPE;
#endif
#endif

// Bitsery support - games can define AFTER_HOURS_USE_BITSERY and include
// bitsery headers before this file
#if defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
#ifndef AFTER_HOURS_USE_BITSERY
#error \
    "AFTER_HOURS_USE_BITSERY must be defined when using bitsery output format"
#endif
#ifndef BITSERY_BITSERY_H
#error \
    "Bitsery headers must be included before settings.h when using bitsery output format"
#endif
using BitsBuffer = std::string;
using BitsOutputAdapter = bitsery::OutputBufferAdapter<BitsBuffer>;
using BitsInputAdapter = bitsery::InputBufferAdapter<BitsBuffer>;
#endif

namespace afterhours {
namespace fs = std::filesystem;

struct settings : developer::Plugin {
    // Template-based settings component for game-specific data
    // SettingsData must have serialization methods based on selected format:
    //   Default (raw_string):
    //     - std::string to_string() const method
    //     - static SettingsData from_string(const std::string&) method
    //   JSON (if AFTERHOURS_SETTINGS_OUTPUT_JSON defined):
    //     - json to_json() const method (or nlohmann::json to_json(const
    //     SettingsData&) free function)
    //     - static SettingsData from_json(const json&) method (or free
    //     function)
    //   Bitsery (if AFTERHOURS_SETTINGS_OUTPUT_BITSERY defined):
    //     - Must be serializable with
    //     bitsery::quickSerialization/quickDeserialization
    template<typename SettingsData>
    struct ProvidesSettings : BaseComponent {
        SettingsData data;
        std::string settings_file;
        std::vector<std::function<void(const SettingsData &)>>
            on_change_callbacks;

        ProvidesSettings() = default;
        ProvidesSettings(const SettingsData &default_data,
                         const std::string &file)
            : data(default_data), settings_file(file) {}

        void trigger_callbacks() {
            for (const auto &callback : on_change_callbacks) {
                if (callback) {
                    callback(data);
                }
            }
        }
    };

    // Helper functions for loading/saving
    template<typename SettingsData>
    static bool load_from_json(ProvidesSettings<SettingsData> *provider,
                               const fs::path &settings_path) {
        json j;
        try {
            std::ifstream ifs(settings_path);
            if (!ifs.is_open()) {
                log_warn("Failed to open settings file: %s",
                         settings_path.string().c_str());
                return false;
            }
            ifs >> j;
            ifs.close();
            provider->data = SettingsData::from_json(j);
            log_info("Settings loaded from JSON: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to parse settings JSON: %s", e.what());
            return false;
        }
    }

#if defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
    template<typename SettingsData>
    static bool load_from_bitsery(ProvidesSettings<SettingsData> *provider,
                                  const fs::path &settings_path) {
        try {
            std::ifstream ifs(settings_path);
            if (!ifs.is_open()) {
                log_warn("Failed to open settings file: %s",
                         settings_path.string().c_str());
                return false;
            }
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            ifs.close();
            auto buf_str = buffer.str();
            bitsery::quickDeserialization<BitsInputAdapter>(
                {buf_str.begin(), buf_str.size()}, provider->data);
            log_info("Settings loaded from bitsery: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to parse settings bitsery: %s", e.what());
            return false;
        }
    }
#endif

    // Raw string format (default) - always available
    template<typename SettingsData>
    static bool load_from_raw_string(ProvidesSettings<SettingsData> *provider,
                                     const fs::path &settings_path) {
        try {
            std::ifstream ifs(settings_path);
            if (!ifs.is_open()) {
                log_warn("Failed to open settings file: %s",
                         settings_path.string().c_str());
                return false;
            }
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            ifs.close();
            std::string content = buffer.str();
            provider->data = SettingsData::from_string(content);
            log_info("Settings loaded from raw string: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to parse settings from raw string: %s", e.what());
            return false;
        }
    }

    template<typename SettingsData>
    static bool save_to_json(ProvidesSettings<SettingsData> *provider,
                             const fs::path &settings_path) {
        try {
            std::ofstream ofs(settings_path);
            if (!ofs.is_open()) {
                log_warn("Failed to open settings file for writing: %s",
                         settings_path.string().c_str());
                return false;
            }
            json j = provider->data.to_json();
            ofs << j.dump(2);  // Pretty print with 2-space indent
            ofs.close();
            log_info("Settings saved to JSON: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to serialize settings to JSON: %s", e.what());
            return false;
        }
    }

#if defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
    template<typename SettingsData>
    static bool save_to_bitsery(ProvidesSettings<SettingsData> *provider,
                                const fs::path &settings_path) {
        try {
            std::ofstream ofs(settings_path);
            if (!ofs.is_open()) {
                log_warn("Failed to open settings file for writing: %s",
                         settings_path.string().c_str());
                return false;
            }
            BitsBuffer buffer;
            bitsery::quickSerialization(BitsOutputAdapter{buffer},
                                        provider->data);
            ofs << buffer;
            ofs.close();
            log_info("Settings saved to bitsery: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to serialize settings with bitsery: %s",
                      e.what());
            return false;
        }
    }
#endif

    // Raw string format (default) - always available
    template<typename SettingsData>
    static bool save_to_raw_string(ProvidesSettings<SettingsData> *provider,
                                   const fs::path &settings_path) {
        try {
            std::ofstream ofs(settings_path);
            if (!ofs.is_open()) {
                log_warn("Failed to open settings file for writing: %s",
                         settings_path.string().c_str());
                return false;
            }
            std::string content = provider->data.to_string();
            ofs << content;
            ofs.close();
            log_info("Settings saved to raw string: %s",
                     settings_path.string().c_str());
            return true;
        } catch (const std::exception &e) {
            log_error("Failed to serialize settings to raw string: %s",
                      e.what());
            return false;
        }
    }

    // System to load settings on startup
    template<typename SettingsData>
    struct SettingsLoadSystem : System<ProvidesSettings<SettingsData>> {
        virtual void once(float) override {
            auto *provider = EntityHelper::get_singleton_cmp<
                ProvidesSettings<SettingsData>>();
            if (!provider) {
                return;
            }

            if (provider->settings_file.empty()) {
                return;
            }

            // Try to get save path from Files plugin if available
            fs::path settings_path;
            auto *files_provider =
                EntityHelper::get_singleton_cmp<files::ProvidesResourcePaths>();
            if (files_provider) {
                settings_path = files_provider->get_save_path() /
                                fs::path(provider->settings_file);
            } else {
                // Fallback to current directory
                settings_path =
                    fs::current_path() / fs::path(provider->settings_file);
            }

            std::ifstream ifs(settings_path);
            if (!ifs.is_open()) {
                log_warn("Failed to find settings file: %s",
                         settings_path.string().c_str());
                return;
            }

            log_trace("Reading settings file: %s",
                      settings_path.string().c_str());

#if defined(AFTERHOURS_SETTINGS_OUTPUT_JSON)
            load_from_json(provider, settings_path);
#elif defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
            load_from_bitsery(provider, settings_path);
#else
            // Default: raw_string
            load_from_raw_string(provider, settings_path);
#endif
        }
    };

    // System to save settings on change
    template<typename SettingsData>
    struct SettingsSaveSystem : System<ProvidesSettings<SettingsData>> {
        virtual void once(float) override {
            auto *provider = EntityHelper::get_singleton_cmp<
                ProvidesSettings<SettingsData>>();
            if (!provider) {
                return;
            }

            if (provider->settings_file.empty()) {
                return;
            }

            // Only save if data has changed (this is a simple implementation)
            // Games can call save() explicitly when needed
        }
    };

    template<typename SettingsData>
    static void add_singleton_components(Entity &entity,
                                         const SettingsData &default_data,
                                         const std::string &settings_file) {
        entity.addComponent<ProvidesSettings<SettingsData>>(default_data,
                                                            settings_file);
        EntityHelper::registerSingleton<ProvidesSettings<SettingsData>>(entity);
    }

    template<typename SettingsData>
    static void enforce_singletons(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<
                developer::EnforceSingleton<ProvidesSettings<SettingsData>>>());
    }

    template<typename SettingsData>
    static void register_update_systems(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<SettingsLoadSystem<SettingsData>>());
        sm.register_update_system(
            std::make_unique<SettingsSaveSystem<SettingsData>>());
    }

    // API methods
    template<typename SettingsData>
    static ProvidesSettings<SettingsData> *get_provider() {
        return EntityHelper::get_singleton_cmp<
            ProvidesSettings<SettingsData>>();
    }

    template<typename SettingsData>
    static void init(const std::string & /* game_name */,
                     const std::string &settings_file) {
        auto *provider = get_provider<SettingsData>();
        if (provider) {
            log_warn("Settings plugin already initialized");
            return;
        }

        // Create singleton entity with default-constructed settings data
        Entity &entity = EntityHelper::createPermanentEntity();
        SettingsData default_data;
        add_singleton_components<SettingsData>(entity, default_data,
                                               settings_file);
        EntityHelper::merge_entity_arrays();
    }

    template<typename SettingsData>
    static SettingsData &get_data() {
        auto *provider = get_provider<SettingsData>();
        if (!provider) {
            log_error(
                "Settings plugin not initialized. Call "
                "settings::init<SettingsData>() "
                "first.");
            static SettingsData fallback;
            return fallback;
        }
        return provider->data;
    }

    template<typename SettingsData>
    static const SettingsData &get_data_const() {
        auto *provider = get_provider<SettingsData>();
        if (!provider) {
            log_error(
                "Settings plugin not initialized. Call "
                "settings::init<SettingsData>() "
                "first.");
            static SettingsData fallback;
            return fallback;
        }
        return provider->data;
    }

    template<typename SettingsData>
    static void register_on_change(
        std::function<void(const SettingsData &)> callback) {
        auto *provider = get_provider<SettingsData>();
        if (!provider) {
            log_error(
                "Settings plugin not initialized. Call "
                "settings::init<SettingsData>() "
                "first.");
            return;
        }
        provider->on_change_callbacks.push_back(callback);
    }

    template<typename SettingsData>
    static bool load() {
        auto *provider = get_provider<SettingsData>();
        if (!provider) {
            log_error(
                "Settings plugin not initialized. Call "
                "settings::init<SettingsData>() "
                "first.");
            return false;
        }

        if (provider->settings_file.empty()) {
            log_warn("Settings file not specified");
            return false;
        }

        // Try to get save path from Files plugin if available
        fs::path settings_path;
        auto *files_provider =
            EntityHelper::get_singleton_cmp<files::ProvidesResourcePaths>();
        if (files_provider) {
            settings_path = files_provider->get_save_path() /
                            fs::path(provider->settings_file);
        } else {
            // Fallback to current directory
            settings_path =
                fs::current_path() / fs::path(provider->settings_file);
        }

        std::ifstream ifs(settings_path);
        if (!ifs.is_open()) {
            log_warn("Failed to find settings file: %s",
                     settings_path.string().c_str());
            return false;
        }

        log_trace("Reading settings file: %s", settings_path.string().c_str());

#if defined(AFTERHOURS_SETTINGS_OUTPUT_JSON)
        return load_from_json(provider, settings_path);
#elif defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
        return load_from_bitsery(provider, settings_path);
#else
        // Default: raw_string
        return load_from_raw_string(provider, settings_path);
#endif
    }

    template<typename SettingsData>
    static bool save() {
        auto *provider = get_provider<SettingsData>();
        if (!provider) {
            log_error(
                "Settings plugin not initialized. Call "
                "settings::init<SettingsData>() "
                "first.");
            return false;
        }

        if (provider->settings_file.empty()) {
            log_warn("Settings file not specified");
            return false;
        }

        // Try to get save path from Files plugin if available
        fs::path settings_path;
        auto *files_provider =
            EntityHelper::get_singleton_cmp<files::ProvidesResourcePaths>();
        if (files_provider) {
            settings_path = files_provider->get_save_path() /
                            fs::path(provider->settings_file);
            // Ensure directory exists
            files_provider->ensure_directory_exists(
                settings_path.parent_path());
        } else {
            // Fallback to current directory
            settings_path =
                fs::current_path() / fs::path(provider->settings_file);
        }

#if defined(AFTERHOURS_SETTINGS_OUTPUT_JSON)
        return save_to_json(provider, settings_path);
#elif defined(AFTERHOURS_SETTINGS_OUTPUT_BITSERY)
        return save_to_bitsery(provider, settings_path);
#else
        // Default: raw_string
        return save_to_raw_string(provider, settings_path);
#endif
    }

    static fs::path get_save_path() {
        auto *files_provider =
            EntityHelper::get_singleton_cmp<files::ProvidesResourcePaths>();
        if (files_provider) {
            return files_provider->get_save_path();
        }
        log_warn("Files plugin not initialized, returning current directory");
        return fs::current_path();
    }
};

}  // namespace afterhours
