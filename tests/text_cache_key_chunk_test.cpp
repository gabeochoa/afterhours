
#include "ui_test_harness.h"

#include <afterhours/src/core/text_cache.h>
#include <afterhours/src/plugins/ui/rendering.h>
#include <afterhours/src/plugins/ui/text_measure.h>

using namespace afterhours;
using namespace afterhours::ui;

static auto measure10 = [](const std::string &s) {
  return static_cast<float>(s.size()) * 10.f;
};

TEST(typed_wrap_key_separates_previously_colliding_widths_and_sizes) {
  namespace wm = afterhours::ui::detail::wrap_memo;
  const std::vector<TextSpan> runs{{"hello world", Color{1, 2, 3, 255}}};
  const auto base = wm::key_for(runs, 100.f, 20.f, 1.f, "Face");
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.f, "Face") == base);
  CHECK(wm::key_for(runs, 100.01f, 20.f, 1.f, "Face") != base);
  CHECK(wm::key_for(runs, 100.f, 20.01f, 1.f, "Face") != base);
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.01f, "Face") != base);
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.f, "Other") != base);
  const std::vector<TextSpan> split{{"ab", Color{1, 2, 3, 255}},
                                    {"c", Color{1, 2, 3, 255}}};
  const std::vector<TextSpan> joined{{"abc", Color{1, 2, 3, 255}}};
  CHECK(wm::key_for(split, 100.f, 20.f, 1.f, "Face") !=
        wm::key_for(joined, 100.f, 20.f, 1.f, "Face"));
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.f, "Face", 1) !=
        wm::key_for(runs, 100.f, 20.f, 1.f, "Face", 2));
}

TEST(wrap_memo_state_and_width_serve_only_their_own_lines) {
  namespace wm = afterhours::ui::detail::wrap_memo;
  wm::clear();
  const std::vector<TextSpan> runs{{"aa bb cc", Color{9, 9, 9, 255}}};
  const auto k_state1 = wm::key_for(runs, 50.f, 20.f, 1.f, "Face", 1);
  const auto k_state2 = wm::key_for(runs, 50.f, 20.f, 1.f, "Face", 2);
  const auto k_wider = wm::key_for(runs, 50.01f, 20.f, 1.f, "Face", 1);
  wm::store(k_state1, {{{"sentinel", Color{9, 9, 9, 255}}}});
  CHECK(wm::lookup(k_state1) != nullptr);
  CHECK(wm::lookup(k_state2) == nullptr);
  CHECK(wm::lookup(k_wider) == nullptr);
  wm::clear();
}

TEST(typed_measure_key_distinguishes_text_font_boundary) {
  int calls = 0;
  TextMeasureCache cache([&](std::string_view text, std::string_view font,
                             float size, float) {
    calls++;
    return Vector2Type{static_cast<float>(text.size()) * 10.f +
                           static_cast<float>(font.size()),
                       size};
  });
  const auto a = TextMeasureCache::Key::make("ab", "c", 20.f, 1.f);
  const auto b = TextMeasureCache::Key::make("abc", "", 20.f, 1.f);
  CHECK(a != b);
  CHECK(cache.measure(a).x == 21.f);
  CHECK(cache.measure(b).x == 30.f);
  CHECK(cache.measure(a).x == 21.f);
  CHECK(calls == 2);
  CHECK(cache.measure(TextMeasureCache::Key::make("ab", "c", 20.01f, 1.f)).x ==
        21.f);
  CHECK(calls == 3);
}

static void check_chunking(const std::string &whole,
                           const std::vector<std::string> &chunks) {
  auto expected = afterhours::ui::detail::wrap_lines(whole, 70.f, measure10);
  afterhours::ui::detail::WrapChunkState state;
  std::vector<afterhours::ui::detail::WrappedLine> got;
  for (const auto &chunk : chunks) {
    auto lines = afterhours::ui::detail::wrap_chunk(state, chunk, 70.f, measure10);
    got.insert(got.end(), lines.begin(), lines.end());
  }
  auto tail = afterhours::ui::detail::finish_wrap_chunk(state, 70.f, measure10);
  got.insert(got.end(), tail.begin(), tail.end());
  CHECK(got.size() == expected.size());
  for (size_t i = 0; i < got.size() && i < expected.size(); i++) {
    CHECK(got[i].begin == expected[i].begin);
    CHECK(got[i].end == expected[i].end);
    CHECK(got[i].width == expected[i].width);
  }
}

TEST(chunk_resume_matches_whole_text_wrap) {
  const std::string whole = "aaa bbb ccc\ndd ee\n\nlast word here";
  check_chunking(whole, {whole});
  check_chunking(whole, {"aaa b", "bb cc", "c\ndd ", "ee\n", "\nlast wo",
                         "rd here"});
  check_chunking(whole, {"", "aaa bbb ccc\n", "", "dd ee\n\nlast word here"});
  std::vector<std::string> bytes;
  for (char c : whole)
    bytes.emplace_back(1, c);
  check_chunking(whole, bytes);
  check_chunking("trailing newline\n", {"trailing new", "line\n"});
  check_chunking("", {""});
  check_chunking("", {});
}

int main() { return ui_test::run_registered_tests("text cache key/chunk"); }
