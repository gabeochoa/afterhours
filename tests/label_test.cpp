// label_test.cpp
// imm::label / label_muted must match the longhand div chain screens
// hand-roll: with_label + with_font(default) + Left + ignore-pointer +
// theme text colour.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
bool same_color(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
ComponentConfig sized(const char *name) {
  return ComponentConfig{}
      .with_size({pixels(200), pixels(40)})
      .with_debug_name(name);
}
} // namespace

TEST(label_matches_longhand_chain) {
  ImmTestHarness h;
  h.context().theme.font = Color{10, 20, 30, 255};
  h.context().theme.font_muted = Color{40, 50, 60, 255};
  h.begin_frame();
  auto got = label(h.context(), mk(h.root(), 0), "Hello", sized("got"));
  auto &defaults = UIStylingDefaults::get();
  auto want = div(h.context(), mk(h.root(), 1),
                  sized("want")
                      .with_label("Hello")
                      .with_font(defaults.resolved_font_name(),
                                 defaults.default_font_size)
                      .with_alignment(TextAlignment::Left)
                      .with_ignore_pointer_events()
                      .with_text_color(Theme::Usage::Font));
  h.layout_only();

  CHECK(got.ent().has<HasLabel>() && want.ent().has<HasLabel>());
  if (got.ent().has<HasLabel>() && want.ent().has<HasLabel>()) {
    const auto &g = got.ent().get<HasLabel>();
    const auto &w = want.ent().get<HasLabel>();
    CHECK(g.label == w.label);
    CHECK(g.alignment == w.alignment);
    CHECK(g.alignment == TextAlignment::Left);
    CHECK(g.text_overflow == w.text_overflow);
    CHECK(g.explicit_text_color.has_value() &&
          w.explicit_text_color.has_value());
    if (g.explicit_text_color && w.explicit_text_color)
      CHECK(same_color(*g.explicit_text_color, *w.explicit_text_color));
    if (g.explicit_text_color)
      CHECK(same_color(*g.explicit_text_color, h.context().theme.font));
  }
  CHECK(got.ent().get<UIComponent>().font_name ==
        want.ent().get<UIComponent>().font_name);
  CHECK(got.ent().get<UIComponent>().font_name ==
        defaults.resolved_font_name());
  CHECK_APPROX(got.ent().get<UIComponent>().font_size.value,
               want.ent().get<UIComponent>().font_size.value);
  CHECK(got.ent().has<IgnorePointerEvents>());
  CHECK(want.ent().has<IgnorePointerEvents>());
  // They stack in the root column, so only the measured size is comparable.
  CHECK_APPROX(got.cmp().rect().width, want.cmp().rect().width);
  CHECK_APPROX(got.cmp().rect().height, want.cmp().rect().height);

  const auto texts = [&] {
    h.render();
    return h.drawn("text");
  }();
  bool saw_hello = false;
  for (const auto &c : texts)
    if (c.text == "Hello" && same_color(c.color, h.context().theme.font))
      saw_hello = true;
  CHECK(saw_hello);
}

TEST(label_muted_uses_font_muted) {
  ImmTestHarness h;
  h.context().theme.font = Color{10, 20, 30, 255};
  h.context().theme.font_muted = Color{40, 50, 60, 255};
  h.begin_frame();
  auto muted = label_muted(h.context(), mk(h.root(), 0), "Quiet",
                           sized("muted"));
  auto plain = label(h.context(), mk(h.root(), 1), "Loud", sized("plain"));
  h.layout_only();
  CHECK(muted.ent().has<HasLabel>() && plain.ent().has<HasLabel>());
  if (muted.ent().has<HasLabel>() && plain.ent().has<HasLabel>()) {
    const auto &m = muted.ent().get<HasLabel>();
    const auto &p = plain.ent().get<HasLabel>();
    CHECK(m.explicit_text_color.has_value());
    CHECK(p.explicit_text_color.has_value());
    if (m.explicit_text_color)
      CHECK(same_color(*m.explicit_text_color, h.context().theme.font_muted));
    if (p.explicit_text_color)
      CHECK(same_color(*p.explicit_text_color, h.context().theme.font));
  }
}

TEST(label_explicit_overrides_win) {
  ImmTestHarness h;
  h.context().theme.font = Color{10, 20, 30, 255};
  h.begin_frame();
  const Color custom{1, 2, 3, 255};
  auto got = label(h.context(), mk(h.root(), 0), "Styled",
                   sized("styled")
                       .with_font("CustomFace", pixels(28))
                       .with_custom_text_color(custom)
                       .with_alignment(TextAlignment::Right));
  h.layout_only();
  CHECK(got.ent().get<UIComponent>().font_name == "CustomFace");
  CHECK_APPROX(got.ent().get<UIComponent>().font_size.value, 28.f);
  CHECK(got.ent().has<HasLabel>());
  if (got.ent().has<HasLabel>()) {
    const auto &g = got.ent().get<HasLabel>();
    CHECK(g.alignment == TextAlignment::Right);
    CHECK(g.explicit_text_color.has_value());
    if (g.explicit_text_color)
      CHECK(same_color(*g.explicit_text_color, custom));
  }
  // A size-only override keeps that size and still gets the default face.
  auto size_only = label(h.context(), mk(h.root(), 1), "Big",
                         sized("big").with_font_size(pixels(32)));
  CHECK(size_only.ent().get<UIComponent>().font_name ==
        UIStylingDefaults::get().resolved_font_name());
  CHECK_APPROX(size_only.ent().get<UIComponent>().font_size.value, 32.f);
}

int main() { return ui_test::run_registered_tests("label"); }
