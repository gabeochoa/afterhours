// files_resource_path_test.cpp
// Gap #32: files::ProvidesResourcePaths resolved its resource root against the
// CWD. A launched macOS .app has CWD "/", so a bundled app could never find
// its own resources -- the API failed in exactly the case it exists for.
//
// The resolver is file-local in files.cpp, so these drive the real thing the
// only way an outside test can: build the directory layouts it probes for next
// to THIS test binary, construct a ProvidesResourcePaths, and check where it
// landed. That also means the test proves the executable-dir lookup works,
// which is the part that is easy to get wrong per-platform.

#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
// `files` is a class, not a namespace, so this has to be an alias.
using ProvidesResourcePaths = afterhours::files::ProvidesResourcePaths;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

// Where this binary lives; the resolver should prefer this over the CWD.
static fs::path exe_dir(const char *argv0) {
  std::error_code ec;
  fs::path p = fs::weakly_canonical(fs::path(argv0), ec);
  return ec ? fs::path(argv0).parent_path() : p.parent_path();
}

int main(int argc, char **argv) {
  std::error_code ec;

  // Re-exec'd copy of ourselves, living in a .app. Assert and exit; the parent
  // reads the status.
  if (argc > 1 && std::string(argv[1]) == "--bundle-child") {
    fs::current_path(fs::temp_directory_path(ec), ec);
    ProvidesResourcePaths p("testgame", "test_resources_b");
    const fs::path got = fs::weakly_canonical(p.resource_folder_path, ec);
    const bool ok = got.filename() == "test_resources_b" &&
                    got.parent_path().filename() == "Resources";
    if (!ok)
      fprintf(stderr, "  bundle child resolved to %s\n", got.string().c_str());
    return ok ? 0 : 1;
  }

  printf("=== files resource path tests ===\n\n");

  const fs::path here = exe_dir(argv[0]);

  // Run from somewhere that is definitely NOT the executable's directory, so
  // a CWD-based resolver cannot accidentally pass.
  const fs::path elsewhere = fs::temp_directory_path(ec);
  fs::current_path(elsewhere, ec);
  check(fs::current_path(ec) != here, "cwd differs from the executable dir");

  // 1. Sibling layout: <exe_dir>/test_resources_a
  {
    const fs::path root = here / "test_resources_a";
    fs::create_directories(root, ec);
    ProvidesResourcePaths p("testgame", "test_resources_a");
    check(fs::weakly_canonical(p.resource_folder_path, ec) ==
              fs::weakly_canonical(root, ec),
          "resolves a resources dir sitting next to the executable");
    fs::remove_all(root, ec);
  }

  // 2. Bundle layout. The resolver only trusts ../Resources when the binary
  //    really is in Contents/MacOS, so this has to be tested from an actual
  //    bundle: build one, copy this binary into it, and re-run it there. The
  //    child does the assertion and reports via its exit code.
  {
    const fs::path app = here / "FilesPathTest.app";
    const fs::path macos = app / "Contents" / "MacOS";
    const fs::path res = app / "Contents" / "Resources" / "test_resources_b";
    fs::remove_all(app, ec);
    fs::create_directories(macos, ec);
    fs::create_directories(res, ec);
    // argv[0] may be relative and the cwd has already moved, so copy from the
    // absolute path captured before the chdir.
    const fs::path self = here / fs::path(argv[0]).filename();
    const fs::path child = macos / "child";
    ec.clear();
    fs::copy_file(self, child, fs::copy_options::overwrite_existing, ec);
    check(!ec, "staged a test binary inside a .app layout");

    fs::permissions(child, fs::perms::owner_all, fs::perm_options::add, ec);
    const int rc = std::system((child.string() + " --bundle-child").c_str());
    check(rc == 0, "bundled binary resolves resources from Contents/Resources");
    fs::remove_all(app, ec);
  }

  // 3. Nothing to find: fall back to CWD, preserving the old behaviour so a
  //    dev build run from a source tree keeps working.
  {
    ProvidesResourcePaths p("testgame", "definitely_not_here");
    check(p.resource_folder_path ==
              fs::current_path(ec) / fs::path("definitely_not_here"),
          "falls back to cwd when nothing is found");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
