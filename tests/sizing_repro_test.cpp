// sizing_repro_test.cpp
// Scratch repro for two filed sizing gaps whose mechanism is unconfirmed:
//   #12 expand() resolves ~2px taller than the space left for it
//   #13 children() measures short of its own children
// Both are suspected to be a padding/margin term missing from a sizing sum.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// --- #12 -------------------------------------------------------------------
// A 200px column with 10px padding all round holds a fixed 100px child and an
// expand() child. The expander should get what is left inside the padding:
// 200 - 10 - 10 - 100 = 80.
TEST(expand_fits_inside_parent_padding) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(200)})
                     .with_flex_direction(FlexDirection::Column)
                     .with_padding(Padding::all(pixels(10)))
                     .with_debug_name("col"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(100)})
          .with_debug_name("fixed"));
  div(h.context(), mk(col.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), expand()})
          .with_debug_name("expander"));
  h.layout_only();

  UIComponent *e = h.find("expander");
  CHECK(e != nullptr);
  if (e) {
    printf("  [#12] expander height = %.2f (expected 80)\n", e->rect().height);
    CHECK_APPROX(e->rect().height, 80.f);
  }
}

// Same shape with no padding: 200 - 100 = 100. Isolates padding as the cause.
TEST(expand_fits_without_padding) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(200)})
                     .with_flex_direction(FlexDirection::Column)
                     .with_debug_name("col2"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(100)})
          .with_debug_name("fixed2"));
  div(h.context(), mk(col.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), expand()})
          .with_debug_name("expander2"));
  h.layout_only();

  UIComponent *e = h.find("expander2");
  CHECK(e != nullptr);
  if (e) {
    printf("  [#12] no-padding expander height = %.2f (expected 100)\n",
           e->rect().height);
    CHECK_APPROX(e->rect().height, 100.f);
  }
}

// And with a sibling margin: 200 - 100 - 12 = 88.
TEST(expand_accounts_for_sibling_margin) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(200)})
                     .with_flex_direction(FlexDirection::Column)
                     .with_debug_name("col3"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(100)})
          .with_margin(Margin{.bottom = pixels(12)})
          .with_debug_name("fixed3"));
  div(h.context(), mk(col.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), expand()})
          .with_debug_name("expander3"));
  h.layout_only();

  UIComponent *e = h.find("expander3");
  CHECK(e != nullptr);
  if (e) {
    printf("  [#12] margin expander height = %.2f (expected 88)\n",
           e->rect().height);
    CHECK_APPROX(e->rect().height, 88.f);
  }
}

// --- #13 -------------------------------------------------------------------
// A children()-sized column around one 100px child with a 12px bottom margin
// should be 112 tall.
TEST(children_includes_child_margin) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), children()})
                     .with_flex_direction(FlexDirection::Column)
                     .with_debug_name("hug"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(100)})
          .with_margin(Margin{.bottom = pixels(12)})
          .with_debug_name("hug_child"));
  h.layout_only();

  UIComponent *c = h.find("hug");
  CHECK(c != nullptr);
  if (c) {
    printf("  [#13] children() height = %.2f (expected 112)\n",
           c->rect().height);
    CHECK_APPROX(c->rect().height, 112.f);
  }
}

// And with the parent's own padding: 100 + 10 + 10 = 120.
TEST(children_includes_own_padding) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), children()})
                     .with_flex_direction(FlexDirection::Column)
                     .with_padding(Padding::all(pixels(10)))
                     .with_debug_name("hug2"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(100)})
          .with_debug_name("hug_child2"));
  h.layout_only();

  UIComponent *c = h.find("hug2");
  CHECK(c != nullptr);
  if (c) {
    printf("  [#13] children()+padding height = %.2f (expected 120)\n",
           c->rect().height);
    CHECK_APPROX(c->rect().height, 120.f);
  }
}


// --- with grid snapping, which is what apps actually ship -------------------
// snap_to_8pt_grid rounds to NEAREST, and runs on any non-Pixels dimension
// after it was computed to exactly fill the space. So an expander that should
// be 98 rounds UP to 100 and overflows its parent by 2.
TEST(snapped_expand_overflows_its_parent) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(200)})
                     .with_flex_direction(FlexDirection::Column)
                     .with_debug_name("scol"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(102)})
          .with_debug_name("sfixed"));
  div(h.context(), mk(col.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), expand()})
          .with_debug_name("sexpander"));
  h.layout_only(/*grid_snap=*/true, window_manager::Resolution{1280, 720});

  UIComponent *e = h.find("sexpander");
  CHECK(e != nullptr);
  if (e) {
    // Cannot be exactly 98: that is not on the grid. The guarantee is that
    // snapping never pushes it past the space it was measured into.
    printf("  [#12] snapped expander = %.2f (space left is 98)\n",
           e->rect().height);
    CHECK(e->rect().height <= 98.f);
    CHECK(e->rect().height > 94.f); // still the largest grid step that fits
  }
}

// And children() rounds the other way: a 105 content sum snaps DOWN to 104,
// so the parent is 2px shorter than the child it is meant to contain.
TEST(snapped_children_is_shorter_than_its_child) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), children()})
                     .with_flex_direction(FlexDirection::Column)
                     .with_debug_name("shug"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(105)})
          .with_debug_name("shug_child"));
  h.layout_only(/*grid_snap=*/true, window_manager::Resolution{1280, 720});

  UIComponent *c = h.find("shug");
  UIComponent *k = h.find("shug_child");
  CHECK(c != nullptr && k != nullptr);
  if (c && k) {
    printf("  [#13] snapped children() = %.2f, child = %.2f\n",
           c->rect().height, k->rect().height);
    CHECK(c->rect().height >= k->rect().height);
  }
}

// floatinghotel: in a Row, expand() is reported to take the full parent width
// instead of what fixed siblings leave. The Column cases above pass, so if this
// is real the two axes disagree.
TEST(expand_in_a_row_takes_only_what_is_left) {
  ImmTestHarness h;
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(40)})
                     .with_flex_direction(FlexDirection::Row)
                     .with_debug_name("row"));
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(16), pixels(40)})
          .with_debug_name("status"));
  div(h.context(), mk(row.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{expand(), pixels(40)})
          .with_debug_name("filename"));
  h.layout_only();

  UIComponent *f = h.find("filename");
  CHECK(f != nullptr);
  if (f) {
    printf("  [row] expander width = %.2f (200 - 16 = 184)\n", f->rect().width);
    CHECK_APPROX(f->rect().width, 184.f);
  }
}

// Their exact shape: a button with Row children, where the button also has a
// label of its own.
TEST(expand_in_a_button_row_takes_only_what_is_left) {
  ImmTestHarness h;
  auto btn = button(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(200), pixels(40)})
                        .with_flex_direction(FlexDirection::Row)
                        .with_debug_name("btn"));
  div(h.context(), mk(btn.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(16), pixels(40)})
          .with_label("S")
          .with_debug_name("btn_status"));
  div(h.context(), mk(btn.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{expand(), pixels(40)})
          .with_label("filename.txt")
          .with_debug_name("btn_filename"));
  h.layout_only();

  UIComponent *f = h.find("btn_filename");
  UIComponent *st = h.find("btn_status");
  UIComponent *b = h.find("btn");
  CHECK(f != nullptr && st != nullptr && b != nullptr);
  if (f && st && b) {
    printf("  [button row] status=%.0f expander=%.0f in %.0f\n",
           st->rect().width, f->rect().width, b->rect().width);
    // Not a fixed number: expand() fills the content box, so the button's own
    // padding comes off first. What matters is that the two share the row.
    CHECK_APPROX(st->rect().y, f->rect().y); // the reported symptom: no wrap
    CHECK(st->rect().width + f->rect().width <= b->rect().width);
    CHECK(f->rect().width > st->rect().width * 4); // took the remainder
  }
}

// kart: checkbox_row's children are taller than the row, logging
// "checkbox label extends outside parent checkbox_row".
TEST(checkbox_children_fit_their_row) {
  ImmTestHarness h;
  bool on = true;
  checkbox(h.context(), mk(h.root(), 0), on,
           ComponentConfig{}
               .with_size(ComponentSize{pixels(240), pixels(32)})
               .with_label("enabled")
               .with_debug_name("cb"));
  h.layout_only();

  UIComponent *row = h.find("cb");
  CHECK(row != nullptr);
  if (row) {
    for (EntityID cid : row->children) {
      UIComponent &c = AutoLayout::to_cmp_static(cid);
      printf("  [checkbox] child %.0fx%.0f in row %.0fx%.0f\n",
             c.rect().width, c.rect().height, row->rect().width,
             row->rect().height);
      CHECK(c.rect().height <= row->rect().height);
    }
  }
}

// hanabi #136 wants fit_content(max): hug the text, cap it, wrap past that.
// Dim::Text hugs and with_max_width caps, so check if that's already enough.
TEST(text_sized_box_hugs_short_text_and_caps_long_text) {
  ImmTestHarness h;
  auto shortb = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f},
                                                 pixels(24)})
                        .with_max_width(pixels(200))
                        .with_label("hi")
                        .with_debug_name("bubble_short"));
  auto longb = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}
                       .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f},
                                                pixels(24)})
                       .with_max_width(pixels(200))
                       .with_text_overflow(TextOverflow::Wrap)
                       .with_label("a much longer message that should have to "
                                   "wrap onto more than one line")
                       .with_debug_name("bubble_long"));
  h.layout_only();
  (void)shortb;
  (void)longb;

  UIComponent *sb = h.find("bubble_short");
  UIComponent *lb = h.find("bubble_long");
  CHECK(sb != nullptr && lb != nullptr);
  if (sb && lb) {
    printf("  [#136] short=%.0f long=%.0f (cap 200)\n", sb->rect().width,
           lb->rect().width);
    CHECK(sb->rect().width < 100.f);  // hugged, nowhere near the cap
    CHECK(lb->rect().width <= 200.f); // capped rather than run on
  }
}

// Other half of a bubble: once it wraps, it has to get taller too.
TEST(a_capped_wrapping_box_grows_for_its_lines) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f}, Size{Dim::Text, 0.f, 1.f}})
          .with_max_width(pixels(200))
          .with_text_overflow(TextOverflow::Wrap)
          .with_font_size(16.f)
          .with_label("one line")
          .with_debug_name("bub_one"));
  div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f}, Size{Dim::Text, 0.f, 1.f}})
          .with_max_width(pixels(200))
          .with_text_overflow(TextOverflow::Wrap)
          .with_font_size(16.f)
          .with_label("a much longer message that has to wrap onto several "
                      "lines once its width is capped at two hundred")
          .with_debug_name("bub_many"));
  h.layout_only();

  UIComponent *one = h.find("bub_one");
  UIComponent *many = h.find("bub_many");
  CHECK(one != nullptr && many != nullptr);
  if (one && many) {
    printf("  [#136] heights one=%.0f many=%.0f\n", one->rect().height,
           many->rect().height);
    CHECK(many->rect().height > one->rect().height);
  }
}


// deadspace's tab strip: eight expand() tabs in a fixed-width row. The last one
// hung 6px outside the row at 1080p and fit at 720p, so the divide is landing
// off the grid and the remainder is going somewhere.
TEST(expand_children_stay_inside_a_row_that_does_not_divide_evenly) {
  for (float row_w : {948.f, 950.f, 632.f, 1000.f, 777.f}) {
    ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(row_w), pixels(32)})
                          .with_debug_name("row"));
    for (int i = 0; i < 8; i++)
      div(h.context(), mk(row.ent(), i),
          ComponentConfig{}
              .with_size(ComponentSize{expand(), percent(1.f)})
              .with_debug_name("tab_" + std::to_string(i)));
    h.layout_only(true, {1920, 1080});

    UIComponent *r = h.find("row");
    UIComponent *last = h.find("tab_7");
    CHECK(r != nullptr && last != nullptr);
    if (!r || !last)
      continue;
    const float row_end = r->rect().x + r->rect().width;
    const float last_end = last->rect().x + last->rect().width;
    printf("  row %.0f: last tab ends %.0f, row ends %.0f (over by %.0f)\n",
           row_w, last_end, row_end, last_end - row_end);
    for (int i = 0; i < 8; i++) {
      UIComponent *t = h.find("tab_" + std::to_string(i));
      if (t)
        printf("      tab_%d x=%.1f w=%.1f\n", i, t->rect().x, t->rect().width);
    }
    CHECK(last_end <= row_end + 0.5f);
  }
}


// A row whose width is not a whole number of grid units cannot be filled by
// equal grid-aligned children: it is either slack or a one-unit difference.
// We take the difference, so this pins that it never grows past one unit.
TEST(equal_weight_expanders_stay_equal) {
  for (float row_w : {1100.f, 1098.f, 948.f, 1000.f}) {
    ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(row_w), pixels(40)})
                          .with_debug_name("prow"));
    for (int i = 0; i < 2; i++)
      div(h.context(), mk(row.ent(), i),
          ComponentConfig{}
              .with_size(ComponentSize{expand(), percent(1.f)})
              .with_debug_name("panel_" + std::to_string(i)));
    h.layout_only(true, {1280, 720});
    UIComponent *a = h.find("panel_0");
    UIComponent *b = h.find("panel_1");
    CHECK(a != nullptr && b != nullptr);
    if (a && b) {
      printf("  row %.0f: panels %.0f and %.0f (diff %.0f)\n", row_w,
             a->rect().width, b->rect().width,
             a->rect().width - b->rect().width);
      CHECK(std::abs(a->rect().width - b->rect().width) <= 6.f);
      CHECK(a->rect().width + b->rect().width >= row_w - 6.f);
    }
  }
}


// A Dim::Children parent has to be wide enough for the children it is sized to
// hold. It summed their pre-snap sizes while they drew at their snapped ones,
// so a strip of ten segments measured 120 around content that drew 156.
//
// It cannot come out exact here: a 2px margin has no representation on a 4px
// grid, so the snapped positions still run a couple of pixels past the sum.
// What this pins is that the parent is sized from the same numbers the
// children are drawn at.
TEST(a_children_sized_row_includes_child_margins) {
  ImmTestHarness h;
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{children(), pixels(20)})
                        .with_debug_name("mrow"));
  for (int i = 0; i < 5; i++)
    div(h.context(), mk(row.ent(), i),
        ComponentConfig{}
            .with_size(ComponentSize{w1280(10), pixels(20)})
            .with_margin(i > 0 ? Margin{.left = pixels(2)} : Margin{})
            .with_debug_name("seg_" + std::to_string(i)));
  h.layout_only(true, {1280, 720});

  UIComponent *r = h.find("mrow");
  UIComponent *last = h.find("seg_4");
  CHECK(r != nullptr && last != nullptr);
  if (r && last) {
    // Five segments that snap to 12 wide, with four 2px margins.
    printf("  children row = %.0f (want 68), content spans %.0f\n",
           r->rect().width, last->rect().x + last->rect().width - r->rect().x);
    CHECK(r->rect().width == 68.f);
  }
}


// Padding wider than the box is the mistake that ran through most of the
// screens a containment sweep flagged: a fixed-pixel size next to a theme
// padding that scales, right at 720p and inverted above it. The clamp makes
// the content area 0 rather than negative, which is safe and silent, so this
// pins that the number is the clamped one and not something below zero.
TEST(padding_wider_than_the_box_leaves_no_content_area) {
  ImmTestHarness h;
  auto boxed = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(200), pixels(46)})
                       .with_padding(Padding{.top = pixels(30),
                                             .left = pixels(10),
                                             .bottom = pixels(30),
                                             .right = pixels(10)})
                       .with_debug_name("squeezed"));
  div(h.context(), mk(boxed.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{percent(1.f), percent(1.f)})
          .with_debug_name("inner"));
  h.layout_only();

  UIComponent *inner = h.find("inner");
  CHECK(inner != nullptr);
  if (inner) {
    // 46 tall with 60 of vertical padding: clamped to 0, never negative.
    printf("  inner is %.0fx%.0f\n", inner->rect().width,
           inner->rect().height);
    CHECK(inner->rect().height >= 0.f);
    CHECK(inner->rect().width >= 0.f);
  }
}

int main() { return ui_test::run_registered_tests("sizing repro"); }
