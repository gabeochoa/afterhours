// bundle_test.cpp
// Gap #31: tools/mk_bundle.sh, the opt-in .app packaging step.
//
// Drives the real script rather than reimplementing its logic, and uses THIS
// binary as the payload so the test is self-contained. The last case is the
// one that matters: it launches the bundled binary and checks that it can find
// its own resources, which proves bundling and the files-plugin resource
// resolver work together rather than each looking fine alone.
//
// macOS only -- registered in tests/Makefile behind UNAME_S == Darwin.

#include <afterhours/src/plugins/files.h>

#include <array>
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

static int run(const std::string &cmd) {
  return std::system((cmd + " >/dev/null 2>&1").c_str());
}

// Read one plist value via plutil, or "" if it is absent.
static std::string plist_value(const fs::path &plist, const std::string &key) {
  const std::string cmd =
      "plutil -extract " + key + " raw -o - '" + plist.string() + "' 2>/dev/null";
  FILE *p = popen(cmd.c_str(), "r");
  if (!p)
    return "";
  std::array<char, 512> buf{};
  std::string out;
  while (fgets(buf.data(), static_cast<int>(buf.size()), p))
    out += buf.data();
  if (pclose(p) != 0)
    return "";
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

int main(int argc, char **argv) {
  std::error_code ec;

  // Re-exec'd copy of ourselves from inside the bundle: assert that resources
  // resolve to Contents/Resources and report via exit status.
  if (argc > 1 && std::string(argv[1]) == "--bundle-child") {
    fs::current_path(fs::temp_directory_path(ec), ec);
    ProvidesResourcePaths p("testgame", "bundle_res");
    const fs::path got = fs::weakly_canonical(p.resource_folder_path, ec);
    const bool ok = got.filename() == "bundle_res" &&
                    got.parent_path().filename() == "Resources" &&
                    fs::exists(got / "marker.txt", ec);
    if (!ok)
      fprintf(stderr, "  bundle child resolved to %s\n", got.string().c_str());
    return ok ? 0 : 1;
  }

  printf("=== bundle tests ===\n\n");

  const fs::path self = fs::weakly_canonical(fs::path(argv[0]), ec);
  const fs::path here = self.parent_path();
  // tests/ sits next to tools/ under the repo root; output/ is the sibling the
  // binaries land in.
  const fs::path script = here.parent_path() / "tools" / "mk_bundle.sh";
  check(fs::exists(script), "mk_bundle.sh is where the test expects it");
  if (!fs::exists(script)) {
    printf("\n%d/%d checks passed\nFAILURES: %d\n", checks_passed, checks_run,
           checks_run - checks_passed);
    return 1;
  }

  const fs::path work = here / "bundle_test_work";
  fs::remove_all(work, ec);
  fs::create_directories(work / "res", ec);
  {
    std::FILE *f = std::fopen((work / "res" / "marker.txt").string().c_str(), "w");
    if (f) {
      std::fputs("marker\n", f);
      std::fclose(f);
    }
  }
  // The payload keeps its own filename inside the bundle, and
  // CFBundleExecutable must match it.
  const fs::path payload = work / "bundled_app";
  fs::copy_file(self, payload, fs::copy_options::overwrite_existing, ec);
  check(!ec, "staged the payload binary");

  const fs::path app = work / "Bundled.app";
  const std::string base = "'" + script.string() + "'" +
                           " --exe '" + payload.string() + "'" +
                           " --name Bundled --id com.example.bundled" +
                           " --out '" + app.string() + "'";

  // 1-2. Layout and a lintable plist.
  {
    const int rc = run(base + " --resources '" + (work / "res").string() +
                       "/../res' --url-scheme bundled");
    check(rc == 0, "mk_bundle.sh exits 0");
    check(fs::is_regular_file(app / "Contents" / "MacOS" / "bundled_app", ec),
          "binary landed in Contents/MacOS keeping its name");
    check(fs::is_regular_file(app / "Contents" / "Info.plist", ec),
          "Info.plist was written");
    check(run("plutil -lint '" + (app / "Contents" / "Info.plist").string() +
              "'") == 0,
          "Info.plist passes plutil -lint");
  }

  const fs::path plist = app / "Contents" / "Info.plist";

  // 3. The silent won't-launch guard.
  check(plist_value(plist, "CFBundleExecutable") == "bundled_app",
        "CFBundleExecutable matches the binary's filename");
  check(plist_value(plist, "CFBundleIdentifier") == "com.example.bundled",
        "CFBundleIdentifier is set");
  check(plist_value(plist, "NSHighResolutionCapable") == "true",
        "NSHighResolutionCapable is set (otherwise Retina looks soft)");

  // 4. Resources.
  check(fs::exists(app / "Contents" / "Resources" / "marker.txt", ec),
        "--resources contents landed in Contents/Resources");

  // 5. URL scheme declaration.
  check(plist_value(plist, "CFBundleURLTypes.0.CFBundleURLSchemes.0") ==
            "bundled",
        "--url-scheme produced a CFBundleURLTypes entry");

  // 6. Missing required args must fail rather than emit a broken bundle.
  check(run("'" + script.string() + "' --name X --id com.example.x") != 0,
        "missing --exe fails");
  check(run("'" + script.string() + "' --exe '" + payload.string() +
            "' --id com.example.x") != 0,
        "missing --name fails");
  check(run("'" + script.string() + "' --exe '" + payload.string() +
            "' --name X") != 0,
        "missing --id fails");

  // 7. End to end: the bundled binary finds its own resources. This is the
  //    whole point -- it exercises mk_bundle.sh and the resource resolver
  //    together, in the layout a shipped app actually has.
  {
    const fs::path res_root = app / "Contents" / "Resources" / "bundle_res";
    fs::create_directories(res_root, ec);
    std::FILE *f =
        std::fopen((res_root / "marker.txt").string().c_str(), "w");
    if (f) {
      std::fputs("marker\n", f);
      std::fclose(f);
    }
    const fs::path child = app / "Contents" / "MacOS" / "bundled_app";
    const int rc = std::system((child.string() + " --bundle-child").c_str());
    check(rc == 0, "bundled binary resolves resources from Contents/Resources");
  }

  fs::remove_all(work, ec);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
