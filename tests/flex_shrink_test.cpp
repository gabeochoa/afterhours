#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

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
      CHECK(x->rect().width > 0.f);
    if (cw <= 150.f)
      CHECK_APPROX(x->rect().width, 0.f);
  }
}

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
