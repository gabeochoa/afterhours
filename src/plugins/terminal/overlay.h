#pragma once

#include "ui.h"
#include "../modal.h"

namespace afterhours::terminal {

template <typename Layer>
class Overlay {
 public:
  Overlay(ProvidesLayeredInputMapping<Layer> &mapping, Layer terminal_layer)
      : mapping_(mapping), terminal_layer_(terminal_layer) {}
  Overlay(const Overlay &) = delete;
  Overlay &operator=(const Overlay &) = delete;
  ~Overlay() { close(); }

  bool is_open() const { return previous_layer_.has_value(); }

  bool open() {
    if (is_open()) return true;
    if (!mapping_.layers.contains(terminal_layer_)) return false;
    previous_layer_ = mapping_.active_layer;
    mapping_.set_active_layer(terminal_layer_);
    clear_collected_input();
    return true;
  }

  void close() {
    if (!is_open()) return;
    if (mapping_.active_layer == terminal_layer_)
      mapping_.set_active_layer(*previous_layer_);
    previous_layer_.reset();
    clear_collected_input();
  }

  void toggle() {
    if (is_open()) { close(); return; }
    open();
  }

 private:
  static void clear_collected_input() {
    auto collector = input::get_input_collector();
    if (!collector.has_value()) return;
    collector.inputs().clear();
    collector.inputs_pressed().clear();
    collector.inputs_pressed_repeat().clear();
  }

  ProvidesLayeredInputMapping<Layer> &mapping_;
  Layer terminal_layer_;
  std::optional<Layer> previous_layer_;
};

struct OverlayStyle {
  ModalConfig window = ModalConfig{}.with_size(ui::percent(0.9f), ui::h720(480.f));
  ui::imm::ComponentConfig panel;
  AutocompleteStyle autocomplete;
};

template <typename Layer>
inline auto overlay(ui::imm::HasUIContext auto &ctx, ui::imm::EntityParent parent,
                    Console &console, Overlay<Layer> &controls, OverlayStyle style = {}) {
  using namespace ui;
  using namespace ui::imm;
  using Action = typename std::remove_reference_t<decltype(ctx)>::value_type;
  auto &entity = deref(parent).first;
  auto content_parent = mk(entity, 1);
  auto &state = deref(content_parent).first.template addComponentIfMissing<detail::PanelState>();
  const bool was_open = entity.template has<modal::Modal>() &&
      entity.template get<modal::Modal>().was_open_last_frame;
  if (controls.is_open() && !was_open) state.autocomplete.refresh(console, true);
  if (controls.is_open() && (!was_open || modal::top_modal() == entity.id)) {
    state.autocomplete.refresh(console);
    const bool completing = !state.autocomplete.matches.empty() &&
        (ctx.has_focus(state.input_id) || ctx.has_focus(state.field_id));
    if (!completing && ctx.pressed(Action::MenuBack)) controls.close();
  }
  if (was_open != controls.is_open()) {
    ctx.last_action = Action::None;
    ctx.all_actions.reset();
    ctx.all_actions_repeat.reset();
  }
  style.window.closed_by = ClosedBy::None;
  style.window.show_close_button = false;
  bool open = controls.is_open();
  auto window = afterhours::modal(ctx, parent, open, style.window);
  if (!open) {
    controls.close();
    state.focus_requested = true;
    return window;
  }
  if (!was_open) state.focus_requested = true;
  if (style.panel.size.is_default)
    style.panel.with_size({percent(1.f), expand()});
  style.panel.with_render_layer(window.ent().template get<modal::Modal>().render_layer);
  panel(ctx, content_parent, console, style.panel, style.autocomplete);
  return window;
}

}
