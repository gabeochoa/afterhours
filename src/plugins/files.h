#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

// Compile-time check: ensure files.cpp is compiled in your project
// If you see a linker error about ProvidesResourcePaths constructor,
// make sure to compile vendor/afterhours/src/plugins/files.cpp
#ifndef AFTERHOURS_FILES_CPP_COMPILED
// This will cause a linker error if files.cpp is not compiled
// The error message will guide you to compile the .cpp file
namespace afterhours {
namespace files_plugin_internal {
void _require_files_cpp_compiled();
}  // namespace files_plugin_internal
}  // namespace afterhours
#endif

namespace afterhours {
namespace fs = std::filesystem;

struct files : developer::Plugin {
    struct ProvidesResourcePaths : BaseComponent {
        std::string game_name;
        std::string root_folder = "resources";
        fs::path resource_folder_path;
        fs::path save_folder_path;
        fs::path config_folder_path;

        ProvidesResourcePaths() = default;
        // Constructor implementation in files.cpp to avoid including
        // platform_folders.h in header
        ProvidesResourcePaths(const std::string &game_name,
                              const std::string &root_folder);

        // Inline getters (trivial)
        [[nodiscard]] fs::path get_resource_path(
            const std::string &group, const std::string &name) const {
            return resource_folder_path / fs::path(group) / fs::path(name);
        }

        [[nodiscard]] fs::path get_save_path() const {
            return save_folder_path;
        }

        [[nodiscard]] fs::path get_config_path() const {
            return config_folder_path;
        }

        // Implementation in files.cpp
        void for_resources_in_group(
            const std::string &group,
            const std::function<void(std::string, std::string, std::string)>
                &cb) const;

        void for_resources_in_folder(
            const std::string &group, const std::string &folder,
            const std::function<void(std::string, std::string)> &cb) const;

        bool ensure_directory_exists(const fs::path &path);
    };

    // Implementation in files.cpp
    static void add_singleton_components(
        Entity &entity, const std::string &game_name,
        const std::string &root_folder = "resources");

    static void enforce_singletons(SystemManager &sm);

    static void register_update_systems(SystemManager &) {}

    // API methods - implementation in files.cpp
    static ProvidesResourcePaths *get_provider();

    static void init(const std::string &game_name,
                     const std::string &root_folder = "resources");

    static fs::path get_resource_path(const std::string &group,
                                      const std::string &name);

    static fs::path get_save_path();

    static fs::path get_config_path();

    static void for_resources_in_group(
        const std::string &group,
        const std::function<void(std::string, std::string, std::string)> &cb);

    static void for_resources_in_folder(
        const std::string &group, const std::string &folder,
        const std::function<void(std::string, std::string)> &cb);

    static bool ensure_directory_exists(const fs::path &path);
};

}  // namespace afterhours
