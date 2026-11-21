// capture::record and warn_on_missing_glyphs sit on every draw. Counted, not
// timed: allocations and decode-loop runs are deterministic, and a millisecond
// cannot tell a strlen from a malloc.
//
// - record() disabled must allocate nothing, even for a long text argument.
// - warn_on_missing_glyphs must not decode pure ASCII, and must not re-decode
//   a (font, text) pair it already answered -- warn_once already keeps the log
//   quiet, the loop is what was left.

#define FMT_HEADER_ONLY
#include <afterhours/src/backends/raylib/font_helper.h>
#include <afterhours/src/capture.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace afterhours;

static std::size_t g_allocs = 0;
static bool g_counting = false;
void *operator new(std::size_t n) {
  if (g_counting)
    g_allocs++;
  return std::malloc(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  const RectangleType r{1.f, 2.f, 3.f, 4.f};
  const ColorType c{10, 20, 30, 255};

  // Disabled: recording a long text must not build a string or touch the heap.
  capture::disable();
  capture::clear();
  const std::string long_text(4096, 'x');
  g_allocs = 0;
  g_counting = true;
  for (int i = 0; i < 1000; i++)
    capture::record("text", r, c, long_text.c_str());
  g_counting = false;
  std::printf("  disabled record: %.2f allocations per call\n",
              double(g_allocs) / 1000);
  CHECK(g_allocs == 0);
  CHECK(capture::calls().empty());

  // Enabled: unchanged, text kept.
  capture::enable();
  capture::record("text", r, c, "hello");
  CHECK(capture::calls().size() == 1);
  CHECK(capture::calls()[0].text == "hello");
  capture::disable();
  capture::clear();

  // Glyph warn: ASCII never decodes, whatever the font.
  const std::string path =
      (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
       "AtkinsonHyperlegible-Regular.ttf")
          .string();
  const raylib::Font font = load_font_for_string("A", path);
  glyph_warn_decode_runs() = 0;
  for (int i = 0; i < 100; i++)
    warn_on_missing_glyphs(font, "plain ascii label");
  std::printf("  warn ASCII: %zu decode runs over 100 draws\n",
              glyph_warn_decode_runs());
  CHECK(glyph_warn_decode_runs() == 0);

  // Non-ASCII decodes once per (font, text), not once per draw.
  const char *non_ascii = "caf\xC3\xA9 \xE2\x98\x83";
  warn_on_missing_glyphs(font, non_ascii);
  for (int i = 0; i < 100; i++)
    warn_on_missing_glyphs(font, non_ascii);
  std::printf("  warn non-ASCII repeated: %zu decode runs over 101 draws\n",
              glyph_warn_decode_runs());
  CHECK(glyph_warn_decode_runs() == 1);

  // A different string under the same font is a different question.
  warn_on_missing_glyphs(font, "\xE2\x98\x83 snowman");
  std::printf("  warn non-ASCII new string: %zu decode runs\n",
              glyph_warn_decode_runs());
  CHECK(glyph_warn_decode_runs() == 2);

  // Coverage answers did not move under the cache.
  CHECK(font_has_glyph(font, 'A'));
  CHECK(!font_has_glyph(font, 0x2603));

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0)
    std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
