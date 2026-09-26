// text_memo_test.cpp
// The caches in front of text measurement: the wrap memo, the backend
// measure memo and the shared TextMeasureCache. A cached answer is only
// valid for the exact question -- same text, same face, same size, same
// spacing -- and a font reload changes every answer at once.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/rendering.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// The wrap memo's key used to be runs + width only, so the same styled text
// at a different font size was served the first size's line breaks.
TEST(wrap_memo_does_not_serve_another_font_sizes_lines) {
  const std::vector<TextSpan> spans{{"zzq wwq vvq", Color{210, 220, 230, 255}}};
  auto render_at = [&](bool batched, float font_px) {
    ui_test::ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size({pixels(100), pixels(48)})
            .with_absolute_position(40, 50)
            .with_styled_label(spans)
            .with_font_size(pixels(font_px))
            .with_text_overflow(TextOverflow::Wrap)
            .with_text_inset(0));
    if (batched) h.render_batched();
    else h.render();
    return h.drawn("text").size();
  };
  for (bool batched : {false, true}) {
    // 11 chars at 10px/char = 110px in a 100px box wraps; at 5px/char it fits.
    CHECK(render_at(batched, 20.f) == 2);
    CHECK(render_at(batched, 10.f) == 1);
  }
}

// Reloading a font under a name it already holds must drop the measurements
// taken against the old face, including the shared TextMeasureCache.
TEST(font_reload_invalidates_cached_measurements) {
  ui_test::ImmTestHarness h;
  auto *cache = EntityHelper::get_singleton_cmp<TextMeasureCache>();
  CHECK(cache != nullptr);
  if (!cache) return;
  cache->clear();
  (void)cache->measure("reload probe", "face", 20.f);
  (void)cache->measure("reload probe", "face", 20.f);
  CHECK(cache->hits() == 1);

  FontManager fm;
  fm.load_font("face", Font{});
  fm.load_font("face", Font{}); // the reload
  const auto misses_before = cache->misses();
  (void)cache->measure("reload probe", "face", 20.f);
  CHECK(cache->misses() == misses_before + 1);
}

// Styled-run placement. Backend measurement charges per call (spacing and
// kerning live between characters), so a run must start where the JOINED
// line's measurement puts it, not at the sum of the runs before it. In a
// monospace block the accumulated version visibly breaks the columns.
TEST(styled_runs_take_offsets_from_the_joined_line) {
  for (bool batched : {false, true}) {
    ui_test::ImmTestHarness h;
#ifdef AFTER_HOURS_BACKEND_NONE
    set_measure_text_fn([](const char *text, float font_size, float) {
      return Vector2Type{static_cast<float>(std::string_view(text ? text : "")
                                                .size()) *
                                 font_size * 0.5f +
                             6.f,
                         font_size};
    });
#endif
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size({pixels(300), pixels(48)})
            .with_absolute_position(40, 50)
            .with_styled_label({{"aa", Color{255, 0, 0, 255}},
                                {"bb", Color{0, 255, 0, 255}},
                                {"cc", Color{0, 0, 255, 255}}})
            .with_font_size(pixels(20))
            .with_text_inset(0)
            .with_alignment(TextAlignment::Left));
    if (batched) h.render_batched();
    else h.render();
    auto calls = h.drawn("text");
    CHECK(calls.size() == 3);
    if (calls.size() != 3) continue;
    // Joined prefixes at 10px/char + 6 per call: "bb" starts at
    // measure("aa") = 26, "cc" at measure("aabb") = 46 -- not the
    // accumulated 26 + measure("bb") = 52.
    CHECK_APPROX(calls[0].rect.x, 40.f);
    CHECK_APPROX(calls[1].rect.x, 66.f);
    CHECK_APPROX(calls[2].rect.x, 86.f);
  }
}

// The coverage query (hanabi #48). Under the none backend no face is loaded,
// so every codepoint reports missing; what is pinned here is the decoding,
// the dedup and the order. Real coverage is pinned against a real cmap in
// font_atlas_measure_test (sokol) and by the WM demo (raylib).
TEST(missing_codepoints_decodes_dedups_and_orders) {
  FontManager fm;
  CHECK(!fm.has_glyph("any", 'a'));
  const auto missing =
      fm.missing_codepoints("any", "\xe2\x86\xb5" "a" "\xe2\x86\xb5"
                                   "\xe2\x98\x85" "\xf0\x9f\x8e\x89");
  CHECK(missing.size() == 4);
  if (missing.size() != 4) return;
  CHECK(missing[0] == 0x21B5);
  CHECK(missing[1] == static_cast<uint32_t>('a'));
  CHECK(missing[2] == 0x2605);
  CHECK(missing[3] == 0x1F389);
  CHECK(fm.missing_codepoints("any", "").empty());
}

TEST(wrap_memo_key_covers_face_size_and_spacing) {
  namespace wm = afterhours::ui::detail::wrap_memo;
  const std::vector<TextSpan> runs{{"same text", Color{1, 2, 3, 255}}};
  const auto base = wm::key_for(runs, 100.f, 20.f, 1.f, "Face");
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.f, "Face") == base);
  CHECK(wm::key_for(runs, 100.f, 21.f, 1.f, "Face") != base);
  CHECK(wm::key_for(runs, 100.f, 20.f, 2.f, "Face") != base);
  CHECK(wm::key_for(runs, 100.f, 20.f, 1.f, "Other") != base);
  CHECK(wm::key_for(runs, 101.f, 20.f, 1.f, "Face") != base);
}

TEST(wrap_memo_entries_die_with_the_measure_generation) {
  namespace wm = afterhours::ui::detail::wrap_memo;
  using afterhours::ui::detail::TextRunLine;
  const std::vector<TextSpan> runs{{"generation probe", Color{9, 9, 9, 255}}};
  const auto key = wm::key_for(runs, 100.f, 20.f, 1.f, "Face");
  wm::store(key, {TextRunLine{TextSpan{"generation probe", Color{}}}});
  CHECK(wm::lookup(key) != nullptr);
  afterhours::measure_memo::invalidate();
  CHECK(wm::lookup(key) == nullptr);
}

int main() { return ui_test::run_registered_tests("text memo"); }
