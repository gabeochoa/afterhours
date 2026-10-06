#pragma once

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <process.h>
#endif

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

  // If a <root_folder>/ dir sits next to the executable (packaged layout),
  // chdir there so cwd-relative "<root_folder>/..." loads resolve. Skipped when
  // cwd already has that folder (dev: run from the repo root). No-op on
  // Emscripten (preload mounts at /<root_folder>).
  static void
  chdir_to_resource_root(const std::string &root_folder = "resources");

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
  // Each write gets its own temp, `<target>.afh-tmp.<pid>.<n>`: two writers
  // to one path (two windows, a save racing a sync) never share a temp, so
  // the last rename to complete wins and a reader sees the old file or a
  // whole new one, never a mix.
  static fs::path unique_temp_path(const fs::path &path) {
    static std::atomic<unsigned long> counter{0};
    fs::path tmp = path;
    tmp += TEMP_SUFFIX;
    tmp += "." + std::to_string(current_pid()) + "." + std::to_string(counter++);
    return tmp;
  }

  // Durable replace: write a fresh temp, flush it to the device, rename it
  // over the target, then flush the directory so the rename itself survives
  // power loss. On macOS fsync stops at the drive's cache; F_FULLFSYNC does
  // not, so it is used there (it costs milliseconds per write).
  static bool write_string_atomic(const fs::path &path,
                                  std::string_view content) {
    std::error_code ec;
    if (!path.parent_path().empty())
      fs::create_directories(path.parent_path(), ec);
    // Older versions used one fixed temp name per target; a crash could
    // leave it behind, and nothing else ever writes that exact name.
    fs::remove(fs::path(path.string() + TEMP_SUFFIX), ec);
    const fs::path tmp = unique_temp_path(path);
#ifndef _WIN32
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) {
      log_warn("write_string_atomic: cannot create {}: {}", tmp.string(), strerror(errno));
      return false;
    }
    auto fail = [&](const char *what) {
      const int saved = errno;
      ::close(fd);
      fs::remove(tmp, ec);
      log_warn("write_string_atomic: {} failed for {}: {}", what, path.string(), strerror(saved));
      return false;
    };
    // Keep the target's permissions (a 0600 settings file stays 0600).
    struct stat existing {};
    if (::stat(path.c_str(), &existing) == 0 && ::fchmod(fd, existing.st_mode & 07777) != 0)
      return fail("chmod");
    const char *at = content.data();
    size_t left = content.size();
    while (left > 0) {
      const ssize_t n = ::write(fd, at, left);
      if (n < 0) {
        if (errno == EINTR)
          continue;
        return fail("write");  // ENOSPC, EFBIG, EIO: the target is untouched
      }
      at += n;
      left -= static_cast<size_t>(n);
    }
#ifdef __APPLE__
    if (::fcntl(fd, F_FULLFSYNC) != 0 && ::fsync(fd) != 0)
      return fail("fsync");
#else
    if (::fsync(fd) != 0)
      return fail("fsync");
#endif
    if (::close(fd) != 0) {
      fs::remove(tmp, ec);
      log_warn("write_string_atomic: close failed for {}", path.string());
      return false;
    }
    if (::rename(tmp.c_str(), path.c_str()) != 0) {
      const int saved = errno;
      fs::remove(tmp, ec);
      log_warn("write_string_atomic: rename to {} failed: {}", path.string(), strerror(saved));
      return false;
    }
    const fs::path dir = path.parent_path().empty() ? fs::path(".") : path.parent_path();
    const int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0) {
      ::fsync(dfd);  // best effort: some filesystems refuse directory fsync
      ::close(dfd);
    }
    return true;
#else
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) {
        log_warn("write_string_atomic: cannot open {}", tmp.string());
        return false;
      }
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      out.flush();
      out.close();
      if (!out) {
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
#endif
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

  // A temp whose writer can no longer finish it: the legacy fixed name, or a
  // `.afh-tmp.<pid>.<n>` whose process has exited. A live writer's temp
  // (another window mid-save) is left for its rename.
  static bool is_orphaned_temp(const std::string &name) {
    const size_t at = name.rfind(TEMP_SUFFIX);
    if (at == std::string::npos || at == 0)
      return false;
    const std::string rest = name.substr(at + strlen(TEMP_SUFFIX));
    if (rest.empty())
      return true;
    long pid = 0;
    if (std::sscanf(rest.c_str(), ".%ld.", &pid) != 1 || pid <= 0)
      return false;
#ifndef _WIN32
    if (pid == current_pid())
      return false;
    return ::kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH;
#else
    return true;
#endif
  }

  static long current_pid() {
#ifndef _WIN32
    return static_cast<long>(::getpid());
#else
    return static_cast<long>(_getpid());
#endif
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
      if (!is_orphaned_temp(it->path().filename().string()))
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
