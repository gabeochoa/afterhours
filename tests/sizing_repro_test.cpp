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

int main() { return ui_test::run_registered_tests("sizing repro"); }
