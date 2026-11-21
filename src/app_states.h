#pragma once

// App states without a manager per state.
//
//   enum class App { Menu, Game, Pause };
//   afterhours::State<App> state{App::Menu};
//   state.on_exit([](App s) { /* leaving s */ });
//   state.on_enter([](App s) { /* entered s */ });
//   state.set(App::Game);
//
//   struct MenuSystem : afterhours::System<> {
//     bool should_run(float dt) const override {
//       return afterhours::run_if(afterhours::in_state(state, App::Menu), dt);
//     }
//     ...
//   };
//
// Register every system on the one SystemManager and gate by state, instead
// of keeping a SystemManager per state and switching managers per frame.
// Deferred on purpose: transition tables, guards, timed transitions and a
// state stack — games that need those keep them game-side for now.

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace afterhours {

template<typename E>
struct State {
  using Hook = std::function<void(E)>;

  explicit State(E initial) : current_(initial) {}

  E current() const { return current_; }
  bool is(E wanted) const { return current_ == wanted; }

  void on_enter(Hook hook) { enter_hooks_.push_back(std::move(hook)); }
  void on_exit(Hook hook) { exit_hooks_.push_back(std::move(hook)); }

  // Same-state set() is a no-op (returns false, no hooks refire), so callers
  // can set() unconditionally every frame.
  bool set(E next) {
    if (next == current_) return false;
    const E old = current_;
    for (auto &hook : exit_hooks_) hook(old);
    // Current changes between exit and enter so enter hooks read the new state.
    current_ = next;
    for (auto &hook : enter_hooks_) hook(next);
    return true;
  }

 private:
  E current_;
  std::vector<Hook> enter_hooks_;
  std::vector<Hook> exit_hooks_;
};

// Predicate for run_if / should_run: true while `state` holds `wanted`.
template<typename E>
struct InState {
  const State<E> *state;
  E wanted;
  bool operator()() const { return state->is(wanted); }
  bool operator()(float) const { return state->is(wanted); }
};

template<typename E>
InState<E> in_state(const State<E> &state, E wanted) {
  return InState<E>{&state, wanted};
}

// Evaluate a gate predicate, with or without the frame dt should_run receives.
template<typename Pred>
bool run_if(Pred &&pred) {
  return static_cast<bool>(pred());
}

template<typename Pred>
bool run_if(Pred &&pred, float dt) {
  if constexpr (std::is_invocable_v<Pred, float>) {
    return static_cast<bool>(pred(dt));
  } else {
    return static_cast<bool>(pred());
  }
}

} // namespace afterhours
