#define AFTERHOURS_FILES_CPP_COMPILED

#if defined(_WIN32)
#undef AFTER_HOURS_USE_RAYLIB
#endif

#include "files.h"

#include <cstdlib>
#include <format>
#include <random>
#include <tuple>

#if defined(_WIN32)
#define AFTER_HOURS_USE_RAYLIB
#endif

// On Emscripten the virtual filesystem provides save/config/resource paths
// directly — no platform_folders dependency needed.
#ifndef __EMSCRIPTEN__

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

#endif // !__EMSCRIPTEN__

#ifndef __EMSCRIPTEN__
#include <cstdint>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif
#include <unistd.h> // chdir 
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#endif

namespace afterhours {

#ifndef __EMSCRIPTEN__
namespace {
// Directory holding the running executable, or empty if it cannot be found.
std::filesystem::path executable_dir() {
  std::error_code ec;
#if defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buf(size, '\0');
  if (_NSGetExecutablePath(buf.data(), &size) != 0)
    return {};
  auto exe = std::filesystem::canonical(buf.c_str(), ec);
#elif defined(_WIN32)
  char buf[MAX_PATH];
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (n == 0 || n == MAX_PATH)
    return {};
  auto exe = std::filesystem::canonical(std::string(buf, n), ec);
#else
  auto exe = std::filesystem::canonical("/proc/self/exe", ec);
#endif
  if (ec)
    return {};
  return exe.parent_path();
}

// Resource root, preferring the executable's own location.
//
// This used to be current_path()/root_folder. A launched macOS .app has CWD
// "/", so a bundled app could never find its own resources -- the API failed
// in exactly the case it exists for. CWD stays as the last resort so running
// from a build tree keeps working.
std::filesystem::path resolve_resource_root(const std::string &root_folder) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path exe = executable_dir();
  if (!exe.empty()) {
    if (fs::is_directory(exe / root_folder, ec)) // binary next to resources/
      return fs::weakly_canonical(exe / root_folder, ec);

    // .app only, recognised by the Contents/MacOS layout rather than by the
    // mere existence of a sibling Resources dir -- otherwise an unrelated
    // Resources would be returned even when it lacks the requested root.
    const bool in_app_bundle = exe.filename() == "MacOS" &&
                               exe.parent_path().filename() == "Contents";
    if (in_app_bundle) {
      const fs::path resources = exe.parent_path() / "Resources";
      if (fs::is_directory(resources / root_folder, ec))
        return fs::weakly_canonical(resources / root_folder, ec);
      if (fs::is_directory(resources, ec))
        return fs::weakly_canonical(resources, ec);
    }
  }
  return fs::current_path() / fs::path(root_folder);
}
} // namespace
#endif // !__EMSCRIPTEN__

namespace {
std::pair<std::filesystem::path, std::filesystem::path>
default_roots(const std::string &game_name) {
  if (files::active_test_roots)
    return {files::active_test_roots->save, files::active_test_roots->config};
#ifdef __EMSCRIPTEN__
  return {fs::path("/save") / game_name, fs::path("/config") / game_name};
#else
  return {fs::path(sago::getSaveGamesFolder1()) / game_name,
          sago::getConfigHome() / fs::path(game_name)};
#endif
}
} // namespace

// Implementation of ProvidesResourcePaths constructor that uses
// platform_folders on native, and Emscripten virtual filesystem paths on web.
files::ProvidesResourcePaths::ProvidesResourcePaths(
    const std::string &game_name, const std::string &root_folder)
    : game_name(game_name), root_folder(root_folder) {
#ifdef __EMSCRIPTEN__
  // Emscripten virtual filesystem: files preloaded with --preload-file are
  // available at the mount point. Save/config use in-memory directories.
  std::tie(save_folder_path, config_folder_path) = default_roots(game_name);
  resource_folder_path = fs::path("/") / fs::path(root_folder);
  cache_folder_path = fs::path("/cache") / fs::path(game_name);
  fs::create_directories(cache_folder_path);

  if (!fs::exists(save_folder_path)) {
    fs::create_directories(save_folder_path);
  }
  if (!fs::exists(config_folder_path)) {
    fs::create_directories(config_folder_path);
  }
#else
  // Native: use platform-specific directories
  std::tie(save_folder_path, config_folder_path) = default_roots(game_name);
  resource_folder_path = resolve_resource_root(root_folder);
  cache_folder_path = fs::path(sago::getCacheDir()) / fs::path(game_name);

  if (!fs::exists(save_folder_path)) {
    bool was_created = fs::create_directories(save_folder_path);
    if (was_created) {
      log_info("Created save folder: %s", save_folder_path.string().c_str());
    }
  }
#endif
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
  auto folder_path = resource_folder_path / fs::path(group) / fs::path(folder);

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
  if (!EntityHelper::get_default_collection()
           .has_singleton<ProvidesResourcePaths>()) {
    return nullptr;
  }
  return EntityHelper::get_singleton_cmp<ProvidesResourcePaths>();
}

void files::init(const std::string &game_name, const std::string &root_folder) {
  // Check if already initialized by checking singleton map directly
  if (EntityHelper::get_default_collection()
          .has_singleton<ProvidesResourcePaths>()) {
    log_warn("Files plugin already initialized");
    return;
  }

  // Create singleton entity
  Entity &entity = EntityHelper::createPermanentEntity();
  add_singleton_components(entity, game_name, root_folder);
  EntityHelper::merge_entity_arrays();

  // A crash leaves a temp behind, and a write-once path never rewrites it, so
  // nothing else would reclaim it. Deleting is safe: the target is intact
  // whether the temp was complete or half-written.
  sweep_temp_files(get_config_path());
  sweep_temp_files(get_save_path());
}

files::TestRoots::TestRoots() {
  std::error_code ec;
  const fs::path base = fs::temp_directory_path(ec);
  std::random_device rd;
  for (int attempt = 0; attempt < 100 && root.empty(); ++attempt) {
    fs::path candidate =
        base / std::format("afterhours-test-{:08x}{:08x}", rd(), rd());
    if (fs::create_directory(candidate, ec))
      root = candidate;
  }
  if (root.empty()) {
    log_error("TestRoots: cannot create a directory under {}", base.string());
    std::abort();
  }
  save = root / "save";
  config = root / "config";
  temp = root / "temp";
  for (const fs::path &dir : {save, config, temp})
    fs::create_directory(dir);

  previous = active_test_roots;
  active_test_roots = this;
  if (auto *provider = get_provider()) {
    provider_paths = {provider->save_folder_path, provider->config_folder_path};
    provider->save_folder_path = save;
    provider->config_folder_path = config;
  }
}

files::TestRoots::~TestRoots() {
  if (active_test_roots != this) {
    log_error("TestRoots destroyed out of order: {}", root.string());
    std::abort();
  }
  active_test_roots = previous;
  auto *provider = get_provider();
  if (provider && provider->save_folder_path == save) {
    std::tie(provider->save_folder_path, provider->config_folder_path) =
        provider_paths ? *provider_paths : default_roots(provider->game_name);
  }
  std::error_code ec;
  fs::remove_all(root, ec);
  if (ec)
    log_warn("TestRoots: could not delete {}: {}", root.string(), ec.message());
}

void files::chdir_to_resource_root(const std::string &root_folder) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (fs::is_directory(fs::current_path(ec) / root_folder, ec))
    return;
#ifdef __EMSCRIPTEN__
  // Preloaded at /<root_folder> via --preload-file; init() resolves it.
  (void)root_folder;
#else
  const fs::path exe = executable_dir();
  if (exe.empty())
    return;
  if (!fs::is_directory(exe / root_folder, ec))
    return;
  if (chdir(exe.string().c_str()) != 0)
    log_warn("chdir_to_resource_root: chdir(%s) failed", exe.string().c_str());
#endif
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

fs::path files::get_cache_path() {
  auto *provider = get_provider();
  if (!provider) {
    log_error("Files plugin not initialized. Call files::init() first.");
    return fs::path();
  }
  return provider->get_cache_path();
}

fs::path files::get_executable_dir() {
#ifdef __EMSCRIPTEN__
  return {};
#else
  return executable_dir();
#endif
}

fs::path files::get_documents_path() {
#ifdef __EMSCRIPTEN__
  log_error("@notimplemented files::get_documents_path on Emscripten");
  return {};
#else
  // platform_folders.h has non-inline defs: include only here.
  return fs::path(sago::getDocumentsFolder());
#endif
}

std::optional<std::vector<std::string>>
files::reveal_command(const fs::path &path) {
#ifdef __EMSCRIPTEN__
  log_error("@notimplemented files::reveal_command on Emscripten");
  return std::nullopt;
#endif
  std::error_code ec;
  if (!fs::exists(path, ec))
    return std::nullopt;
#if defined(__APPLE__)
  return std::vector<std::string>{"open", "-R", path.string()};
#elif defined(_WIN32)
  return fs::is_directory(path, ec)
             ? std::vector<std::string>{"explorer.exe", path.string()}
             : std::vector<std::string>{"explorer.exe",
                                        "/select," + path.string()};
#else
  // xdg-open cannot select a file; open its containing folder instead.
  const fs::path dir =
      fs::is_directory(path, ec) ? path : path.parent_path();
  if (dir.empty())
    return std::nullopt;
  return std::vector<std::string>{"xdg-open", dir.string()};
#endif
}

bool files::reveal(const fs::path &path) {
  auto cmd = reveal_command(path);
  if (!cmd)
    return false;
#if defined(_WIN32)
  std::string cmdline;
  for (size_t i = 0; i < cmd->size(); ++i) {
    if (i)
      cmdline += ' ';
    cmdline += '"';
    for (const char c : (*cmd)[i])
      cmdline += c == '"' ? std::string("\\\"") : std::string(1, c);
    cmdline += '"';
  }
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  if (!::CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi))
    return false;
  ::WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  const bool ok =
      ::GetExitCodeProcess(pi.hProcess, &code) && code == 0;
  ::CloseHandle(pi.hThread);
  ::CloseHandle(pi.hProcess);
  return ok;
#elif defined(__EMSCRIPTEN__)
  log_error("@notimplemented files::reveal on Emscripten");
  return false;
#else
  const pid_t pid = ::fork();
  if (pid < 0)
    return false;
  if (pid == 0) {
    std::vector<char *> argv;
    argv.reserve(cmd->size() + 1);
    for (auto &s : *cmd)
      argv.push_back(s.data());
    argv.push_back(nullptr);
    ::execvp(argv.front(), argv.data());
    ::_exit(127);
  }
  int status = 0;
  if (::waitpid(pid, &status, 0) < 0)
    return false;
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
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
} // namespace files_plugin_internal

} // namespace afterhours
