#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "../core/base_component.h"
#include "../core/system.h"
#include "../developer.h"
#include "../ecs.h"
#include "input_system.h"

namespace afterhours {

struct app_states : developer::Plugin {
  template<typename E>
  struct State {
    using Hook = std::function<void(E)>;

    explicit State(E initial) : current_value(initial) {}

    E current() const { return current_value; }

    bool is(E wanted) const { return current_value == wanted; }

    void on_enter(Hook hook) { enter_hooks.push_back(std::move(hook)); }

    void on_exit(Hook hook) { exit_hooks.push_back(std::move(hook)); }

    bool set(E next) {
      if (next == current_value) return false;
      const E old = current_value;
      for (auto &hook : exit_hooks) hook(old);
      current_value = next;
      for (auto &hook : enter_hooks) hook(next);
      return true;
    }

   private:
    E current_value;
    std::vector<Hook> enter_hooks;
    std::vector<Hook> exit_hooks;
  };

  template<typename E>
  struct InState {
    const State<E> *state;
    E wanted;

    bool operator()() const { return state->is(wanted); }
  };

  template<typename E>
  static InState<E> in_state(const State<E> &state, E wanted) {
    return InState<E>{&state, wanted};
  }

  template<typename Pred>
  static bool run_if(Pred &&pred, float dt = 0.f) {
    if constexpr (std::is_invocable_v<Pred, float>) {
      return static_cast<bool>(pred(dt));
    } else {
      return static_cast<bool>(pred());
    }
  }

  template<typename E>
  struct HasState : BaseComponent {
    State<E> state;

    HasState() : state(E{}) {}

    explicit HasState(E initial) : state(initial) {}
  };

  template<typename E, typename Action>
  struct Transition {
    E from;
    Action action;
    E to;
  };

  template<typename E, typename Action>
  struct TransitionSystem : System<> {
    std::vector<Transition<E, Action>> transitions;

    explicit TransitionSystem(std::vector<Transition<E, Action>> mapping)
        : transitions(std::move(mapping)) {}

    void once(const float) override {
      auto *has_state = EntityHelper::get_singleton_cmp<HasState<E>>();
      if (!has_state) return;
      for (auto &transition : transitions) {
        if (has_state->state.is(transition.from) &&
            input::is_pressed<Action>(transition.action)) {
          has_state->state.set(transition.to);
          return;
        }
      }
    }
  };

  static void add_singleton_components(Entity &) {}

  static void enforce_singletons(SystemManager &) {}

  static void register_update_systems(SystemManager &) {}

  template<typename E>
  static void add_singleton_components(Entity &entity, E initial) {
    entity.addComponent<HasState<E>>(initial);
    EntityHelper::registerSingleton<HasState<E>>(entity);
  }

  template<typename E>
  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<HasState<E>>>());
  }

  template<typename E, typename Action>
  static void register_update_systems(
      SystemManager &sm, std::vector<Transition<E, Action>> mapping) {
    sm.register_update_system(
        std::make_unique<TransitionSystem<E, Action>>(std::move(mapping)));
  }
};

static_assert(developer::PluginCore<app_states>,
              "app_states must implement the core plugin interface");

} // namespace afterhours
