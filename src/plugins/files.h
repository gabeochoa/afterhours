#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#pragma clang diagnostic ignored "-Wdeprecated-volatile"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(WIN32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "../../vendor/sago/platform_folders.h"

#ifdef __APPLE__
#pragma clang diagnostic pop
#elif defined(WIN32)
#pragma GCC diagnostic pop
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
        ProvidesResourcePaths(const std::string &game_name,
                              const std::string &root_folder)
            : game_name(game_name), root_folder(root_folder) {
            // Initialize paths
            const fs::path master_folder(sago::getSaveGamesFolder1());
            save_folder_path = master_folder / fs::path(game_name);
            config_folder_path = sago::getConfigHome() / fs::path(game_name);
            resource_folder_path = fs::current_path() / fs::path(root_folder);

            // Ensure save folder exists
            if (!fs::exists(save_folder_path)) {
                bool was_created = fs::create_directories(save_folder_path);
                if (was_created) {
                    log_info("Created save folder: %s",
                             save_folder_path.string().c_str());
                }
            }
        }

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

        void for_resources_in_group(
            const std::string &group,
            const std::function<void(std::string, std::string, std::string)>
                &cb) const {
            auto folder_path = resource_folder_path / fs::path(group);

            try {
                auto dir_iter = fs::directory_iterator{folder_path};
                for (auto const &dir_entry : dir_iter) {
                    cb(dir_entry.path().stem().string(),
                       dir_entry.path().string(),
                       dir_entry.path().extension().string());
                }
            } catch (const std::exception &e) {
                log_warn(
                    "Exception while iterating over group resources %s: %s",
                    group.c_str(), e.what());
                return;
            }
        }

        void for_resources_in_folder(
            const std::string &group, const std::string &folder,
            const std::function<void(std::string, std::string)> &cb) const {
            auto folder_path =
                resource_folder_path / fs::path(group) / fs::path(folder);

            try {
                auto dir_iter = fs::directory_iterator{folder_path};
                for (auto const &dir_entry : dir_iter) {
                    cb(dir_entry.path().stem().string(),
                       dir_entry.path().string());
                }
            } catch (const std::exception &e) {
                log_warn(
                    "Exception while iterating over folder resources %s/%s: %s",
                    group.c_str(), folder.c_str(), e.what());
                return;
            }
        }

        bool ensure_directory_exists(const fs::path &path) {
            if (fs::exists(path)) {
                return true;
            }
            bool was_created = fs::create_directories(path);
            if (was_created) {
                log_info("Created directory: %s", path.string().c_str());
                return true;
            }
            return false;
        }
    };

    static void add_singleton_components(
        Entity &entity, const std::string &game_name,
        const std::string &root_folder = "resources") {
        entity.addComponent<ProvidesResourcePaths>(game_name, root_folder);
        EntityHelper::registerSingleton<ProvidesResourcePaths>(entity);
    }

    static void enforce_singletons(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<
                developer::EnforceSingleton<ProvidesResourcePaths>>());
    }

    static void register_update_systems(SystemManager &) {}

    // API methods
    static ProvidesResourcePaths *get_provider() {
        return EntityHelper::get_singleton_cmp<ProvidesResourcePaths>();
    }

    static void init(const std::string &game_name,
                     const std::string &root_folder = "resources") {
        // Check if already initialized by checking singleton map directly
        const ComponentID id = components::get_type_id<ProvidesResourcePaths>();
        if (EntityHelper::get().singletonMap.contains(id)) {
            log_warn("Files plugin already initialized");
            return;
        }

        // Create singleton entity
        Entity &entity = EntityHelper::createPermanentEntity();
        add_singleton_components(entity, game_name, root_folder);
        EntityHelper::merge_entity_arrays();
    }

    static fs::path get_resource_path(const std::string &group,
                                      const std::string &name) {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return fs::path();
        }
        return provider->get_resource_path(group, name);
    }

    static fs::path get_save_path() {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return fs::path();
        }
        return provider->get_save_path();
    }

    static fs::path get_config_path() {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return fs::path();
        }
        return provider->get_config_path();
    }

    static void for_resources_in_group(
        const std::string &group,
        const std::function<void(std::string, std::string, std::string)> &cb) {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return;
        }
        provider->for_resources_in_group(group, cb);
    }

    static void for_resources_in_folder(
        const std::string &group, const std::string &folder,
        const std::function<void(std::string, std::string)> &cb) {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return;
        }
        provider->for_resources_in_folder(group, folder, cb);
    }

    static bool ensure_directory_exists(const fs::path &path) {
        auto *provider = get_provider();
        if (!provider) {
            log_error(
                "Files plugin not initialized. Call files::init() first.");
            return false;
        }
        return provider->ensure_directory_exists(path);
    }
};

}  // namespace afterhours
