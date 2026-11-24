#include <afterhours/src/plugin.h>

#include "ui_test_harness.h"

#ifdef AFTER_HOURS_USE_RAYLIB
namespace raylib {
inline bool operator==(const Vector2 &a, const Vector2 &b) {
  return a.x == b.x && a.y == b.y;
}
inline bool operator<(const Vector2 &a, const Vector2 &b) {
  return a.x != b.x ? a.x < b.x : a.y < b.y;
}
} // namespace raylib
#endif

#include <afterhours/src/plugins/animation/plugin.h>
#include <afterhours/src/plugins/app_states.h>
#include <afterhours/src/plugins/camera.h>
#include <afterhours/src/plugins/collision.h>
#include <afterhours/src/plugins/files.h>
#include <afterhours/src/plugins/input_system.h>
#include <afterhours/src/plugins/modal.h>
#include <afterhours/src/plugins/pathfinding.h>
#include <afterhours/src/plugins/random.h>
#include <afterhours/src/plugins/settings.h>
#include <afterhours/src/plugins/sound_system.h>
#include <afterhours/src/plugins/telemetry.h>
#include <afterhours/src/plugins/timer.h>
#include <afterhours/src/plugins/toast.h>
#include <afterhours/src/plugins/translation.h>
#include <afterhours/src/plugins/trigger_volume.h>
#include <afterhours/src/plugins/window_manager.h>

#include <algorithm>
#include <cmath>
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

enum struct PluginState { Menu, Game, Pause };
enum struct PluginAction { Start, TogglePause };
enum struct PluginStringKey { Hello };
enum struct PluginParam { Count };
enum struct PluginFontID { Default };
struct PluginFontNameGetter {
  std::string operator()(PluginFontID) const { return "default"; }
};

struct PluginPos : BaseComponent {
  float x = 0.f;
  float y = 0.f;
  PluginPos() = default;
  PluginPos(float x_, float y_) : x(x_), y(y_) {}
};

struct CollisionTransform : BaseComponent {
  vec2 position{0.f, 0.f};
  vec2 size{1.f, 1.f};
  vec2 velocity{0.f, 0.f};
  collision::CollisionConfig collision_config;
};

struct CountingTelemetryProvider : TelemetryProvider {
  int *sent = nullptr;
  bool send_get(const std::string &, const TelemetryFields &) override {
    if (sent) ++*sent;
    return true;
  }
};

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

TEST(plugin_h_provides_installable_plugin_types) {
  static_assert(InstallablePlugin<WeatherPlugin>);
  static_assert(developer::PluginCore<timer>);
  static_assert(developer::plugin_ok<timer>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(WeatherPlugin{.wind = 3.f});
  CHECK(coll.has_singleton<WeatherState>());
  CHECK_APPROX(coll.get_singleton_cmp_enforce<WeatherState>().wind, 3.f);
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

TEST(sound_plugin_installs_and_ticks) {
  static_assert(InstallablePlugin<SoundPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(SoundPlugin{.default_alias_copies = 7})
      .use(SoundPlugin{.default_alias_copies = 2});

  CHECK(coll.has_singleton<sound_system::SoundEmitter>());
  CHECK(coll.get_singleton_cmp_enforce<sound_system::SoundEmitter>()
            .default_alias_copies == 7);
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(coll.get_singleton_cmp_enforce<sound_system::SoundEmitter>()
            .default_alias_copies == 7);
}

TEST(camera_plugin_installs_config) {
  static_assert(InstallablePlugin<CameraPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(CameraPlugin{.target = {10.f, 20.f},
                       .offset = {5.f, 6.f},
                       .rotation = 15.f,
                       .zoom = 2.f})
      .use(CameraPlugin{.zoom = 9.f});

  CHECK(coll.has_singleton<camera::HasCamera>());
  auto &cam = coll.get_singleton_cmp_enforce<camera::HasCamera>();
#ifdef AFTER_HOURS_USE_RAYLIB
  CHECK_APPROX(cam.camera.target.x, 10.f);
  CHECK_APPROX(cam.camera.target.y, 20.f);
  CHECK_APPROX(cam.camera.offset.x, 5.f);
  CHECK_APPROX(cam.camera.rotation, 15.f);
  CHECK_APPROX(cam.camera.zoom, 2.f);
#else
  (void)cam;
#endif
}

TEST(pathfinding_plugin_applies_response) {
  static_assert(InstallablePlugin<PathfindingPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(PathfindingPlugin{
      .algorithm = pathfinding::Algorithm::AStar,
      .walkability_checker = [](pathfinding::Vec2) { return true; }});

  CHECK(coll.has_singleton<pathfinding::ProvidesPathfinding>());
  auto &provider =
      coll.get_singleton_cmp_enforce<pathfinding::ProvidesPathfinding>();
  CHECK(provider.current_algorithm == pathfinding::Algorithm::AStar);
  CHECK(provider.walkability_checker(pathfinding::Vec2{0.f, 0.f}));

  Entity &actor = EntityHelper::createEntity();
  actor.addComponent<pathfinding::CanPathfind>();
  EntityHelper::merge_entity_arrays();
  provider.response_queue.push_back(pathfinding::PathResponse{
      .entity_id = actor.id,
      .path = {pathfinding::Vec2{1.f, 1.f}, pathfinding::Vec2{2.f, 2.f}},
      .on_complete = nullptr});
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(actor.get<pathfinding::CanPathfind>().path_size == 2);
  CHECK(!actor.get<pathfinding::CanPathfind>().has_active_request);
}

TEST(animation_plugin_advances_tracks) {
  static_assert(InstallablePlugin<AnimationPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(AnimationPlugin{});
  CHECK(!animation::is_instant());

  Entity &e = EntityHelper::createEntity();
  auto &tracks = e.addComponent<motion::HasTracks>();
  auto &track = tracks.track<float>(42);
  track.from(0.f).to(100.f, motion::Spring{});
  EntityHelper::merge_entity_arrays();
  auto &entities = coll.get_entities_for_mod();
  for (int i = 0; i < 120; ++i) sm.tick(entities, 1.f / 60.f);
  CHECK_APPROX(track.value(), 100.f);
}

TEST(translation_plugin_installs_and_looks_up) {
  using Plugin = translation::TranslationPlugin<PluginStringKey, PluginParam,
                                                PluginFontID, PluginFontNameGetter>;
  static_assert(InstallablePlugin<Plugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);

  Plugin::PluginType::LanguageMap translations;
  translations[translation::Language::English][PluginStringKey::Hello] =
      Plugin::PluginType::TranslatableStringType(std::string("Hello plugin"));
  ctx.use(Plugin{.translations = translations,
                 .default_language = translation::Language::English,
                 .param_name_map = {{PluginParam::Count, "count"}}});

  CHECK(coll.has_singleton<Plugin::PluginType::ProvidesTranslation>());
  CHECK(Plugin::PluginType::get_string(PluginStringKey::Hello) ==
        "Hello plugin");
  CHECK(coll.get_singleton_cmp_enforce<Plugin::PluginType::ProvidesTranslation>()
            .current_language == translation::Language::English);
}

TEST(app_states_plugin_transitions_on_input) {
  static_assert(InstallablePlugin<AppStatesPlugin<PluginState, PluginAction>>);
  static_assert(InstallablePlugin<AppStatesPlugin<PluginState>>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  PluginState entered = PluginState::Menu;
  ctx.use(AppStatesPlugin<PluginState, PluginAction>{
      .initial = PluginState::Menu,
      .transitions = {{PluginState::Menu, PluginAction::Start, PluginState::Game},
                      {PluginState::Game, PluginAction::TogglePause,
                       PluginState::Pause}},
      .on_enter = [&entered](PluginState next) { entered = next; }});
  ctx.ensure_singleton<input::InputCollector>();

  CHECK(coll.has_singleton<app_states::HasState<PluginState>>());
  auto &has_state =
      coll.get_singleton_cmp_enforce<app_states::HasState<PluginState>>();
  CHECK(has_state.state.is(PluginState::Menu));

  auto &collector = coll.get_singleton_cmp_enforce<input::InputCollector>();
  collector.inputs_pressed.clear();
  collector.inputs_pressed.push_back(input::ActionDone(
      input::DeviceMedium::Keyboard, 0,
      static_cast<int>(PluginAction::Start), 1.f, 0.f));
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(has_state.state.is(PluginState::Game));
  CHECK(entered == PluginState::Game);
}

TEST(trigger_volume_plugin_counts_entrant) {
  static_assert(InstallablePlugin<TriggerVolumePlugin<PluginPos>>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(TriggerVolumePlugin<PluginPos>{
      .make_candidate = [](const Entity &e, const PluginPos &pos) {
        return trigger_volume::Candidate{e.id, pos.x, pos.y};
      }});

  Entity &vol = EntityHelper::createEntity();
  vol.addComponent<trigger_volume::HasTriggerVolume>(
      trigger_volume::AABB{0.f, 0.f, 10.f, 10.f}, 1);
  Entity &actor = EntityHelper::createEntity();
  actor.addComponent<PluginPos>(5.f, 5.f);
  EntityHelper::merge_entity_arrays();
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(vol.get<trigger_volume::HasTriggerVolume>().volume.count() == 1);
}

TEST(telemetry_plugin_installs_and_flushes) {
  static_assert(InstallablePlugin<TelemetryPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  int sent = 0;
  TelemetryPlugin plugin;
  plugin.config.enabled = true;
  plugin.config.endpoint = "http://example.invalid/telemetry";
  plugin.config.game_version = "plugin-test";
  plugin.provider_factory = [&sent] {
    auto provider = std::make_unique<CountingTelemetryProvider>();
    provider->sent = &sent;
    return provider;
  };
  ctx.use(plugin);

  CHECK(coll.has_singleton<telemetry::ProvidesTelemetry>());
  auto &state = coll.get_singleton_cmp_enforce<telemetry::ProvidesTelemetry>();
  CHECK(state.config.game_version == "plugin-test");
  CHECK(!state.session_id.empty());
  CHECK(!state.config.platform.empty());
  telemetry::track_custom("plugin_event", {{"kind", "test"}});
  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(sent == 1);
  telemetry::shutdown();
}

TEST(random_plugin_seeds_engine_and_singleton) {
  static_assert(InstallablePlugin<RandomPlugin>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  ctx.use(RandomPlugin{.seed = "plugin-seed"});

  CHECK(coll.has_singleton<random_plugin::HasRandomSeed>());
  CHECK(coll.get_singleton_cmp_enforce<random_plugin::HasRandomSeed>().seed ==
        "plugin-seed");
  RNGStream expected(std::string("plugin-seed"));
  CHECK(RandomEngine::get().get_seed() == expected.get_seed());
  CHECK(RandomEngine::get().get_int(0, 1000000) ==
        expected.get_int(0, 1000000));
}

TEST(collision_plugin_resolves_overlap) {
  static_assert(InstallablePlugin<CollisionPlugin<CollisionTransform>>);
  EntityCollection coll;
  EntityHelper::ScopedCollection scope(coll);
  SystemManager sm;
  PluginContext ctx(sm, coll);
  CollisionPlugin<CollisionTransform> plugin;
  plugin.config.get_collision_scalar = [] { return 100.f; };
  plugin.config.get_max_speed = [] { return 50.f; };
  plugin.callbacks.normalize_vec = [](const vec2 &v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    return len < 0.0001f ? vec2{0.f, 0.f} : vec2{v.x / len, v.y / len};
  };
  plugin.callbacks.dot_product = [](const vec2 &a, const vec2 &b) {
    return a.x * b.x + a.y * b.y;
  };
  plugin.callbacks.vector_length = [](const vec2 &v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
  };
  plugin.callbacks.get_speed = [](const CollisionTransform &t) {
    return std::sqrt(t.velocity.x * t.velocity.x +
                     t.velocity.y * t.velocity.y);
  };
  plugin.callbacks.check_overlap = [](const CollisionTransform &a,
                                      const CollisionTransform &b) {
    return a.position.x < b.position.x + b.size.x &&
           a.position.x + a.size.x > b.position.x &&
           a.position.y < b.position.y + b.size.y &&
           a.position.y + a.size.y > b.position.y;
  };
  ctx.use(plugin);

  Entity &first = EntityHelper::createEntity();
  auto &first_transform = first.addComponent<CollisionTransform>();
  first_transform.position = {0.f, 0.f};
  first_transform.size = {10.f, 10.f};
  first_transform.velocity = {5.f, 0.f};
  first_transform.collision_config.mass = 1.f;
  first_transform.collision_config.restitution = 0.8f;
  Entity &second = EntityHelper::createEntity();
  auto &second_transform = second.addComponent<CollisionTransform>();
  second_transform.position = {8.f, 0.f};
  second_transform.size = {10.f, 10.f};
  second_transform.velocity = {-3.f, 0.f};
  second_transform.collision_config.mass = 2.f;
  second_transform.collision_config.restitution = 0.8f;
  EntityHelper::merge_entity_arrays();

  auto &entities = coll.get_entities_for_mod();
  sm.tick(entities, 0.016f);
  CHECK(first.get<CollisionTransform>().velocity.x != 5.f);
  CHECK(second.get<CollisionTransform>().velocity.x != -3.f);
}

int main() { return ui_test::run_registered_tests("plugin context"); }
