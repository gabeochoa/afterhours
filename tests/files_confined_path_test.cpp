// files_confined_path_test.cpp
// files::resolve_confined / path_within_root. Caller-supplied relative paths
// must stay under a root: `..` escapes, absolute paths, and symlinks pointing
// outside are rejected; `.` and in-root `..` fold away.

#include <afterhours/src/plugins/files.h>

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using afterhours::files;

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

static std::string str(const std::optional<fs::path> &p) {
  return p ? p->string() : std::string("<nullopt>");
}

int main() {
  printf("=== files confined path tests ===\n\n");

  std::error_code ec;
  const fs::path root = fs::temp_directory_path(ec) /
                       ("afh_confined_path_test_" + std::to_string(getpid()));
  fs::remove_all(root, ec);
  fs::create_directories(root / "sub" / "deep", ec);
  fs::create_directories(root / "real", ec);
  const fs::path outside = root.parent_path() /
                           ("afh_confined_outside_" + std::to_string(getpid()));
  fs::remove_all(outside, ec);
  fs::create_directories(outside, ec);
  // A same-prefix sibling of the root: prefix matching must not let it in.
  const fs::path sibling = fs::path(root.string() + "_backup");
  fs::remove_all(sibling, ec);
  fs::create_directories(sibling, ec);
  const fs::path base = fs::weakly_canonical(fs::absolute(root, ec), ec);

  // 1. Plain, nested, `./`, and in-root `..` paths resolve inside the root.
  check(str(files::resolve_confined(root, "a.txt")) == (base / "a.txt").string(),
        "plain name resolves");
  check(str(files::resolve_confined(root, "sub/deep/f.txt")) ==
            (base / "sub" / "deep" / "f.txt").string(),
        "nested path resolves");
  check(str(files::resolve_confined(root, "./a.txt")) == (base / "a.txt").string(),
        "./ prefix folds away");
  check(str(files::resolve_confined(root, "sub/../a.txt")) ==
            (base / "a.txt").string(),
        "in-root .. folds away");
  check(str(files::resolve_confined(root, "sub/deep/../../a.txt")) ==
            (base / "a.txt").string(),
        "double in-root .. folds away");
  check(str(files::resolve_confined(root, "")) == base.string(),
        "empty resolves to the root itself");
  check(str(files::resolve_confined(root, ".")) == base.string(),
        ". resolves to the root itself");
  check(str(files::resolve_confined(root, "not/on/disk/yet.txt")) ==
            (base / "not" / "on" / "disk" / "yet.txt").string(),
        "missing tail resolves lexically without existing");
  check(str(files::resolve_confined(fs::path(root.string() + "/"), "a.txt")) ==
            (base / "a.txt").string(),
        "trailing slash on the root is fine");

  // 2. Escapes are rejected.
  check(!files::resolve_confined(root, "../escape.txt").has_value(),
        "../ escape rejected");
  check(!files::resolve_confined(root, "sub/../../escape.txt").has_value(),
        "nested ../ escape rejected");
  check(!files::resolve_confined(root, "../" + root.filename().string() +
                                          "_backup/x.txt")
             .has_value(),
        "same-prefix sibling rejected");
  check(!files::resolve_confined(root, (outside / "x.txt").string()).has_value(),
        "absolute path outside rejected");
  check(!files::resolve_confined(root, (base / "a.txt").string()).has_value(),
        "absolute path inside rejected (relative only)");
  check(!files::resolve_confined(root, "/etc/passwd").has_value(),
        "absolute system path rejected");

  // 3. Symlink policy: staying inside is allowed, leaving is not.
  fs::create_symlink("real", root / "link_in", ec);
  fs::create_symlink(outside, root / "link_out", ec);
  fs::create_symlink(sibling, root / "sub" / "link_sib", ec);
  fs::create_symlink("../" + outside.filename().string(), root / "rel_out", ec);
  check(str(files::resolve_confined(root, "link_in/f.txt")) ==
            (base / "real" / "f.txt").string(),
        "symlink staying inside resolves to its target");
  check(!files::resolve_confined(root, "link_out/x.txt").has_value(),
        "symlink to outside rejected");
  check(!files::resolve_confined(root, "rel_out/x.txt").has_value(),
        "relative symlink to outside rejected");
  check(!files::resolve_confined(root, "sub/link_sib/x.txt").has_value(),
        "nested symlink to same-prefix sibling rejected");

  // 4. path_within_root is component-wise, not a string prefix test.
  check(files::path_within_root(base, base), "root contains itself");
  check(files::path_within_root(base, base / "a" / "b"), "root contains a child");
  check(!files::path_within_root(base, sibling), "same-prefix sibling is outside");
  check(!files::path_within_root(base, base.parent_path()),
        "parent of the root is outside");

  fs::remove_all(sibling, ec);
  fs::remove_all(outside, ec);
  fs::remove_all(root, ec);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
