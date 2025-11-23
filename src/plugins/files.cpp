#define AFTERHOURS_FILES_CPP_COMPILED

#include "files.h"

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

// Implementation of ProvidesResourcePaths constructor that uses
// platform_folders This is in a .cpp file to avoid duplicate symbol errors from
// platform_folders.h
files::ProvidesResourcePaths::ProvidesResourcePaths(
    const std::string &game_name, const std::string &root_folder)
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

void files::ProvidesResourcePaths::for_resources_in_group(
    const std::string &group,
    const std::function<void(std::string, std::string, std::string)> &cb)
    const {
    auto folder_path = resource_folder_path / fs::path(group);

    try {
        auto dir_iter = fs::directory_iterator{folder_path};
        for (auto const &dir_entry : dir_iter) {
            cb(dir_entry.path().stem().string(), dir_entry.path().string(),
               dir_entry.path().extension().string());
        }
    } catch (const std::exception &e) {
        log_warn("Exception while iterating over group resources %s: %s",
                 group.c_str(), e.what());
        return;
    }
}

void files::ProvidesResourcePaths::for_resources_in_folder(
    const std::string &group, const std::string &folder,
    const std::function<void(std::string, std::string)> &cb) const {
    auto folder_path =
        resource_folder_path / fs::path(group) / fs::path(folder);

    try {
        auto dir_iter = fs::directory_iterator{folder_path};
        for (auto const &dir_entry : dir_iter) {
            cb(dir_entry.path().stem().string(), dir_entry.path().string());
        }
    } catch (const std::exception &e) {
        log_warn("Exception while iterating over folder resources %s/%s: %s",
                 group.c_str(), folder.c_str(), e.what());
        return;
    }
}

bool files::ProvidesResourcePaths::ensure_directory_exists(
    const fs::path &path) {
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

void files::add_singleton_components(Entity &entity,
                                     const std::string &game_name,
                                     const std::string &root_folder) {
    entity.addComponent<ProvidesResourcePaths>(game_name, root_folder);
    EntityHelper::registerSingleton<ProvidesResourcePaths>(entity);
}

void files::enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesResourcePaths>>());
}

files::ProvidesResourcePaths *files::get_provider() {
    return EntityHelper::get_singleton_cmp<ProvidesResourcePaths>();
}

void files::init(const std::string &game_name, const std::string &root_folder) {
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

fs::path files::get_resource_path(const std::string &group,
                                  const std::string &name) {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return fs::path();
    }
    return provider->get_resource_path(group, name);
}

fs::path files::get_save_path() {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return fs::path();
    }
    return provider->get_save_path();
}

fs::path files::get_config_path() {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return fs::path();
    }
    return provider->get_config_path();
}

void files::for_resources_in_group(
    const std::string &group,
    const std::function<void(std::string, std::string, std::string)> &cb) {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return;
    }
    provider->for_resources_in_group(group, cb);
}

void files::for_resources_in_folder(
    const std::string &group, const std::string &folder,
    const std::function<void(std::string, std::string)> &cb) {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return;
    }
    provider->for_resources_in_folder(group, folder, cb);
}

bool files::ensure_directory_exists(const fs::path &path) {
    auto *provider = get_provider();
    if (!provider) {
        log_error("Files plugin not initialized. Call files::init() first.");
        return false;
    }
    return provider->ensure_directory_exists(path);
}

// Provide the symbol that the header checks for
namespace files_plugin_internal {
void _require_files_cpp_compiled() {}
}  // namespace files_plugin_internal

}  // namespace afterhours
