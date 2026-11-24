#include "ui_test_harness.h"

#include <afterhours/src/plugins/timer.h>
#include <afterhours/src/plugins/toast.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace afterhours;

namespace {

struct Shared : BaseComponent {
  int value = 0;
  explicit Shared(int v = 0) : value(v) {}
};

struct PluginA {
  void install(PluginContext &ctx) const { ctx.ensure_singleton<Shared>(42); }
};
struct PluginB {
  void install(PluginContext &ctx) const { ctx.ensure_singleton<Shared>(7); }
};

struct WeatherState : BaseComponent {
  float wind = 0.f;
  explicit WeatherState(float w = 0.f) : wind(w) {}
};

struct WeatherPlugin {
  float wind = 2.f;
  void install(PluginContext &ctx) const {
    ctx.ensure_singleton<WeatherState>(wind);
    const float w = wind;
    ctx.add_system(Stage::Update, [w](float) {
      if (EntityHelper::has_singleton<WeatherState>())
        EntityHelper::get_singleton_cmp_enforce<WeatherState>().wind = w;
    });
  }
};

int count_with_shared(EntityCollection &coll) {
  int n = 0;
  for (const auto &e : coll.get_entities())
    if (e && e->has<Shared>()) n++;
  return n;
}

} // namespace

TEST(stage_ordering_update_and_render) {
  std::vector<std::string> order;
  auto push = [&order](const char *name) {
    return [&order, name](float) { order.push_back(name); };
  };
  SystemManager sm;
  sm.register_system(Stage::Render, push("render"));
  sm.register_system(Stage::PostUpdate, push("post"));
  sm.register_system(Stage::OverlayRender, push("overlay"));
  sm.register_system(Stage::Update, push("update"));
  sm.register_system(Stage::PreRender, push("pre_render"));
  sm.register_system(Stage::PreUpdate, push("pre"));

  Entities entities;
  sm.tick(entities, 0.016f);
  CHECK((order == std::vector<std::string>{"pre", "update", "post"}));

  order.clear();
  sm.render(entities, 0.016f);
  auto first = [&order](const char *name) {
    return std::find(order.begin(), order.end(), name);
  };
  CHECK(first("pre_render") != order.end());
  CHECK(first("render") != order.end());
  CHECK(first("overlay") != order.end());
  CHECK(first("pre_render") < first("render"));
  CHECK(first("render") < first("overlay"));

  order.clear();
  bool fixed_ran = false;
  sm.register_system(Stage::FixedUpdate,
                     [&fixed_ran](float) { fixed_ran = true; });
  sm.tick(entities, 0.016f);
  CHECK(!fixed_ran);
  sm.fixed_tick(entities, 0.016f);
  CHECK(fixed_ran);
}

TEST(staged_systems_respect_groups) {
  std::vector<int> order;
  SystemManager sm;
  sm.register_system(Stage::PreUpdate,
                     [&order](float) { order.push_back(1); }, "off");
  sm.register_system(Stage::Update, [&order](float) { order.push_back(2); },
                     "off");
  sm.register_system(Stage::PostUpdate,
                     [&order](float) { order.push_back(3); }, "off");
  sm.register_system(Stage::FixedUpdate,
                     [&order](float) { order.push_back(4); }, "off");
  sm.register_system(Stage::PreRender,
                     [&order](float) { order.push_back(5); }, "off");
  sm.register_system(Stage::Render, [&order](float) { order.push_back(6); },
                     "off");
  sm.register_system(Stage::OverlayRender,
                     [&order](float) { order.push_back(7); }, "off");
  sm.register_system(Stage::Update, [&order](float) { order.push_back(8); });

  sm.set_group_enabled("off", false);
  Entities entities;
  sm.tick(entities, 0.016f);
  sm.fixed_tick(entities, 0.016f);
  sm.render(entities, 0.016f);
  CHECK((order == std::vector<int>{8}));
  CHECK(sm.systems_in_group("off").size() == 7);

  order.clear();
  sm.set_group_enabled("off", true);
  sm.tick(entities, 0.016f);
  CHECK((order == std::vector<int>{1, 2, 8, 3}));
}

TEST(ensure_singleton_is_idempotent_across_plugins) {
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(PluginA{}).use(PluginB{});
  static_assert(InstallablePlugin<PluginA>);

  CHECK(coll.has_singleton<Shared>());
  CHECK(coll.get_singleton_cmp_enforce<Shared>().value == 42);
  Shared *first = coll.get_singleton_cmp<Shared>();
  Shared &again = ctx.ensure_singleton<Shared>(99);
  CHECK(&again == first);
  CHECK(again.value == 42);
  CHECK(count_with_shared(coll) == 1);

  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(coll.get_singleton_cmp_enforce<Shared>().value == 42);
}

TEST(custom_config_plugin_needs_no_base_class) {
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(WeatherPlugin{.wind = 2.5f});
  static_assert(InstallablePlugin<WeatherPlugin>);

  CHECK(coll.has_singleton<WeatherState>());
  CHECK_APPROX(coll.get_singleton_cmp_enforce<WeatherState>().wind, 2.5f);
  coll.get_singleton_cmp_enforce<WeatherState>().wind = 0.f;
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK_APPROX(coll.get_singleton_cmp_enforce<WeatherState>().wind, 2.5f);
}

TEST(timer_plugin_end_to_end) {
  EntityHelper::delete_all_entities(true);
  SystemManager sm;
  Entity &e = EntityHelper::createEntity();
  e.addComponent<timer::HasTimer>(1.f);
  EntityHelper::merge_entity_arrays();

  PluginContext(sm).use(TimerPlugin{});
  auto &entities = EntityHelper::get_entities_for_mod();
  sm.tick(entities, 0.25f);
  CHECK_APPROX(e.get<timer::HasTimer>().current_time, 0.25f);
  sm.tick(entities, 0.25f);
  CHECK_APPROX(e.get<timer::HasTimer>().current_time, 0.5f);
  EntityHelper::delete_all_entities(true);
}

TEST(toast_plugin_installs_in_one_use) {
  EntityHelper::delete_all_entities(true);
  ui_test::ImmTestHarness h;
  EntityHelper::registerSingleton<ui::UIContext<ui_test::TestInputAction>>(
      h.context_entity());

  SystemManager sm;
  PluginContext ctx(sm);
  ctx.use(ToastPlugin<ui_test::TestInputAction>{});

  CHECK(EntityHelper::has_singleton<toast::ToastRoot>());
  auto *root = EntityHelper::get_singleton_cmp<toast::ToastRoot>();
  CHECK(root != nullptr);
  if (root) {
    CHECK(root->entity_id >= 0);
    CHECK(toast::get_root().id == root->entity_id);
  }

  auto notice = toast::send_info(h.context(), "Saved", 30.f);
  EntityHelper::merge_entity_arrays();
  auto &entities = EntityHelper::get_entities_for_mod();
  sm.tick(entities, 0.5f);
  CHECK_APPROX(notice.ent().get<toast::Toast>().elapsed, 0.5f);

  bool render_saw_layout = false;
  ctx.add_system(Stage::Render, [&](float) {
    render_saw_layout =
        notice.ent().get<ui::UIComponent>().computed_rel[ui::Axis::Y] > 0.f;
  });
  sm.render(entities, 0.016f);
  CHECK(render_saw_layout);

  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<ui::UIContext<ui_test::TestInputAction>>());
  EntityHelper::delete_all_entities(true);
}

int main() { return ui_test::run_registered_tests("plugin context"); }
