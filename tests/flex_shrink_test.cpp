// flex_shrink_test.cpp -- R-070: overflow shrink must not squeeze a fixed
// control below its own content. Flexible siblings absorb first; what they
// cannot absorb stays as overflow/clip instead of shrinking the fixed box.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// A w1280 control (strictness 0.9, so shrinkable) holding a 100px child, next
// to an expand() sibling and a strict pixels() sibling. As the row is squeezed
// the expander goes to 0 first, then the row overflows -- the fixed control
// keeps the 100px its child needs at every container width.
TEST(fixed_control_holds_its_child_size_when_row_is_squeezed) {
  for (float cw : {300.f, 200.f, 150.f, 105.f}) {
    ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(cw), pixels(40)})
                          .with_debug_name("row"));
    auto fixed = div(h.context(), mk(row.ent(), 0),
                     ComponentConfig{}
                         .with_size(ComponentSize{w1280(100), pixels(40)})
                         .with_debug_name("fixed"));
    div(h.context(), mk(fixed.ent(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(100), pixels(40)})
            .with_debug_name("inner"));
    div(h.context(), mk(row.ent(), 1),
        ComponentConfig{}
            .with_size(ComponentSize{expand(), pixels(40)})
            .with_debug_name("flex"));
    div(h.context(), mk(row.ent(), 2),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(60), pixels(40)})
            .with_debug_name("fixed2"));
    h.layout_only(false, {1280, 720});

    UIComponent *f = h.find("fixed");
    UIComponent *inner = h.find("inner");
    UIComponent *x = h.find("flex");
    UIComponent *f2 = h.find("fixed2");
    CHECK(f != nullptr && inner != nullptr && x != nullptr && f2 != nullptr);
    if (!f || !inner || !x || !f2)
      continue;
    printf("  cw=%.0f fixed=%.1f inner=%.1f flex=%.1f fixed2=%.1f\n", cw,
           f->rect().width, inner->rect().width, x->rect().width,
           f2->rect().width);
    CHECK_APPROX(f->rect().width, 100.f);
    CHECK_APPROX(f2->rect().width, 60.f);
    if (cw >= 200.f)
      CHECK(x->rect().width > 0.f); // flexible absorbs while it has room
    if (cw <= 150.f)
      CHECK_APPROX(x->rect().width, 0.f); // then overflow, not fixed shrink
  }
}

// Same protection on the cross axis of the floatinghotel symptom: a column of
// h720 rows. A shrinkable row with a 28px child keeps 28px as the column is
// squeezed; the expand() sibling absorbs.
TEST(fixed_row_holds_its_child_height_when_column_is_squeezed) {
  for (float ch : {200.f, 120.f, 80.f, 50.f}) {
    ImmTestHarness h;
    auto col = vstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(200), pixels(ch)})
                          .with_debug_name("col"));
    auto fixed = div(h.context(), mk(col.ent(), 0),
                     ComponentConfig{}
                         .with_size(ComponentSize{pixels(200), h720(28)})
                         .with_debug_name("fixed"));
    div(h.context(), mk(fixed.ent(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(200), pixels(28)})
            .with_debug_name("inner"));
    div(h.context(), mk(col.ent(), 1),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(200), expand()})
            .with_debug_name("flex"));
    div(h.context(), mk(col.ent(), 2),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(200), pixels(60)})
            .with_debug_name("fixed2"));
    h.layout_only(false, {1280, 720});

    UIComponent *f = h.find("fixed");
    UIComponent *x = h.find("flex");
    CHECK(f != nullptr && x != nullptr);
    if (!f || !x)
      continue;
    printf("  ch=%.0f fixed=%.1f flex=%.1f\n", ch, f->rect().height,
           x->rect().height);
    CHECK_APPROX(f->rect().height, 28.f);
  }
}

// All-fixed row: nothing is flexible, so nothing shrinks -- the row overflows
// (existing overflow/clip semantics) with every control at its fixed size.
TEST(all_fixed_row_overflows_instead_of_shrinking) {
  ImmTestHarness h;
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(150), pixels(40)})
                        .with_debug_name("row"));
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(40)})
          .with_debug_name("a"));
  div(h.context(), mk(row.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100), pixels(40)})
          .with_debug_name("b"));
  h.layout_only(false, {1280, 720});

  UIComponent *a = h.find("a");
  UIComponent *b = h.find("b");
  CHECK(a != nullptr && b != nullptr);
  if (a && b) {
    CHECK_APPROX(a->rect().width, 100.f);
    CHECK_APPROX(b->rect().width, 100.f);
  }
}

// A leaf with no content and an explicitly lowered strictness is still
// shrinkable: the floor only protects content, it does not make every
// definite size rigid.
TEST(leaf_with_lowered_strictness_still_shrinks_to_fit) {
  ImmTestHarness h;
  auto col = vstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(200), pixels(200)})
                        .with_debug_name("col"));
  div(h.context(), mk(col.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(150, 0.5f)})
          .with_debug_name("a"));
  div(h.context(), mk(col.ent(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(150, 0.5f)})
          .with_debug_name("b"));
  h.layout_only(false, {1280, 720});

  UIComponent *a = h.find("a");
  UIComponent *b = h.find("b");
  CHECK(a != nullptr && b != nullptr);
  if (a && b)
    CHECK(a->rect().height + b->rect().height <= 201.f);
}

int main() { return ui_test::run_registered_tests("flex shrink"); }
