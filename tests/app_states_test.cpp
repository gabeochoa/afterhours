#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/app_states.h>

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
  const State<App> *state;
  App wanted;
  int *runs;
  GatedSystem(const State<App> &s, App w, int &r)
      : state(&s), wanted(w), runs(&r) {}
  bool should_run(const float dt) const override {
    return run_if(in_state(*state, wanted), dt);
  }
  void once(const float) override { ++*runs; }
};

struct UngatedSystem : System<> {
  int *runs;
  explicit UngatedSystem(int &r) : runs(&r) {}
  void once(const float) override { ++*runs; }
};

int main() {
  // enter/exit order, and what current() reads inside each hook
  {
    State<App> state{App::Menu};
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

    // same-state set(): no refire
    CHECK(!state.set(App::Game));
    CHECK(log.size() == 4);
    CHECK(state.set(App::Pause));
    CHECK(log.size() == 8);
  }

  // run_if(in_state(...)) on/off per state, with and without dt
  {
    State<App> state{App::Menu};
    CHECK(run_if(in_state(state, App::Menu)));
    CHECK(run_if(in_state(state, App::Menu), 0.016f));
    CHECK(!run_if(in_state(state, App::Game)));
    CHECK(!run_if(in_state(state, App::Game), 0.016f));
    CHECK(run_if([] { return true; }));
    CHECK(!run_if([](float dt) { return dt > 1.f; }, 0.016f));
    state.set(App::Game);
    CHECK(!run_if(in_state(state, App::Menu)));
    CHECK(run_if(in_state(state, App::Game)));
  }

  // one SystemManager, systems gated by state
  {
    State<App> state{App::Menu};
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

  std::printf("app_states_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures ? 1 : 0;
}
