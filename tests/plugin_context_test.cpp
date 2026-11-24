#include "ui_test_harness.h"

#include <afterhours/src/plugins/files.h>
#include <afterhours/src/plugins/input_system.h>
#include <afterhours/src/plugins/modal.h>
#include <afterhours/src/plugins/settings.h>
#include <afterhours/src/plugins/timer.h>
#include <afterhours/src/plugins/toast.h>
#include <afterhours/src/plugins/window_manager.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
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

enum struct PluginTestAction { Jump, Fire };
enum struct PluginTestLayer { Game, Menu };

struct PluginSettingsData {
  int volume = 7;
  std::string name = "default";

  json to_json() const { return json{{"volume", volume}, {"name", name}}; }
  static PluginSettingsData from_json(const json &j) {
    PluginSettingsData data;
    data.volume = j.value("volume", 7);
    data.name = j.value("name", std::string("default"));
    return data;
  }
  std::string to_string() const { return std::to_string(volume) + ":" + name; }
  static PluginSettingsData from_string(const std::string &text) {
    PluginSettingsData data;
    const auto colon = text.find(':');
    if (colon != std::string::npos) {
      data.volume = std::stoi(text.substr(0, colon));
      data.name = text.substr(colon + 1);
    }
    return data;
  }
};

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

TEST(window_manager_plugin_installs_and_collects) {
  static_assert(InstallablePlugin<WindowManagerPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(WindowManagerPlugin{.target_fps = 120,
                              .resolution = window_manager::Resolution{1024, 768},
                              .available_resolutions = {{1024, 768}, {800, 600}}})
      .use(WindowManagerPlugin{.target_fps = 30});

  CHECK(coll.has_singleton<window_manager::ProvidesTargetFPS>());
  CHECK(coll.get_singleton_cmp_enforce<window_manager::ProvidesTargetFPS>().fps == 120);
  auto &current =
      coll.get_singleton_cmp_enforce<window_manager::ProvidesCurrentResolution>();
  CHECK(current.current_resolution == window_manager::Resolution({1024, 768}));
  CHECK(!current.should_refetch);
  auto &available = coll.get_singleton_cmp_enforce<
      window_manager::ProvidesAvailableWindowResolutions>();
  CHECK(available.available_resolutions.size() == 2);
  CHECK(!available.should_refetch);

  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(current.current_resolution == window_manager::Resolution({1024, 768}));
  CHECK(available.available_resolutions.size() == 2);

  EntityCollection coll2;
  EntityHelper::ScopedCollection scope2(coll2);
  SystemManager sm2;
  PluginContext(sm2, coll2).use(WindowManagerPlugin{});
  CHECK(coll2.get_singleton_cmp_enforce<window_manager::ProvidesTargetFPS>().fps == 60);
  auto &entities2 = coll2.get_entities_for_mod();
  sm2.tick(entities2, 0.016f);
  CHECK(coll2.get_singleton_cmp_enforce<window_manager::ProvidesCurrentResolution>()
            .current_resolution == window_manager::headless_resolution);
  CHECK(!coll2.get_singleton_cmp_enforce<
             window_manager::ProvidesAvailableWindowResolutions>()
             .available_resolutions.empty());
}

TEST(input_plugin_installs_and_collects) {
  static_assert(InstallablePlugin<InputPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(InputPlugin::from_enum<PluginTestAction>(
      {{PluginTestAction::Jump, {input::KeyChord{keys::SPACE}}}}));

  CHECK(coll.has_singleton<input::InputCollector>());
  CHECK(coll.has_singleton<input::ProvidesMaxGamepadID>());
  CHECK(coll.has_singleton<input::ProvidesInputMapping>());
  CHECK(coll.has_singleton<input::ProvidesInputConfig>());
  const auto &mapping =
      coll.get_singleton_cmp_enforce<input::ProvidesInputMapping>().mapping;
  CHECK(mapping.size() == 1);
  CHECK(mapping.at(static_cast<int>(PluginTestAction::Jump)).size() == 1);

  auto &collector = coll.get_singleton_cmp_enforce<input::InputCollector>();
  collector.since_last_input = 0.f;
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.5f);
  CHECK_APPROX(collector.since_last_input, 0.5f);
}

TEST(layered_input_plugin_installs_and_collects) {
  static_assert(InstallablePlugin<LayeredInputPlugin<PluginTestLayer>>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(LayeredInputPlugin<PluginTestLayer>{
      .mapping = {{PluginTestLayer::Menu,
                   {{7, {input::KeyChord{keys::ENTER}}}}}},
      .starting_layer = PluginTestLayer::Menu});

  CHECK(coll.has_singleton<input::InputCollector>());
  CHECK(coll.has_singleton<input::ProvidesMaxGamepadID>());
  CHECK(coll.has_singleton<ProvidesLayeredInputMapping<PluginTestLayer>>());
  auto &layered = coll.get_singleton_cmp_enforce<
      ProvidesLayeredInputMapping<PluginTestLayer>>();
  CHECK(layered.active_layer == PluginTestLayer::Menu);
  CHECK(layered.get_bindings(7).size() == 1);

  auto &collector = coll.get_singleton_cmp_enforce<input::InputCollector>();
  collector.since_last_input = 0.f;
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.25f);
  CHECK_APPROX(collector.since_last_input, 0.25f);
}

TEST(modal_plugin_installs_and_prunes_stale_stack) {
  static_assert(InstallablePlugin<ModalPlugin<ui_test::TestInputAction>>);
  EntityHelper::delete_all_entities(true);
  ui_test::ImmTestHarness h;
  EntityHelper::registerSingleton<ui::UIContext<ui_test::TestInputAction>>(
      h.context_entity());

  SystemManager sm;
  PluginContext ctx(sm);
  ctx.use(ModalPlugin<ui_test::TestInputAction>{});

  CHECK(EntityHelper::has_singleton<modal::ModalRoot>());
  auto *root = EntityHelper::get_singleton_cmp<modal::ModalRoot>();
  CHECK(root != nullptr);
  if (root) {
    CHECK(root->modal_stack.empty());
    root->modal_stack.push_back(999999);
    auto &entities = EntityHelper::get_entities_for_mod();
    sm.tick(entities, 0.016f);
    CHECK(root->modal_stack.empty());
    sm.render(entities, 0.016f);
    CHECK(root->modal_stack.empty());
  }

  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<ui::UIContext<ui_test::TestInputAction>>());
  EntityHelper::delete_all_entities(true);
}

TEST(files_plugin_installs_paths) {
  static_assert(InstallablePlugin<FilesPlugin>);
  files::TestRoots roots;
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(FilesPlugin{.game_name = "plugin_test_game"});

  CHECK(coll.has_singleton<files::ProvidesResourcePaths>());
  auto &provider =
      coll.get_singleton_cmp_enforce<files::ProvidesResourcePaths>();
  CHECK(provider.game_name == "plugin_test_game");
  CHECK(provider.root_folder == "resources");
  CHECK(provider.get_save_path() == roots.save);
  CHECK(provider.get_config_path() == roots.config);
  CHECK(provider.get_resource_path("audio", "hit.wav").filename() == "hit.wav");

  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(provider.game_name == "plugin_test_game");
}

TEST(settings_plugin_installs_and_loads) {
  static_assert(InstallablePlugin<SettingsPlugin<PluginSettingsData>>);
  EntityHelper::delete_all_entities(true);
  namespace fs = std::filesystem;
  const auto dir = fs::temp_directory_path() / "afterhours-plugin-settings-test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  settings::set_path_override(dir);
  {
    std::ofstream out(dir / "plugin_settings.json");
    out << PluginSettingsData{.volume = 42, .name = "loaded"}.to_string();
  }

  SystemManager sm;
  PluginContext ctx(sm);
  ctx.use(SettingsPlugin<PluginSettingsData>{
      .data = {.volume = 1, .name = "initial"}, .file = "plugin_settings.json"});

  CHECK(EntityHelper::has_singleton<settings::ProvidesSettings<PluginSettingsData>>());
  CHECK(settings::get_data<PluginSettingsData>().volume == 1);
  auto &entities = EntityHelper::get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(settings::get_data<PluginSettingsData>().volume == 42);
  CHECK(settings::get_data<PluginSettingsData>().name == "loaded");

  settings::clear_path_override();
  fs::remove_all(dir);
  EntityHelper::delete_all_entities(true);
}

int main() { return ui_test::run_registered_tests("plugin context"); }
