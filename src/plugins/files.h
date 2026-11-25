#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <sys/stat.h>
extern "C" __declspec(dllimport) int __stdcall MoveFileExW(const wchar_t *,
                                                           const wchar_t *,
                                                           unsigned long);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
#else
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
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

namespace files_plugin_internal {
#ifdef _WIN32
constexpr unsigned long MOVE_REPLACE_EXISTING = 0x1;
constexpr unsigned long MOVE_WRITE_THROUGH = 0x8;
inline long process_id() { return static_cast<long>(::_getpid()); }
inline int open_new(const fs::path &p) {
  return ::_wopen(p.c_str(),
                  _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT,
                  _S_IREAD | _S_IWRITE);
}
inline long write_some(int fd, const char *data, size_t n) {
  return ::_write(fd, data,
                  static_cast<unsigned>(std::min<size_t>(n, 1u << 30)));
}
inline bool sync_fd(int fd) { return ::_commit(fd) == 0; }
inline bool close_fd(int fd) { return ::_close(fd) == 0; }
inline std::string replace_file(const fs::path &from, const fs::path &to) {
  if (MoveFileExW(from.c_str(), to.c_str(),
                  MOVE_REPLACE_EXISTING | MOVE_WRITE_THROUGH))
    return {};
  return "Win32 error " + std::to_string(GetLastError());
}
inline std::string sync_dir(const fs::path &) { return {}; }
inline std::string copy_mode(const fs::path &, int) { return {}; }
#else
inline long process_id() { return static_cast<long>(::getpid()); }
inline int open_new(const fs::path &p) {
  return ::open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
}
inline long write_some(int fd, const char *data, size_t n) {
  return static_cast<long>(::write(fd, data, n));
}
inline bool sync_fd(int fd) {
#ifdef F_FULLFSYNC
  if (::fcntl(fd, F_FULLFSYNC) == 0)
    return true;
#endif
  return ::fsync(fd) == 0;
}
inline bool close_fd(int fd) { return ::close(fd) == 0; }
inline std::string replace_file(const fs::path &from, const fs::path &to) {
  if (::rename(from.c_str(), to.c_str()) == 0)
    return {};
  return std::strerror(errno);
}
inline std::string sync_dir(const fs::path &dir) {
  const int fd = ::open(dir.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return std::strerror(errno);
  std::string err = sync_fd(fd) ? "" : std::strerror(errno);
  ::close(fd);
  return err;
}
inline std::string copy_mode(const fs::path &from, int fd) {
  struct stat existing {};
  if (::stat(from.c_str(), &existing) != 0)
    return {};
  if (::fchmod(fd, existing.st_mode & 07777) != 0)
    return std::strerror(errno);
  return {};
}
#endif
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
inline bool process_alive(long) { return false; }
#else
inline bool process_alive(long pid) {
  return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno != ESRCH;
}
#endif
}

struct files : developer::Plugin {
  struct ProvidesResourcePaths : BaseComponent {
    std::string game_name;
    std::string root_folder = "resources";
    fs::path resource_folder_path;
    fs::path save_folder_path;
    fs::path config_folder_path;
    fs::path cache_folder_path;

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

    [[nodiscard]] fs::path get_cache_path() const { return cache_folder_path; }

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

  struct TestRoots {
    fs::path root;
    fs::path save;
    fs::path config;
    fs::path temp;

    TestRoots();
    ~TestRoots();
    TestRoots(const TestRoots &) = delete;
    TestRoots &operator=(const TestRoots &) = delete;

  private:
    TestRoots *previous = nullptr;
    std::optional<std::pair<fs::path, fs::path>> provider_paths;
  };
  inline static TestRoots *active_test_roots = nullptr;

  static bool maybe_install_e2e_test_roots(int argc, char *argv[]);

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

  static fs::path get_cache_path();

  // Directory containing the running executable, or empty if unknown.
  // Available before init(); empty on Emscripten.
  static fs::path get_executable_dir();

  // The user's Documents folder (~/Documents etc.), or empty on Emscripten.
  // Available before init(); the caller adds its own subfolder.
  static fs::path get_documents_path();

  // Show path in the platform file manager (Finder/Explorer/open folder).
  // False when the path does not exist or the platform is unsupported
  // (Emscripten). Testable seam: reveal_command() builds the invocation
  // without running it, so tests never pop a window.
  static std::optional<std::vector<std::string>>
  reveal_command(const fs::path &path);
  static bool reveal(const fs::path &path);

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

  static bool path_within_root(const fs::path &base, const fs::path &p) {
    auto b = base.begin();
    auto it = p.begin();
    for (; b != base.end(); ++b, ++it)
      if (it == p.end() || *it != *b)
        return false;
    return true;
  }

  static std::optional<fs::path> resolve_confined(const fs::path &root,
                                                  const fs::path &relative) {
    if (relative.is_absolute() || relative.has_root_name())
      return std::nullopt;
    std::error_code ec;
    const fs::path base = fs::weakly_canonical(fs::absolute(root, ec), ec);
    if (ec)
      return std::nullopt;
    fs::path cur = base;
    for (const auto &part : relative) {
      const std::string name = part.string();
      if (name.empty() || name == ".")
        continue;
      if (name == "..") {
        if (cur == base)
          return std::nullopt;
        cur = cur.parent_path();
        continue;
      }
      cur /= part;
      ec.clear();
      if (fs::is_symlink(cur, ec)) {
        const fs::path target = fs::canonical(cur, ec);
        if (ec || !path_within_root(base, target))
          return std::nullopt;
        cur = target;
      }
    }
    return cur;
  }

  static bool write_string_atomic(const fs::path &path,
                                  std::string_view content) {
    namespace sys = files_plugin_internal;
    static std::atomic<unsigned long> counter{0};
    std::error_code ec;
    if (!path.parent_path().empty())
      fs::create_directories(path.parent_path(), ec);

    fs::path tmp;
    int fd = -1;
    for (int attempt = 0; attempt < 100 && fd < 0; attempt++) {
      tmp = path;
      tmp += "." + std::to_string(sys::process_id()) + "." +
             std::to_string(counter++) + TEMP_SUFFIX;
      fd = sys::open_new(tmp);
      if (fd < 0 && errno != EEXIST)
        break;
    }
    if (fd < 0) {
      log_warn("write_string_atomic: cannot create a temp beside {}: {}",
               path.string(), std::strerror(errno));
      return false;
    }

    auto fail = [&](const char *step, const std::string &why) {
      if (fd >= 0)
        sys::close_fd(fd);
      fs::remove(tmp, ec);
      log_warn("write_string_atomic: {} failed for {}: {}", step,
               path.string(), why);
      return false;
    };

    if (std::string why = sys::copy_mode(path, fd); !why.empty())
      return fail("chmod", why);

    size_t done = 0;
    while (done < content.size()) {
      const long n =
          sys::write_some(fd, content.data() + done, content.size() - done);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        return fail("write", std::strerror(errno));
      done += static_cast<size_t>(n);
    }
    if (!sys::sync_fd(fd))
      return fail("fsync", std::strerror(errno));
    const bool closed = sys::close_fd(fd);
    fd = -1;
    if (!closed)
      return fail("close", std::strerror(errno));
    if (std::string why = sys::replace_file(tmp, path); !why.empty())
      return fail("rename", why);

    const fs::path dir =
        path.parent_path().empty() ? fs::path(".") : path.parent_path();
    if (std::string why = sys::sync_dir(dir); !why.empty())
      log_warn("write_string_atomic: wrote {} but fsync of {} failed: {}",
               path.string(), dir.string(), why);
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

  static long temp_writer_pid(std::string_view stem) {
    const size_t n_dot = stem.rfind('.');
    if (n_dot == std::string_view::npos || n_dot == 0)
      return 0;
    const size_t pid_dot = stem.rfind('.', n_dot - 1);
    if (pid_dot == std::string_view::npos)
      return 0;
    const std::string_view n_part = stem.substr(n_dot + 1);
    const std::string_view pid_part =
        stem.substr(pid_dot + 1, n_dot - pid_dot - 1);
    unsigned long n = 0;
    long pid = 0;
    const auto n_end = n_part.data() + n_part.size();
    const auto pid_end = pid_part.data() + pid_part.size();
    if (n_part.empty() || std::from_chars(n_part.data(), n_end, n).ptr != n_end)
      return 0;
    if (pid_part.empty() ||
        std::from_chars(pid_part.data(), pid_end, pid).ptr != pid_end)
      return 0;
    return pid;
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
      const long pid =
          temp_writer_pid(name.substr(0, name.size() - strlen(TEMP_SUFFIX)));
      if (pid > 0 && files_plugin_internal::process_alive(pid))
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

struct FilesPlugin {
  std::string game_name{};
  std::string root_folder = "resources";

  void install(PluginContext &ctx) const {
    ctx.ensure_singleton<files::ProvidesResourcePaths>(game_name, root_folder);
  }
};
static_assert(InstallablePlugin<FilesPlugin>);

} // namespace afterhours
