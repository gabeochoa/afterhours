// text_fallback_test.cpp
// Font fallback chains and the styled-text layout, against real fonts on the
// sokol backend, with every drawn glyph captured.
//
// fontstash runs on a CPU-only context whose renderDraw records each glyph
// quad, so "what was drawn" is exact geometry, no GPU and no window. The
// draws go through afterhours::draw_text_ex, the same call the renderers make.
//
// Fonts: the two fixtures (Atkinson primary, Archivo Narrow) plus the first
// system CJK face (system_fonts::cjk(), a .ttc on macOS) and, for U+FFFD and
// dingbats, the first system symbols face. macOS-only, like every sokol test.
// Not on ui_test_harness.h: that pulls in rendering.h, which does not build
// under sokol at this commit (begin_shader_mode is ambiguous; sokol_blend_test
// fails the same way).

#define AFTER_HOURS_USE_METAL

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <afterhours/src/plugins/ui/styled_text.h>
#include <afterhours/src/system_fonts.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace md = afterhours::graphics::metal_detail;
namespace font_coverage = afterhours::font_coverage;
namespace system_fonts = afterhours::system_fonts;
namespace text_selection = afterhours::ui::text_selection;
namespace uid = afterhours::ui::detail;
namespace colors = afterhours::colors;
using afterhours::Color;
using afterhours::Font;
using afterhours::draw_text_ex;
using afterhours::font_fallbacks;
using afterhours::font_has_glyph;
using afterhours::load_font_face;
using afterhours::load_font_from_file;
using afterhours::measure_text;
using afterhours::ui::FontManager;
using afterhours::ui::StyledFonts;
using afterhours::ui::StyledLayout;
using afterhours::ui::TextSpan;
using afterhours::ui::layout_styled_text;
using afterhours::ui::styled_selection_rects;
using afterhours::ui::utf8_char_length;
namespace font_detail = afterhours::font_detail;

static int checks = 0, failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    checks++;                                                                  \
    if (!(cond)) {                                                             \
      failures++;                                                              \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
    }                                                                          \
  } while (0)

static std::vector<std::pair<const char *, void (*)()>> &tests() {
  static std::vector<std::pair<const char *, void (*)()>> t;
  return t;
}
#define TEST(name)                                                             \
  static void test_##name();                                                   \
  static const bool registered_##name =                                        \
      (tests().push_back({#name, test_##name}), true);                         \
  static void test_##name()

namespace {

struct Quad {
  float x0, y0, x1, y1;
  bool operator==(const Quad &o) const {
    return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1;
  }
};
std::vector<Quad> g_quads;

void capture(void *, const float *v, const float *, const unsigned int *,
             int n) {
  for (int i = 0; i + 6 <= n; i += 6)
    g_quads.push_back({v[i * 2], v[i * 2 + 1], v[i * 2 + 2], v[i * 2 + 3]});
}

struct Fonts {
  Font ui, narrow, cjk, symbols;
} F;
FontManager *g_fm = nullptr;

const Color kWhite{255, 255, 255, 255};
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};
constexpr float kScales[] = {1.0f, 1.4f, 2.0f};

std::string fixture(const char *name) {
  return (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / name).string();
}

std::vector<Quad> draw(Font f, const std::string &s, float x, float y,
                       float size) {
  g_quads.clear();
  draw_text_ex(f, s.c_str(), Vector2Type{x, y}, size, 1.f, kWhite);
  return g_quads;
}

// fonsDrawText's own advance for what draw_text_ex hands it.
float drawn_advance(Font f, const std::string &s, float size) {
  std::string scratch;
  const char *visible = font_detail::visible(f.id, s.c_str(), scratch);
  fonsSetFont(md::g_fons_ctx, f.id);
  fonsSetSize(md::g_fons_ctx, size);
  fonsSetAlign(md::g_fons_ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  const float end = fonsDrawText(md::g_fons_ctx, 0.f, 0.f, visible, nullptr);
  g_quads.clear();
  return end;
}

// Where draw_text_ex put a trailing '|' against where measurement says its
// pen is: the advance of everything before it, as drawn versus as measured.
bool sentinel_lands(Font f, const std::string &s, float size) {
  const auto with = draw(f, s + "|", 0, 0, size);
  const auto alone = draw(f, "|", 0, 0, size);
  if (with.empty() || alone.empty())
    return false;
  const float drawn = with.back().x0 - alone[0].x0;
  const float measured = measure_text(f, (s + "|").c_str(), size, 1).x -
                         measure_text(f, "|", size, 1).x;
  if (drawn != measured)
    std::fprintf(stderr, "        '|' drawn at %.1f, measured %.1f\n", drawn,
                 measured);
  return drawn == measured;
}

size_t codepoints(const std::string &s) {
  size_t n = 0;
  for (unsigned char c : s)
    n += (c & 0xC0) != 0x80;
  return n;
}

void chain(std::vector<std::string> names) {
  g_fm->set_fallbacks("ui", std::move(names));
}

StyledFonts metrics_at(float scale) {
  return StyledFonts{*g_fm, "ui", colors::FontWeight::Regular, 16.f * scale,
                     1.f};
}

// Each placed run drawn where the layout put it, quads in order.
std::vector<Quad> draw_layout(const StyledLayout &layout) {
  std::vector<Quad> all;
  for (const auto &run : layout.runs) {
    const auto q = draw(g_fm->get_font(run.font_name), run.text, run.rect.x,
                        run.rect.y, run.font_size);
    all.insert(all.end(), q.begin(), q.end());
  }
  return all;
}

} // namespace

TEST(coverage_queries_answer_per_face_and_per_chain) {
  chain({});
  CHECK(font_has_glyph(F.ui, 'A'));
  CHECK(!font_has_glyph(F.ui, 0x65E5)); // 日
  CHECK(font_has_glyph(F.cjk, 0x65E5));
  CHECK(!font_has_glyph(F.cjk, 0x1F600));
  CHECK(g_fm->face_for("ui", 0x65E5).empty());
  chain({"narrow", "cjk"});
  CHECK(g_fm->face_for("ui", 'A') == "ui");
  CHECK(g_fm->face_for("ui", 0x65E5) == "cjk");
  CHECK(!g_fm->has_glyph("ui", 0x65E5)); // has_glyph stays per face
  const auto missing = g_fm->missing_codepoints("ui", "a\xE6\x97\xA5\xF0\x9F\x98\x80");
  CHECK(missing.size() == 1 && missing[0] == 0x1F600);
  const auto listed = font_fallbacks(F.ui);
  CHECK(listed.size() == 2 && listed[0].id == F.narrow.id &&
        listed[1].id == F.cjk.id);
}

TEST(the_first_face_in_the_chain_that_has_a_glyph_draws_it) {
  // A two-byte codepoint both fallbacks have at different widths, so the
  // measurement says which one answered.
  const auto utf8 = [](int c) {
    return std::string{static_cast<char>(0xC0 | (c >> 6)),
                       static_cast<char>(0x80 | (c & 63))};
  };
  int cp = 0;
  for (int c = 0x80; c < 0x800 && !cp; c++)
    if (!font_has_glyph(F.ui, c) && font_has_glyph(F.narrow, c) &&
        font_has_glyph(F.cjk, c) &&
        measure_text(F.narrow, utf8(c).c_str(), 32, 1).x !=
            measure_text(F.cjk, utf8(c).c_str(), 32, 1).x)
      cp = c;
  CHECK(cp != 0);
  if (!cp)
    return;
  const std::string s = utf8(cp);
  fprintf(stderr, "        U+%04X: in both fallbacks, not the primary\n", cp);
  chain({"narrow", "cjk"});
  const float via_narrow = measure_text(F.ui, s.c_str(), 32, 1).x;
  CHECK(via_narrow == measure_text(F.narrow, s.c_str(), 32, 1).x);
  CHECK(draw(F.ui, s, 0, 0, 32)[0].x1 == draw(F.narrow, s, 0, 0, 32)[0].x1);
  chain({"cjk", "narrow"}); // replaces, does not append
  const float via_cjk = measure_text(F.ui, s.c_str(), 32, 1).x;
  CHECK(via_cjk == measure_text(F.cjk, s.c_str(), 32, 1).x);
  CHECK(via_cjk != via_narrow);
  CHECK(draw(F.ui, s, 0, 0, 32)[0].x1 == draw(F.cjk, s, 0, 0, 32)[0].x1);
}

TEST(collection_faces_load_by_index) {
  CHECK(F.cjk.id != FONS_INVALID);
  const auto faces = system_fonts::cjk();
  CHECK(!faces.empty());
  if (faces.empty())
    return;
  CHECK(load_font_face(faces[0].path.c_str(), 999).id == FONS_INVALID);
  std::vector<unsigned char> plain;
  CHECK(font_coverage::read_file(fixture("ArchivoNarrow-Regular.ttf").c_str(),
                                 plain));
  CHECK(font_coverage::extract_face(plain, 0) == plain);
  CHECK(font_coverage::extract_face(plain, 1).empty());
}

TEST(measurement_equals_the_drawn_advance_with_mixed_latin_and_cjk) {
  chain({"narrow", "cjk"});
  const std::string text = "Hello \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E "
                           "w\xC3\xB6rld \xE3\x81\xB2\xE3\x82\x89 AVA";
  for (float scale : kScales) {
    const float size = 16.f * scale;
    const float measured = measure_text(F.ui, text.c_str(), size, 1).x;
    CHECK(measured == drawn_advance(F.ui, text, size));
    CHECK(sentinel_lands(F.ui, text, size));
    const auto quads = draw(F.ui, text, 0, 0, size);
    CHECK(quads.size() == codepoints(text)); // nothing dropped
    // The CJK glyphs came from the fallback with ink, not as empty boxes.
    int inked = 0;
    for (size_t i : {6u, 7u, 8u, 16u, 17u})
      inked += i < quads.size() && quads[i].x1 - quads[i].x0 > 6.f;
    CHECK(inked == 5);
    fprintf(stderr, "        scale %.1f: measured %.0f, drawn %.0f\n", scale,
            measured, drawn_advance(F.ui, text, size));
  }
}

TEST(uncovered_emoji_draw_one_visible_replacement_each) {
  chain({"narrow", "cjk"}); // nothing here has U+FFFD
  const auto w = [](const char *s) { return measure_text(F.ui, s, 24, 1).x; };
  CHECK(w("a\xF0\x9F\x98\x80" "b") == w("a?b")); // 😀
  // man ZWJ woman ZWJ girl: one replacement, not five glyphs.
  CHECK(w("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
          "\xF0\x9F\x91\xA7") == w("?"));
  CHECK(w("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5") == w("?")); // flag JP
  CHECK(w("x\xEF\xB8\x8F") == w("x"));                     // VS16 alone
  const auto quads = draw(F.ui, "a\xF0\x9F\x98\x80" "b", 0, 0, 24);
  CHECK(quads.size() == 3);
  CHECK(quads.size() == 3 && quads[1].x1 - quads[1].x0 > 6.f); // visible
  CHECK(w("a\xF0\x9F\x98\x80" "b") ==
        drawn_advance(F.ui, "a\xF0\x9F\x98\x80" "b", 24));
  // Drawn exactly as the replacement it was measured as.
  CHECK(quads == draw(F.ui, "a?b", 0, 0, 24));
  CHECK(sentinel_lands(F.ui, "a\xF0\x9F\x98\x80" "b", 24));

  if (F.symbols.id == FONS_INVALID) {
    fprintf(stderr, "        SKIP U+FFFD: no system symbols face\n");
    return;
  }
  chain({"narrow", "cjk", "symbols"});
  CHECK(w("a\xF0\x9F\x98\x80" "b") == w("a\xEF\xBF\xBD" "b"));
  // ❤️ = U+2764 U+FE0F: the heart draws from the symbols face, the selector
  // vanishes instead of drawing a box.
  CHECK(w("\xE2\x9D\xA4\xEF\xB8\x8F") == w("\xE2\x9D\xA4"));
  CHECK(draw(F.ui, "\xE2\x9D\xA4\xEF\xB8\x8F", 0, 0, 24).size() == 1);
}

namespace {
const std::vector<TextSpan> &styled_runs() {
  static const std::vector<TextSpan> runs{
      TextSpan{"Total ", kWhite},
      TextSpan{"\xE5\x90\x88\xE8\xA8\x88 ", kRed}, // 合計, fallback face
      TextSpan{"AVA ", kBlue, colors::FontWeight::Regular, "narrow"},
      TextSpan{"12\xE4\xBB\xB6 \xF0\x9F\x98\x80 ", kWhite,
               colors::FontWeight::Regular, "", 1.5f}, // 12件 😀, larger
      TextSpan{"done", kRed},
  };
  return runs;
}
} // namespace

// Runs drawn one by one at the layout's x land every glyph exactly where one
// call per same-style stretch puts it, and the layout's width is the drawn
// advance.
TEST(styled_runs_draw_where_one_call_would_at_three_scales) {
  chain({"narrow", "cjk"});
  for (float scale : kScales) {
    const auto metrics = metrics_at(scale);
    const auto layout = layout_styled_text(styled_runs(), metrics, 1e9f);
    CHECK(layout.lines.size() == 1);
    const auto per_run = draw_layout(layout);
    std::vector<Quad> per_stretch;
    float end = 0.f;
    const auto &runs = layout.runs;
    for (size_t i = 0; i < runs.size();) {
      std::string joined;
      size_t j = i;
      while (j < runs.size() && runs[j].font_name == runs[i].font_name &&
             runs[j].font_size == runs[i].font_size)
        joined += runs[j++].text;
      const Font f = g_fm->get_font(runs[i].font_name);
      const auto q = draw(f, joined, runs[i].rect.x, runs[i].rect.y,
                          runs[i].font_size);
      per_stretch.insert(per_stretch.end(), q.begin(), q.end());
      end = runs[i].rect.x + drawn_advance(f, joined, runs[i].font_size);
      i = j;
    }
    CHECK(per_run == per_stretch);
    CHECK(layout.width == end);
    // One baseline for every run, whatever its face or size.
    float base = runs[0].rect.y + metrics.baseline(styled_runs()[0]);
    bool shared = true;
    for (size_t i = 0; i < runs.size(); i++) {
      TextSpan style;
      style.font = runs[i].font_name == "narrow" ? "narrow" : "";
      style.size_scale = runs[i].font_size / metrics.font_size;
      shared = shared &&
               std::abs(runs[i].rect.y + metrics.baseline(style) - base) <
                   0.01f;
    }
    CHECK(shared);
    fprintf(stderr, "        scale %.1f: %zu glyphs, width %.0f\n", scale,
            per_run.size(), layout.width);
  }
}

// A selection edge at byte k is glyph k's drawn origin: its quad minus its
// own bearing (the quad it draws alone at x = 0).
TEST(selection_edges_are_drawn_glyph_origins_at_three_scales) {
  chain({"narrow", "cjk"});
  for (float scale : kScales) {
    const auto metrics = metrics_at(scale);
    const auto layout = layout_styled_text(styled_runs(), metrics, 1e9f);
    const auto quads = draw_layout(layout);
    const std::string text = text_selection::line_text(layout.lines[0]);
    size_t glyph = 0, mismatches = 0, checked = 0;
    size_t run_index = 0, run_start = 0;
    for (size_t k = 0; k < text.size(); glyph++) {
      while (k >= run_start + layout.runs[run_index].text.size())
        run_start += layout.runs[run_index++].text.size();
      const auto &run = layout.runs[run_index];
      const size_t len = utf8_char_length(text, k);
      const auto rects = styled_selection_rects(
          layout, text_selection::Range{k, k + len}, Vector2Type{0, 0},
          metrics);
      const auto alone = draw(g_fm->get_font(run.font_name),
                              text.substr(k, len), 0, 0, run.font_size);
      if (rects.size() != 1 || alone.size() != 1 || glyph >= quads.size()) {
        mismatches++;
      } else {
        checked++;
        if (rects[0].x != quads[glyph].x0 - alone[0].x0) {
          mismatches++;
          fprintf(stderr, "        byte %zu: selection %.2f, glyph %.2f\n", k,
                  rects[0].x, quads[glyph].x0 - alone[0].x0);
        }
      }
      k += len;
    }
    CHECK(checked == codepoints(text));
    CHECK(mismatches == 0);
  }
}

TEST(wrap_breaks_where_measurement_says_at_three_scales) {
  chain({"narrow", "cjk"});
  const std::vector<TextSpan> runs{TextSpan{
      "Fallback \xE6\x96\x87\xE5\xAD\x97\xE3\x82\x82 measured "
      "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE\xE6\x96\x87"
      "\xE7\xAB\xA0\xE3\x80\x82 and drawn alike, wrapping cleanly",
      kWhite}};
  for (float scale : kScales) {
    const auto metrics = metrics_at(scale);
    const float width = 140.f * scale;
    const auto layout = layout_styled_text(runs, metrics, width);
    CHECK(layout.lines.size() > 2);
    bool fits = true, maximal = true, drawn = true;
    for (size_t i = 0; i < layout.lines.size(); i++) {
      const auto &line = layout.lines[i];
      const float w = uid::line_width(line, metrics);
      const std::string t = text_selection::line_text(line);
      fits = fits && w <= width;
      drawn = drawn && w == drawn_advance(F.ui, t, metrics.font_size);
      if (i + 1 < layout.lines.size()) {
        // The next line's first piece would not have fit here.
        const std::string next = text_selection::line_text(layout.lines[i + 1]);
        size_t cut = next.find(' ');
        size_t first = utf8_char_length(next, 0);
        if (uid::breaks_around(font_coverage::decode_utf8(
                reinterpret_cast<const unsigned char *>(next.c_str()), first)))
          cut = first;
        const std::string sep =
            uid::breaks_around(font_coverage::decode_utf8(
                reinterpret_cast<const unsigned char *>(next.c_str()), first))
                ? ""
                : " ";
        const std::string grown = t + sep + next.substr(0, cut);
        maximal = maximal && metrics(grown, runs[0]) > width;
      }
    }
    CHECK(fits);
    CHECK(maximal);
    CHECK(drawn);
    fprintf(stderr, "        scale %.1f: %zu lines at %.0f\n", scale,
            layout.lines.size(), width);
  }
}

TEST(ellipsis_cuts_where_measurement_says_at_three_scales) {
  chain({"narrow", "cjk"});
  for (float scale : kScales) {
    const auto metrics = metrics_at(scale);
    const float full = layout_styled_text(styled_runs(), metrics, 1e9f).width;
    const float width = full * 0.6f;
    const auto layout =
        layout_styled_text(styled_runs(), metrics, 1e9f, width);
    const auto &line = layout.lines[0];
    const std::string t = text_selection::line_text(line);
    CHECK(t.size() > 3 && t.substr(t.size() - 3) == "...");
    CHECK(layout.width <= width);
    // One more codepoint before the "..." would not fit.
    const std::string source = text_selection::line_text(
        layout_styled_text(styled_runs(), metrics, 1e9f).lines[0]);
    const size_t kept = t.size() - 3;
    const size_t more = kept + utf8_char_length(source, kept);
    uid::TextRunLine longer;
    size_t at = 0;
    for (const auto &run : styled_runs()) {
      if (at >= more)
        break;
      longer.push_back(uid::with_text(run, run.text.substr(0, more - at)));
      at += run.text.size();
    }
    longer.back().text += "...";
    CHECK(uid::line_width(longer, metrics) > width);
    // And the cut line draws exactly as wide as it measured.
    const auto quads = draw_layout(layout);
    CHECK(quads.size() == codepoints(t));
    const auto &last = layout.runs.back();
    CHECK(last.rect.x + drawn_advance(g_fm->get_font(last.font_name),
                                      last.text, last.font_size) ==
          layout.width);
  }
}

int main() {
  md::g_headless = true; // dpi 1, no sokol_app
  FONSparams params{};
  params.width = params.height = 4096;
  params.flags = FONS_ZERO_TOPLEFT;
  params.renderDraw = capture;
  md::g_fons_ctx = fonsCreateInternal(&params);
  if (!md::g_fons_ctx)
    return 2;

  F.ui = load_font_from_file(fixture("AtkinsonHyperlegible-Regular.ttf").c_str());
  F.narrow = load_font_from_file(fixture("ArchivoNarrow-Regular.ttf").c_str());
  const auto cjk = system_fonts::cjk();
  if (cjk.empty()) {
    fprintf(stderr, "no system CJK face; this test needs one\n");
    return 2;
  }
  fprintf(stderr, "CJK face: %s #%d\n", cjk[0].path.c_str(),
          cjk[0].face_index);
  F.cjk = load_font_face(cjk[0].path.c_str(), cjk[0].face_index);
  const auto symbols = system_fonts::symbols();
  F.symbols = symbols.empty()
                  ? Font{}
                  : load_font_face(symbols[0].path.c_str(),
                                   symbols[0].face_index);

  FontManager fm;
  fm.load_font("ui", F.ui);
  fm.load_font("narrow", F.narrow);
  fm.load_font("cjk", F.cjk);
  if (F.symbols.id != FONS_INVALID)
    fm.load_font("symbols", F.symbols);
  g_fm = &fm;
  for (const auto &[name, fn] : tests()) {
    const int before = failures;
    fn();
    std::fprintf(stderr, "%s %s\n", failures == before ? "PASS" : "FAIL", name);
  }
  std::fprintf(stderr, "text fallback: %d/%d checks passed\n",
               checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
