#pragma once


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

  bool set(E next) {
    if (next == current_) return false;
    const E old = current_;
    for (auto &hook : exit_hooks_) hook(old);
    current_ = next;
    for (auto &hook : enter_hooks_) hook(next);
    return true;
  }

 private:
  E current_;
  std::vector<Hook> enter_hooks_;
  std::vector<Hook> exit_hooks_;
};

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

}
