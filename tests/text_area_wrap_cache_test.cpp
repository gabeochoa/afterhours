// text_area_wrap_cache_test.cpp
// R-027: text_area rebuilt its TextLayoutCache every frame. The cache now
// skips the wrap when text, wrap width, line height and the measure key are
// all unchanged, so N standing-still frames pay for exactly one wrap.
// Counting measure_fn calls is the counted proof: a skipped rebuild never
// measures anything.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/text_layout.h>

#include <string>
#include <string_view>

using namespace afterhours;
namespace ti = afterhours::text_input;

namespace {
int g_measures = 0;
float measure10(std::string_view s) {
  ++g_measures;
  return static_cast<float>(s.size()) * 10.f;
}

float measure5(std::string_view s) {
  ++g_measures;
  return static_cast<float>(s.size()) * 5.f;
}
constexpr float LINE_H = 20.f;
const std::string SRC = "hello world again and again";
} // namespace

TEST(unchanged_frames_wrap_exactly_once) {
  ti::TextLayoutCache c;
  g_measures = 0;
  c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  const int one_wrap = g_measures;
  CHECK(one_wrap > 0);
  const size_t lines = c.line_count();
  for (int frame = 0; frame < 29; ++frame)
    c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  CHECK(g_measures == one_wrap);
  CHECK(c.line_count() == lines);
}

TEST(cached_lines_are_byte_for_byte_identical) {
  ti::TextLayoutCache fresh, cached;
  fresh.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  cached.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  cached.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  CHECK(cached.line_count() == fresh.line_count());
  for (size_t i = 0; i < fresh.line_count(); ++i) {
    CHECK(cached.line(i).source_offset == fresh.line(i).source_offset);
    CHECK(cached.line(i).length == fresh.line(i).length);
    CHECK(cached.line_text(SRC, i) == fresh.line_text(SRC, i));
  }
  CHECK_APPROX(cached.total_height(), fresh.total_height());
}

TEST(edit_rewraps_exactly_once) {
  ti::TextLayoutCache c;
  c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  g_measures = 0;
  const std::string edited = SRC + " more";
  c.rebuild(edited, 110.f, LINE_H, measure10, 1);
  const int one_wrap = g_measures;
  CHECK(one_wrap > 0);
  c.rebuild(edited, 110.f, LINE_H, measure10, 1);
  CHECK(g_measures == one_wrap);
}

TEST(resize_rewraps_exactly_once) {
  ti::TextLayoutCache c;
  c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  g_measures = 0;
  c.rebuild(SRC, 60.f, LINE_H, measure10, 1);
  const int one_wrap = g_measures;
  CHECK(one_wrap > 0);
  c.rebuild(SRC, 60.f, LINE_H, measure10, 1);
  CHECK(g_measures == one_wrap);
}

TEST(line_height_change_rewraps) {
  ti::TextLayoutCache c;
  c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  g_measures = 0;
  c.rebuild(SRC, 110.f, 30.f, measure10, 1);
  CHECK(g_measures > 0);
  CHECK_APPROX(c.total_height(), 30.f * static_cast<float>(c.line_count()));
}

// A new font at the same name/size is a new measure key: stale breaks from
// the old face must not be served.
TEST(measure_key_change_rewraps) {
  ti::TextLayoutCache c;
  c.rebuild(SRC, 110.f, LINE_H, measure10, 1);
  const size_t wide_lines = c.line_count();
  g_measures = 0;
  c.rebuild(SRC, 110.f, LINE_H, measure5, 2);
  CHECK(g_measures > 0);
  CHECK(c.line_count() != wide_lines);
}

int main() { return ui_test::run_registered_tests("text_area_wrap_cache"); }
