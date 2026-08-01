// theme_io_test.cpp
// Covers ui::theme_io -- reading/writing Theme as a text file.
//
// Two things this is really guarding:
//   1. Round-trip fidelity. A save/load cycle that quietly drops a field turns
//      "tweak the theme file" into "lose the rest of your theme".
//   2. Partial + malformed input. The file is hand-edited, so a typo must cost
//      one property and be reported -- never silently reset to defaults.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/theme_io.h>

using namespace afterhours;
using namespace afterhours::ui;
namespace tio = afterhours::ui::theme_io;

static bool same_color(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

TEST(color_hex_round_trip) {
  auto c = tio::parse_color("#0C1B33");
  CHECK(c.has_value());
  CHECK(c && same_color(*c, Color{12, 27, 51, 255}));
  CHECK(tio::format_color(Color{12, 27, 51, 255}) == "#0C1B33FF");

  // Explicit alpha survives.
  auto with_alpha = tio::parse_color("#0C1B3380");
  CHECK(with_alpha && with_alpha->a == 128);
  // The leading '#' is optional; lowercase hex works.
  auto bare = tio::parse_color("0c1b33");
  CHECK(bare && same_color(*bare, Color{12, 27, 51, 255}));

  // Rejected: wrong length (#RGB shorthand included) and non-hex digits.
  CHECK(!tio::parse_color("#FFF").has_value());
  CHECK(!tio::parse_color("#GGGGGG").has_value());
  CHECK(!tio::parse_color("").has_value());
}

TEST(number_parsing_rejects_trailing_garbage) {
  CHECK(tio::parse_float("0.5").value_or(-1.f) == 0.5f);
  CHECK(tio::parse_float("2").value_or(-1.f) == 2.f);
  // stof alone would return 1.0 here and drop the rest silently.
  CHECK(!tio::parse_float("1.0abc").has_value());
  CHECK(!tio::parse_float("abc").has_value());
  CHECK(!tio::parse_float("").has_value());
}

// The point of the Usage-driven writer: every valid usage reaches the file and
// comes back. A usage added to the enum is covered without editing theme_io.
TEST(full_round_trip_preserves_every_field) {
  Theme original;
  unsigned char tick = 1;
  for (Theme::Usage usage : magic_enum::enum_values<Theme::Usage>()) {
    if (!Theme::is_valid(usage))
      continue;
    original.set_color(usage, Color{tick, static_cast<unsigned char>(tick + 1),
                                    static_cast<unsigned char>(tick + 2),
                                    static_cast<unsigned char>(200 + tick)});
    tick = static_cast<unsigned char>(tick + 10);
  }
  original.roundness = 0.25f;
  original.focus_ring_thickness = 5.f;
  original.focus_ring_offset = 1.5f;
  original.disabled_opacity = 0.75f;
  original.ui_scale = 1.5f;
  original.click_activation_mode = ClickActivationMode::Release;
  original.highlight_mode = HighlightMode::FollowsMostRecentInput;

  // Start from a theme that differs everywhere, so a field the writer skips
  // shows up as a mismatch instead of coincidentally already being right.
  Theme restored;
  for (Theme::Usage usage : magic_enum::enum_values<Theme::Usage>())
    if (Theme::is_valid(usage))
      restored.set_color(usage, Color{0, 0, 0, 0});
  restored.roundness = 99.f;
  restored.ui_scale = 99.f;

  auto result = tio::read_into(tio::to_string(original), restored);
  CHECK(result.errors == 0);

  for (Theme::Usage usage : magic_enum::enum_values<Theme::Usage>()) {
    if (!Theme::is_valid(usage))
      continue;
    bool match =
        same_color(original.color_ref(usage), restored.color_ref(usage));
    if (!match)
      fprintf(stderr, "        color mismatch: %s\n",
              std::string(magic_enum::enum_name(usage)).c_str());
    CHECK(match);
  }
  CHECK(restored.roundness == 0.25f);
  CHECK(restored.focus_ring_thickness == 5.f);
  CHECK(restored.focus_ring_offset == 1.5f);
  CHECK(restored.disabled_opacity == 0.75f);
  CHECK(restored.ui_scale == 1.5f);
  CHECK(restored.click_activation_mode == ClickActivationMode::Release);
  CHECK(restored.highlight_mode == HighlightMode::FollowsMostRecentInput);

  // Every field the writer emits must be one the reader recognizes; an
  // unreadable key would land in `errors`, which is already asserted above.
  CHECK(result.applied > 0);
}

TEST(partial_file_leaves_other_fields_alone) {
  Theme theme;
  const Color untouched = theme.accent;
  theme.roundness = 0.9f;

  auto result = tio::read_into("primary = #112233\n", theme);
  CHECK(result.applied == 1);
  CHECK(result.errors == 0);
  CHECK(same_color(theme.primary, Color{0x11, 0x22, 0x33, 255}));
  CHECK(same_color(theme.accent, untouched));
  CHECK(theme.roundness == 0.9f);
}

TEST(key_matching_ignores_case_and_underscores) {
  Theme theme;
  auto result = tio::read_into("font_muted = #010203\n"
                           "FontMuted  = #040506\n"
                           "fontmuted  = #070809\n"
                           "  BACKGROUND=#0A0B0C\n",
                           theme);
  CHECK(result.errors == 0);
  CHECK(result.applied == 4);
  CHECK(same_color(theme.font_muted, Color{7, 8, 9, 255})); // last wins
  CHECK(same_color(theme.background, Color{0x0A, 0x0B, 0x0C, 255}));
}

TEST(enum_values_parse_by_name) {
  Theme theme;
  auto result = tio::read_into("click_activation_mode = release\n"
                           "highlight_mode = FollowsMostRecentInput\n",
                           theme);
  CHECK(result.errors == 0);
  CHECK(theme.click_activation_mode == ClickActivationMode::Release);
  CHECK(theme.highlight_mode == HighlightMode::FollowsMostRecentInput);

  auto bad = tio::read_into("highlight_mode = Sparkles\n", theme);
  CHECK(bad.errors == 1);
  // Rejected, not defaulted -- the previous value stands.
  CHECK(theme.highlight_mode == HighlightMode::FollowsMostRecentInput);
}

TEST(comments_and_blank_lines_are_skipped) {
  Theme theme;
  auto result = tio::read_into("# a comment\n"
                           "\n"
                           "   \n"
                           "primary = #112233   # trailing comment\n"
                           "# background = #FFFFFF\n",
                           theme);
  CHECK(result.errors == 0);
  CHECK(result.applied == 1);
  // A '#' after the '=' is part of the color, not a comment -- otherwise every
  // color line would parse as empty.
  CHECK(same_color(theme.primary, Color{0x11, 0x22, 0x33, 255}));
  // The commented-out line must not apply.
  CHECK(!same_color(theme.background, Color{255, 255, 255, 255}));
}

// One typo costs one property. The alternative -- bailing on the file -- would
// swap in a whole default theme over a single mistyped line.
TEST(malformed_lines_are_reported_and_isolated) {
  Theme theme;
  auto result = tio::read_into("primary = #112233\n"
                           "secondary = notacolor\n"
                           "roundness = 1.0oops\n"
                           "no_equals_sign\n"
                           "nonsense_key = 5\n"
                           "accent = #445566\n",
                           theme);
  CHECK(result.errors == 4);
  CHECK(result.applied == 2);
  // The good lines on both sides of the bad ones still landed.
  CHECK(same_color(theme.primary, Color{0x11, 0x22, 0x33, 255}));
  CHECK(same_color(theme.accent, Color{0x44, 0x55, 0x66, 255}));
}

// Custom/Default/None are markers rather than colors; writing them would emit
// keys that read back as junk, and color_ref maps them all onto primary.
TEST(marker_usages_are_not_serialized) {
  Theme theme;
  const std::string text = tio::to_string(theme);
  CHECK(text.find("custom") == std::string::npos);
  CHECK(text.find("default") == std::string::npos);
  CHECK(text.find("none") == std::string::npos);
}

TEST(save_and_load_via_disk) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "afterhours_theme_io_test.theme";
  std::filesystem::remove(path);

  Theme original;
  original.primary = Color{1, 2, 3, 4};
  original.roundness = 0.125f;
  CHECK(tio::save(original, path));

  auto loaded = tio::load(path);
  CHECK(loaded.has_value());
  CHECK(loaded && same_color(loaded->primary, Color{1, 2, 3, 4}));
  CHECK(loaded && loaded->roundness == 0.125f);

  // A missing file is distinguishable from an empty one.
  std::filesystem::remove(path);
  CHECK(!tio::load(path).has_value());
}

int main() { return ui_test::run_registered_tests("theme_io tests"); }
