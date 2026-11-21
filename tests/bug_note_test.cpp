// bug_note(dir, text, context...) helper: note file contents, timestamped
// name, dir auto-create, screenshot created headless via capture_frame.
// Compiled as a raylib test; skips when headless init fails (no GPU).

#define FMT_HEADER_ONLY
#include <afterhours/src/bug_note.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static std::string slurp(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 32;
  config.title = "bug_note test";
  if (!graphics::init(config)) {
    printf("SKIP: headless init failed (no GPU available)\n");
    return 0;
  }
  graphics::begin_frame();
  graphics::clear_background(Color{255, 0, 0, 128});
  graphics::end_frame();

  const auto base = std::filesystem::temp_directory_path() /
      ("afterhours-bugnote-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto dir = base / "nested" / "notes"; // not created: capture must make it
  CHECK(!std::filesystem::exists(dir));

  const auto r = bug_note::capture(
      dir, "jump is too floaty on the icy slope", "cartographer 1.2.3 (abc123)",
      {"level: valley", "player: (12, 4, 9)"});

  CHECK(std::filesystem::exists(r.note_path));
  CHECK(r.screenshot_captured);
  CHECK(std::filesystem::exists(r.screenshot_path));
  const std::string png = slurp(r.screenshot_path);
  CHECK(png.size() > 8);
  CHECK(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0);

  const std::string note = slurp(r.note_path);
  CHECK(note.find("jump is too floaty on the icy slope") != std::string::npos);
  CHECK(note.find("cartographer 1.2.3 (abc123)") != std::string::npos);
  CHECK(note.find("- level: valley") != std::string::npos);
  CHECK(note.find("- player: (12, 4, 9)") != std::string::npos);
  CHECK(note.find("timestamp:") != std::string::npos);
  CHECK(note.find("screenshot:") != std::string::npos);
  CHECK(note.find("bug-") != std::string::npos);

  // Same second, second note: a distinct timestamped file, not an overwrite.
  const auto r2 = bug_note::capture(dir, "second note", "cartographer 1.2.3 (abc123)");
  CHECK(r2.note_path != r.note_path);
  CHECK(std::filesystem::exists(r2.note_path));
  CHECK(slurp(r2.note_path).find("(none)") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  graphics::shutdown();

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
