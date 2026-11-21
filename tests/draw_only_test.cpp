// Draw-only elements: counted cost before/after, identical draw output.
#include "ui_test_harness.h"

using namespace ui_test;
using namespace afterhours::ui::imm;

static int component_count(Entity &e) {
  int n = 0;
  for (size_t i = 0; i < afterhours::max_num_components; i++)
    if (e.componentSet.test(i))
      n++;
  return n;
}

static std::vector<std::string> draw_signature(bool draw_only) {
  ImmTestHarness h;
  auto cfg = ComponentConfig{}
                 .with_size({pixels(24), pixels(12)})
                 .with_custom_background(colors::red)
                 .with_label("hi");
  if (draw_only)
    cfg.with_draw_only();
  div(h.context(), mk(h.root(), 0), cfg);
  std::vector<std::string> sig;
  for (const auto &c : h.render())
    sig.push_back(fmt::format("{} {:.0f},{:.0f},{:.0f},{:.0f} {} {} {} {} {}",
                              c.op, c.rect.x, c.rect.y, c.rect.width,
                              c.rect.height, (int)c.color.r, (int)c.color.g,
                              (int)c.color.b, (int)c.color.a, c.text));
  return sig;
}

TEST(draw_only_costs_fewer_components_same_entities) {
  ImmTestHarness h;
  auto plain = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_size({pixels(10), pixels(10)}));
  auto deco = div(h.context(), mk(h.root(), 1),
                  ComponentConfig{}
                      .with_size({pixels(10), pixels(10)})
                      .with_draw_only());
  const int plain_n = component_count(plain.ent());
  const int deco_n = component_count(deco.ent());
  printf("  plain div: %d components; draw-only div: %d components\n", plain_n,
         deco_n);
  CHECK(plain.ent().id != deco.ent().id); // still 1 entity each
  CHECK(deco.ent().hasTag(UITag::DrawOnly));
  CHECK(!deco.ent().has<UIComponentDebug>());
  CHECK(!deco.ent().has<HasOpacity>());
  CHECK(deco.ent().get<UIComponent>().skip_when_tabbing);
  CHECK(deco_n == plain_n - 2); // UIComponentDebug + HasOpacity, tag is free
}

TEST(draw_only_label_and_opacity_kept_when_needed) {
  ImmTestHarness h;
  auto deco = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size({pixels(10), pixels(10)})
                      .with_label("hi")
                      .with_draw_only());
  CHECK(deco.ent().has<HasLabel>());
  auto faded = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}
                       .with_size({pixels(10), pixels(10)})
                       .with_opacity(0.5f)
                       .with_draw_only());
  CHECK(faded.ent().has<HasOpacity>());
  CHECK_APPROX(faded.ent().get<HasOpacity>().value, 0.5f);
}

TEST(draw_only_draw_output_identical) {
  CHECK(draw_signature(false) == draw_signature(true));
  CHECK(!draw_signature(true).empty());
}

int main() { return run_registered_tests("draw only"); }
