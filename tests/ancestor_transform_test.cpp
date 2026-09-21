// ancestor_transform_test.cpp
// A child must be hit where a scaled or translated parent draws it.

#include "ui_test_harness.h"

using namespace ui_test;
using namespace afterhours::ui::imm;

static Entity &entity_named(ImmTestHarness &h, const std::string &name) {
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<UIComponentDebug>() && e->get<UIComponentDebug>().name() == name)
      return *e;
  fprintf(stderr, "no entity named %s\n", name.c_str());
  std::abort();
}

TEST(child_follows_scaled_and_translated_parent) {
  ImmTestHarness h;
  auto parent = imm::div(h.context(), imm::mk(h.root(), 0),
                         ComponentConfig{}.with_size({pixels(200), pixels(100)}).with_absolute_position(100, 100).with_debug_name("parent"));
  imm::div(h.context(), imm::mk(parent.ent(), 0),
           ComponentConfig{}.with_size({pixels(50), pixels(20)}).with_absolute_position(10, 10).with_debug_name("child"));
  h.layout_only();

  Entity &p = entity_named(h, "parent");
  Entity &c = entity_named(h, "child");
  const RectangleType flat = afterhours::ui::detail::hit_rect(c, c.get<UIComponent>());
  CHECK_APPROX(flat.x, 110.f);
  CHECK_APPROX(flat.y, 110.f);

  auto &mods = p.addComponent<HasUIModifiers>();
  mods.scale = 2.f;
  RectangleType hit = afterhours::ui::detail::hit_rect(c, c.get<UIComponent>());
  CHECK_APPROX(hit.x, 20.f);
  CHECK_APPROX(hit.y, 70.f);
  CHECK_APPROX(hit.width, 100.f);
  CHECK_APPROX(hit.height, 40.f);

  mods.translate_x = 5.f;
  hit = afterhours::ui::detail::hit_rect(c, c.get<UIComponent>());
  CHECK_APPROX(hit.x, 25.f);

  mods.scale = 1.f;
  mods.origin_x = 0.f;
  mods.origin_y = 0.f;
  mods.scale = 0.5f;
  mods.translate_x = 0.f;
  hit = afterhours::ui::detail::hit_rect(c, c.get<UIComponent>());
  CHECK_APPROX(hit.x, 105.f);
  CHECK_APPROX(hit.y, 105.f);
  CHECK_APPROX(hit.width, 25.f);
}

int main() { return run_registered_tests("ancestor transform"); }
