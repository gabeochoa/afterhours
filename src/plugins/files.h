#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

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
} // namespace files_plugin_internal
} // namespace afterhours
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
    [[nodiscard]] fs::path get_resource_path(const std::string &group,
                                             const std::string &name) const {
      return resource_folder_path / fs::path(group) / fs::path(name);
    }

    [[nodiscard]] fs::path get_save_path() const { return save_folder_path; }

    [[nodiscard]] fs::path get_config_path() const {
      return config_folder_path;
    }

    // Implementation in files.cpp
    void for_resources_in_group(
        const std::string &group,
        const std::function<void(std::string, std::string, std::string)> &cb)
        const;

    void for_resources_in_folder(
        const std::string &group, const std::string &folder,
        const std::function<void(std::string, std::string)> &cb) const;

    bool ensure_directory_exists(const fs::path &path);
  };

  // Non-templated version for PluginCore concept compatibility
  // Note: For actual files functionality, use the overload with game_name.
  static void add_singleton_components(Entity &entity) {
    // Default initialization with empty game name
    add_singleton_components(entity, "", "resources");
  }

  // Implementation in files.cpp
  static void
  add_singleton_components(Entity &entity, const std::string &game_name,
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

  // Suffix for in-progress writes. Distinctive so the sweep never deletes a
  // caller's own .tmp file.
  static constexpr const char *TEMP_SUFFIX = ".afh-tmp";

  // Defined here rather than in files.cpp: these are plain std::filesystem
  // helpers with no singleton or platform_folders dependency, and theme_io
  // (pulled in by ui.h) needs them without forcing every UI consumer to link
  // files.cpp.

  // Write to a sibling temp then rename, so a crash mid-write cannot truncate
  // the target.
  // TODO: crash-safe but not power-loss-safe. Durability across power loss
  // needs open/write/fsync/close on a raw fd plus an fsync of the directory,
  // which is platform-specific. Fits behind this signature when needed.
  static bool write_string_atomic(const fs::path &path,
                                  std::string_view content) {
    std::error_code ec;
    if (!path.parent_path().empty())
      fs::create_directories(path.parent_path(), ec);

    // Sibling temp: rename is only atomic within one filesystem. The suffix is
    // distinctive so sweep_temp_files can tell ours from a caller's .tmp.
    fs::path tmp = path;
    tmp += TEMP_SUFFIX;

    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) {
        log_warn("write_string_atomic: cannot open {}", tmp.string());
        return false;
      }
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      out.flush();
      if (!out) {
        out.close();
        fs::remove(tmp, ec);
        log_warn("write_string_atomic: write failed for {}", tmp.string());
        return false;
      }
    }

    fs::rename(tmp, path, ec);
    if (ec) {
      fs::remove(tmp, ec);
      log_warn("write_string_atomic: rename to {} failed", path.string());
      return false;
    }
    return true;
  }

  // Whole file, or nullopt if missing/unreadable.
  static std::optional<std::string> read_string(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
      return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad())
      return std::nullopt;
    return ss.str();
  }

  // Delete leftover temps under `dir`. init() runs it over the config and save
  // dirs. Never recovers data from them: a half-written and a complete temp are
  // indistinguishable, so promoting one could overwrite good data.
  static int sweep_temp_files(const fs::path &dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
      return 0;
    int removed = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end;
         it.increment(ec)) {
      if (!it->is_regular_file(ec))
        continue;
      const std::string name = it->path().filename().string();
      if (name.size() <= strlen(TEMP_SUFFIX))
        continue;
      if (name.compare(name.size() - strlen(TEMP_SUFFIX),
                       strlen(TEMP_SUFFIX), TEMP_SUFFIX) != 0)
        continue;
      std::error_code rm;
      if (fs::remove(it->path(), rm))
        removed++;
    }
    if (removed > 0)
      log_info("swept {} leftover temp file(s) from {}", removed, dir.string());
    return removed;
  }
};

// Compile-time verification that files satisfies the PluginCore concept
static_assert(developer::PluginCore<files>,
              "files must implement the core plugin interface");

} // namespace afterhours
