#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

int main() {
  using afterhours::files;

  const fs::path docs = files::get_documents_path();
  printf("  documents: %s\n", docs.string().c_str());
  check(!docs.empty(), "a documents path is set");
#if !defined(__EMSCRIPTEN__)
  const char *home = getenv("HOME");
  check(home && *home, "HOME is set");
  check(home && docs.string().rfind(home, 0) == 0,
        "the documents path is under the home directory");
  check(docs.filename() == "Documents" || docs.filename() == "documents",
        "it ends in the Documents folder");
#endif

  std::error_code ec;
  const fs::path base = fs::temp_directory_path(ec) / "afh-documents-probe";
  fs::remove_all(base, ec);
  fs::create_directories(base / "has space", ec);
  const fs::path file = base / "has space" / "note.txt";
  { std::ofstream out(file); out << "x"; }
  check(fs::exists(file, ec), "the temp file exists");

  // reveal_command is the seam: it builds the invocation without running
  // it, so this never pops a Finder/Explorer window.
  const auto cmd = files::reveal_command(file);
  check(cmd.has_value(), "reveal_command accepts an existing file");
  if (cmd) {
    printf("  reveal_cmd: %s", (*cmd)[0].c_str());
    for (size_t i = 1; i < cmd->size(); ++i)
      printf(" %s", (*cmd)[i].c_str());
    printf("\n");
  }
#if defined(__APPLE__)
  check(cmd && cmd->size() == 3 && (*cmd)[0] == "open" && (*cmd)[1] == "-R" &&
            (*cmd)[2] == file.string(),
        "macOS reveals via open -R, even with spaces in the path");
#elif defined(_WIN32)
  check(cmd && !cmd->empty() && cmd->back().find(file.string()) !=
                                   std::string::npos,
        "Windows reveals via explorer /select");
#elif !defined(__EMSCRIPTEN__)
  check(cmd && cmd->size() == 2 && (*cmd)[0] == "xdg-open" &&
            (*cmd)[1] == (base / "has space").string(),
        "Linux opens the file's folder via xdg-open");
#endif

  const auto dircmd = files::reveal_command(file.parent_path());
  check(dircmd.has_value(), "reveal_command accepts an existing directory");
  check(!files::reveal_command(base / "missing.txt").has_value(),
        "reveal_command rejects a missing path");
  check(!files::reveal(base / "missing.txt"),
        "reveal rejects a missing path (and pops nothing)");

  fs::remove_all(base, ec);
  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
