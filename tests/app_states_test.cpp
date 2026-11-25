#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/app_states.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  ++checks;
  if (!cond) {
    ++failures;
    std::fprintf(stderr, "  FAIL: %s (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

enum class App { Menu, Game, Pause };

struct GatedSystem : System<> {
  const app_states::State<App> *state;
  App wanted;
  int *runs;
  GatedSystem(const app_states::State<App> &s, App w, int &r)
      : state(&s), wanted(w), runs(&r) {}
  bool should_run(const float dt) const override {
    return app_states::run_if(app_states::in_state(*state, wanted), dt);
  }
  void once(const float) override { ++*runs; }
};

struct UngatedSystem : System<> {
  int *runs;
  explicit UngatedSystem(int &r) : runs(&r) {}
  void once(const float) override { ++*runs; }
};

int main() {
  {
    app_states::State<App> state{App::Menu};
    CHECK(state.current() == App::Menu);
    CHECK(state.is(App::Menu));
    CHECK(!state.is(App::Game));

    std::vector<std::string> log;
    state.on_exit([&](App s) {
      log.push_back(s == App::Menu && state.is(App::Menu) ? "exit1:menu@menu"
                                                          : "exit1:bad");
    });
    state.on_exit([&](App) { log.push_back("exit2"); });
    state.on_enter([&](App s) {
      log.push_back(s == App::Game && state.is(App::Game) ? "enter1:game@game"
                                                          : "enter1:bad");
    });
    state.on_enter([&](App) { log.push_back("enter2"); });

    CHECK(state.set(App::Game));
    CHECK((log == std::vector<std::string>{"exit1:menu@menu", "exit2",
                                           "enter1:game@game", "enter2"}));

    CHECK(!state.set(App::Game));
    CHECK(log.size() == 4);
    CHECK(state.set(App::Pause));
    CHECK(log.size() == 8);
  }

  {
    app_states::State<App> state{App::Menu};
    CHECK(app_states::run_if(app_states::in_state(state, App::Menu)));
    CHECK(app_states::run_if(app_states::in_state(state, App::Menu), 0.016f));
    CHECK(!app_states::run_if(app_states::in_state(state, App::Game)));
    CHECK(!app_states::run_if(app_states::in_state(state, App::Game), 0.016f));
    CHECK(app_states::run_if([] { return true; }));
    CHECK(!app_states::run_if([](float dt) { return dt > 1.f; }, 0.016f));
    state.set(App::Game);
    CHECK(!app_states::run_if(app_states::in_state(state, App::Menu)));
    CHECK(app_states::run_if(app_states::in_state(state, App::Game)));
  }

  {
    app_states::State<App> state{App::Menu};
    int menu_runs = 0, game_runs = 0, always_runs = 0;
    SystemManager sm;
    sm.register_update_system(
        std::make_unique<GatedSystem>(state, App::Menu, menu_runs));
    sm.register_update_system(
        std::make_unique<GatedSystem>(state, App::Game, game_runs));
    sm.register_update_system(std::make_unique<UngatedSystem>(always_runs));

    sm.run(1.f / 60.f);
    sm.run(1.f / 60.f);
    CHECK(menu_runs == 2);
    CHECK(game_runs == 0);
    CHECK(always_runs == 2);

    state.set(App::Game);
    sm.run(1.f / 60.f);
    CHECK(menu_runs == 2);
    CHECK(game_runs == 1);
    CHECK(always_runs == 3);

    state.set(App::Pause);
    sm.run(1.f / 60.f);
    CHECK(menu_runs == 2);
    CHECK(game_runs == 1);
    CHECK(always_runs == 4);
  }

  {
    enum class Act { Start, TogglePause };

    Entity &state_entity = EntityHelper::createPermanentEntity();
    app_states::add_singleton_components<App>(state_entity, App::Menu);
    auto *has_state = EntityHelper::get_singleton_cmp<app_states::HasState<App>>();
    CHECK(has_state != nullptr);
    CHECK(has_state->state.is(App::Menu));

    Entity &input_entity = EntityHelper::createPermanentEntity();
    auto &collector = input_entity.addComponent<input::InputCollector>();
    EntityHelper::registerSingleton<input::InputCollector>(input_entity);

    SystemManager plugin_sm;
    app_states::register_update_systems<App, Act>(
        plugin_sm, {{App::Menu, Act::Start, App::Game},
                    {App::Game, Act::TogglePause, App::Pause},
                    {App::Pause, Act::TogglePause, App::Game}});

    auto press = [&](Act action) {
      collector.inputs_pressed.clear();
      collector.inputs_pressed.push_back(input::ActionDone(
          input::DeviceMedium::Keyboard, 0, static_cast<int>(action), 1.f, 0.f));
      plugin_sm.run(1.f / 60.f);
    };

    press(Act::TogglePause);
    CHECK(has_state->state.is(App::Menu));
    press(Act::Start);
    CHECK(has_state->state.is(App::Game));
    press(Act::Start);
    CHECK(has_state->state.is(App::Game));
    press(Act::TogglePause);
    CHECK(has_state->state.is(App::Pause));
    press(Act::TogglePause);
    CHECK(has_state->state.is(App::Game));
  }

  std::printf("app_states_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures ? 1 : 0;
}
