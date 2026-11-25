// E2E Testing Framework - UI Plugin Integration
// Automatic command handlers for UI components
// Include this if you're using both e2e_testing and ui plugins
#pragma once

#include <algorithm>

#include "../../core/key_codes.h"
#include "command_handlers.h"
#include "draw_commands.h"
#include "e2e_testing.h"
#include "pending_command.h"
#include "perf_commands.h"
#include "test_input.h"

#include "../autolayout.h"
#include "../clipboard.h"
#include "../ui/components.h"
#include "../ui/context.h"
#include "../ui.h"
#include "../ui/text_input/state.h"
#include "../ui/text_input/text_area_state.h"
#include "../ui/ui_collection.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <string_view>
#include <magic_enum/magic_enum.hpp>

namespace afterhours {
namespace testing {
namespace ui_commands {

inline EntityQuery<> ui_query() {
  return EntityQuery<>(ui::UICollectionHolder::get().collection,
                       {.ignore_temp_warning = true});
}

// Helper: get the screen-space rect of a UI component, the same way render
// and hit-test place it. Read-only, so a const view of the entity is enough.
inline RectangleType get_screen_rect(const Entity &entity) {
  const auto &cmp = entity.get<ui::UIComponent>();
  RectangleType rect = cmp.rect();
  if (entity.has<ui::HasUIModifiers>()) {
    rect = entity.get<ui::HasUIModifiers>().apply_modifier(rect);
  }
  return ui::detail::apply_ancestor_transform(entity, rect);
}

// Centre of an element's visible (clipped) rect; nullopt when fully
// clipped, so clicks land on a point the user could actually hit.
inline std::optional<Position> visible_center(const Entity &entity) {
  auto rect = ui::detail::hit_rect(entity, entity.get<ui::UIComponent>());
  if (rect.width <= 0.f || rect.height <= 0.f) return std::nullopt;
  return Position{rect.x + rect.width / 2, rect.y + rect.height / 2};
}

enum struct UIMatchKind {
  DebugName,
  TextSubstring,
  TextExact,
};

enum struct UIVisibility {
  Any,
  Rendered,
  RenderedPartiallyVisible,
  Clickable,
};
inline bool is_within_scope(const Entity &entity, EntityID scope);
inline bool is_partially_visible(const Entity &entity);
inline std::string describe_candidate(Entity &entity) {
  auto rect = get_screen_rect(entity);
  std::string desc = std::format("#{} @({},{})", entity.id,
                                 std::lroundf(rect.x), std::lroundf(rect.y));
  EntityID cur = entity.has<ui::UIComponent>()
                     ? entity.get<ui::UIComponent>().parent
                     : -1;
  for (int depth = 0; depth < 64 && cur != -1; ++depth) {
    auto opt = ui::UICollectionHolder::getEntityForID(cur);
    if (!opt.valid()) break;
    if (opt.asE().template has<ui::UIComponentDebug>()) {
      auto &dbg = opt.asE().template get<ui::UIComponentDebug>();
      if (!dbg.name_value.empty()) {
        desc += std::format(" under '{}'", dbg.name());
        break;
      }
    }
    cur = opt.asE().template has<ui::UIComponent>()
              ? opt.asE().template get<ui::UIComponent>().parent
              : -1;
  }
  return desc;
}

inline RefEntities resolve_ui_candidates(
    const std::string &needle, EntityID scope, UIMatchKind kind,
    UIVisibility vis,
    const std::function<bool(Entity &)> &extra = nullptr) {
  RefEntities out;
  // TODO: make kind/visibility first-class query modifiers (whereRendered/whereClickable?) instead of hand-rolled predicates here.
  for (Entity &e : ui_query()
           .whereHasComponent<ui::UIComponent>()
           .whereLambdaIf(kind == UIMatchKind::DebugName,
                          [&](const Entity &cand) {
                            return cand.has<ui::UIComponentDebug>() &&
                                   cand.get<ui::UIComponentDebug>().name() ==
                                       needle;
                          })
           .whereLambdaIf(kind == UIMatchKind::TextExact,
                          [&](const Entity &cand) {
                            return cand.has<ui::HasLabel>() &&
                                   cand.get<ui::HasLabel>().label == needle;
                          })
           .whereLambdaIf(kind == UIMatchKind::TextSubstring,
                          [&](const Entity &cand) {
                            return cand.has<ui::HasLabel>() &&
                                   cand.get<ui::HasLabel>().label.find(
                                       needle) != std::string::npos;
                          })
           .whereLambdaIf(scope != -1,
                          [&](const Entity &cand) {
                            return is_within_scope(cand, scope);
                          })
           .whereLambdaIf(vis == UIVisibility::Rendered,
                          [&](const Entity &cand) {
                            return cand.get<ui::UIComponent>()
                                .was_rendered_to_screen;
                          })
           .whereLambdaIf(vis == UIVisibility::RenderedPartiallyVisible,
                          [&](const Entity &cand) {
                            return cand.get<ui::UIComponent>()
                                       .was_rendered_to_screen &&
                                   is_partially_visible(cand);
                          })
           .whereLambdaIf(vis == UIVisibility::Clickable,
                          [&](const Entity &cand) {
                            return cand.get<ui::UIComponent>()
                                       .was_rendered_to_screen &&
                                   visible_center(const_cast<Entity &>(cand))
                                       .has_value();
                          })
           .gen()) {
    if (extra && !extra(e)) continue;
    out.push_back(e);
  }
  return out;
}

struct UIResolution {
  OptEntity entity;
  std::string ambiguous;
};

inline UIResolution resolve_ui_single(
    const std::string &needle, EntityID scope, UIMatchKind kind,
    UIVisibility vis,
    const std::function<bool(Entity &)> &extra = nullptr) {
  auto candidates = resolve_ui_candidates(needle, scope, kind, vis, extra);
  UIResolution out;
  if (candidates.size() == 1) {
    out.entity = candidates[0];
    return out;
  }
  if (candidates.empty()) return out;
  std::string list;
  for (Entity &e : candidates) {
    if (!list.empty()) list += "; ";
    list += describe_candidate(e);
  }
  out.ambiguous =
      std::format("{} elements match '{}'; scope it with under=<scope> "
                  "(in=<scope> accepted as alias) — candidates: {}",
                  candidates.size(), needle, list);
  return out;
}

enum struct ScopeStatus { Ok, NotFound, Ambiguous };
struct ScopeOutcome {
  ScopeStatus status = ScopeStatus::NotFound;
  EntityID id = -1;
  size_t count = 0;
};
inline ScopeOutcome resolve_scope_entity(const std::string &scope_name) {
  ScopeOutcome out;
  for (Entity &e : ui_query()
           .whereHasComponent<ui::UIComponentDebug>()
           .whereLambda([&](const Entity &cand) {
             return cand.get<ui::UIComponentDebug>().name_value == scope_name;
           })
           .gen()) {
    out.id = e.id;
    out.count++;
  }
  if (out.count == 0)
    out.status = ScopeStatus::NotFound;
  else if (out.count == 1)
    out.status = ScopeStatus::Ok;
  else
    out.status = ScopeStatus::Ambiguous;
  return out;
}
inline std::vector<std::string> strip_trailing_scope(
    const PendingE2ECommand &cmd, size_t start, size_t min_positionals,
    std::string &scope_name, std::string &error) {
  std::vector<std::string> pos;
  size_t end = cmd.args.size();
  while (end > start + min_positionals) {
    const std::string &tok = cmd.args[end - 1];
    if (!tok.starts_with("under=") && !tok.starts_with("in=")) break;
    std::string value = tok.substr(tok.find('=') + 1);
    if (!scope_name.empty() && scope_name != value) {
      error = std::format(
          "duplicate scope '{}' and '{}'; name one element of the subtree",
          scope_name, value);
      break;
    }
    scope_name = value;
    end--;
  }
  for (size_t i = start; i < end; i++)
    pos.push_back(cmd.args[i]);
  return pos;
}
inline std::vector<std::string> split_scope_props(
    const PendingE2ECommand &cmd, size_t start, std::string &scope_name,
    std::string &error) {
  std::vector<std::string> rest;
  for (size_t i = start; i < cmd.args.size(); i++) {
    const std::string &tok = cmd.args[i];
    if (tok.starts_with("under=") || tok.starts_with("in=")) {
      std::string value = tok.substr(tok.find('=') + 1);
      if (!scope_name.empty() && scope_name != value) {
        error = std::format(
            "duplicate scope '{}' and '{}'; name one element of the subtree",
            scope_name, value);
        continue;
      }
      scope_name = value;
    } else {
      rest.push_back(tok);
    }
  }
  return rest;
}
inline bool scope_or_done(PendingE2ECommand &cmd,
                          const std::string &cmd_label,
                          const std::string &scope_name,
                          bool retry_when_missing, EntityID &scope) {
  if (scope_name.empty()) return true;
  auto so = resolve_scope_entity(scope_name);
  if (so.status == ScopeStatus::Ambiguous) {
    cmd.fail(std::format(
        "{}: scope '{}' matches {} elements; scope it with a distinct "
        "ancestor name",
        cmd_label, scope_name, so.count));
    return false;
  }
  if (so.status == ScopeStatus::NotFound) {
    const std::string msg =
        std::format("{}: scope '{}' not found", cmd_label, scope_name);
    if (retry_when_missing)
      cmd.retry_with(msg);
    else
      cmd.fail(msg);
    return false;
  }
  scope = so.id;
  return true;
}
struct ElementPointResult {
  std::optional<Position> point;
  std::string error;
};
inline ElementPointResult element_point(
    const std::string &name, const std::vector<std::string> &off,
    bool use_offset, EntityID scope) {
  ElementPointResult r;
  auto res = resolve_ui_single(name, scope, UIMatchKind::DebugName,
                               UIVisibility::Any);
  if (!res.ambiguous.empty()) {
    r.error = res.ambiguous;
    return r;
  }
  if (!res.entity) return r;
  const RectangleType rect = get_screen_rect(res.entity.asE());
  const auto off_at = [&](size_t i, float extent) {
    if (!use_offset || off.size() <= i - 1 || off[i - 1] == "center")
      return extent * 0.5f;
    return static_cast<float>(std::atof(off[i - 1].c_str()));
  };
  r.point = Position{rect.x + off_at(1, rect.width),
                     rect.y + off_at(2, rect.height)};
  return r;
}
template <typename InputAction>
std::optional<Position> find_component_center(const std::string &name) {
  auto res = resolve_ui_single(name, -1, UIMatchKind::DebugName,
                               UIVisibility::Clickable);
  if (!res.entity) return std::nullopt;
  return visible_center(res.entity.asE());
}
template <typename InputAction>
std::optional<Position> find_component_with_text(const std::string &text) {
  auto res = resolve_ui_single(text, -1, UIMatchKind::TextSubstring,
                               UIVisibility::Clickable);
  if (!res.entity) return std::nullopt;
  return visible_center(res.entity.asE());
}
template <typename InputAction>
struct HandleClickUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_ui"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("click_ui requires component name"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "click_ui", scope_name, false, scope)) return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Clickable);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("UI component not found: {}", pos[0]));
      return;
    }
    auto centre = visible_center(res.entity.asE());
    test_input::simulate_click(centre->x, centre->y);
    cmd.consume();
  }
};

// A context menu is opened by name in the app and could only be opened by
// coordinate in a test, which goes stale the moment the layout moves.
template <typename InputAction>
struct HandleRightClickUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("right_click_ui"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("right_click_ui requires component name"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "right_click_ui", scope_name, false, scope))
      return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Clickable);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("UI component not found: {}", pos[0]));
      return;
    }
    auto centre = visible_center(res.entity.asE());
    test_input::simulate_right_click(centre->x, centre->y);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleRightClickTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("right_click_text"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("right_click_text requires text"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "right_click_text", scope_name, false, scope))
      return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::TextSubstring,
                                 UIVisibility::Clickable);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("No UI with text: {}", pos[0]));
      return;
    }
    auto centre = visible_center(res.entity.asE());
    test_input::simulate_right_click(centre->x, centre->y);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleClickTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_text"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("click_text requires text"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "click_text", scope_name, false, scope)) return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::TextSubstring,
                                 UIVisibility::Clickable);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("No UI with text: {}", pos[0]));
      return;
    }
    auto centre = visible_center(res.entity.asE());
    test_input::simulate_click(centre->x, centre->y);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleFocusUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("focus_ui"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("focus_ui requires component name"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "focus_ui", scope_name, false, scope)) return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Rendered);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("UI component not found: {}", pos[0]));
      return;
    }
    Entity &entity = res.entity.asE();
    auto &cmp = entity.get<ui::UIComponent>();
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (ctx) {
      EntityID focus_target = cmp.id;
      for (int child_id : cmp.children) {
        auto child_opt = ui::UICollectionHolder::getEntityForID(child_id);
        if (child_opt.valid() && child_opt.asE().template has<ui::InFocusCluster>()) {
          focus_target = child_id;
        }
      }
      ctx->set_focus(focus_target);
      cmd.consume();
      return;
    }
    cmd.fail(std::format("UI component not found: {}", pos[0]));
  }
};

// Handle 'tab' - simulates Tab key press
struct HandleTabCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("tab"))
      return;

    test_input::push_key(keys::TAB);
    cmd.consume();
  }
};

// Handle 'shift_tab' - simulates Shift+Tab for reverse tabbing
//
// Uses set_key_held (not set_key_down) for LEFT_SHIFT so that the modifier
// appears as "held" in the InputCollector's down-list (all_actions) but NOT
// as "pressed" in inputs_pressed. This prevents WidgetMod from overwriting
// WidgetNext in UIContext::last_action, which would break process_tabbing's
// pressed(WidgetNext) check.
struct HandleShiftTabCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("shift_tab"))
      return;

    input_injector::set_key_held(keys::LEFT_SHIFT);
    test_input::push_key(keys::TAB);
    shift_tab_detail::release_countdown = 3;
    cmd.consume();
  }
};

// Release shift key after shift_tab countdown expires
struct HandleShiftTabRelease : System<> {
  bool should_iterate() const override { return false; }
  virtual void once(float) override {
    using namespace shift_tab_detail;
    if (release_countdown > 0) {
      release_countdown--;
      if (release_countdown == 0) {
        input_injector::set_key_up(keys::LEFT_SHIFT);
      }
    }
  }
};

// Handle 'enter' - simulates Enter key press (activate focused element)
struct HandleEnterCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("enter"))
      return;

    test_input::push_key(keys::ENTER);
    cmd.consume();
  }
};

// Handle 'escape' - simulates Escape key press
struct HandleEscapeCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("escape"))
      return;

    test_input::push_key(keys::ESCAPE);
    cmd.consume();
  }
};

// Handle 'arrow direction' - simulates arrow key
struct HandleArrowCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("arrow"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("arrow requires direction (up/down/left/right)");
      return;
    }

    const auto &dir = cmd.arg(0);
    int key = 0;
    if (dir == "up")
      key = keys::UP;
    else if (dir == "down")
      key = keys::DOWN;
    else if (dir == "left")
      key = keys::LEFT;
    else if (dir == "right")
      key = keys::RIGHT;
    else {
      cmd.fail(std::format("Invalid arrow direction: {}", dir));
      return;
    }

    test_input::push_key(key);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleExpectFocusedCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_focused"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("expect_focused requires component name"); return; }
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) { cmd.fail("UIContext not found"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_focused", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Any);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("UI component not found: {}", pos[0]));
      return;
    }
    Entity &entity = res.entity.asE();
    auto &cmp = entity.get<ui::UIComponent>();
    if (ctx->has_focus(cmp.id)) {
      cmd.consume();
      return;
    }
    for (int child_id : cmp.children) {
      auto child_opt = ui::UICollectionHolder::getEntityForID(child_id);
      if (child_opt.valid() &&
          child_opt.asE().template has<ui::InFocusCluster>() &&
          ctx->has_focus(child_id)) {
        cmd.consume();
        return;
      }
    }
    std::string holder = "nothing";
    if (ctx->focus_id != ctx->ROOT) {
      auto opt = ui::UICollectionHolder::getEntityForID(ctx->focus_id);
      holder = (opt.valid() && opt.asE().template has<ui::UIComponentDebug>())
                   ? opt.asE().template get<ui::UIComponentDebug>().name()
                   : std::format("entity {}", ctx->focus_id);
    }
    cmd.retry_with(std::format("'{}' is not focused, '{}' is. Focus set by {}",
                               pos[0], holder, ctx->focus_origin()));
  }
};

// Forward declarations: defined with the other scope helpers below.
inline bool is_within_scope(const Entity &entity, EntityID scope);
inline bool is_partially_visible(const Entity &entity);

// Handle 'expect_no_focus' - asserts nothing holds keyboard focus.
template <typename InputAction>
struct HandleExpectNoFocusCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_no_focus"))
      return;
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }
    if (ctx->focus_id == ctx->ROOT) {
      cmd.consume();
      return;
    }
    std::string holder = std::format("entity {}", ctx->focus_id);
    auto opt = ui::UICollectionHolder::getEntityForID(ctx->focus_id);
    if (opt.valid() && opt.asE().template has<ui::UIComponentDebug>())
      holder = opt.asE().template get<ui::UIComponentDebug>().name();
    cmd.retry_with(std::format("'{}' is focused, expected none", holder));
  }
};

template <typename InputAction>
struct HandleExpectHoveredCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_hovered"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("expect_hovered requires component name"); return; }
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) { cmd.fail("UIContext not found"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_hovered", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Any);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("UI component not found: {}", pos[0]));
      return;
    }
    if (ctx->is_hot(res.entity->id)) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format("'{}' is not hovered", pos[0]));
    }
  }
};

// exact label, but only if the click would land on it. click_text clicks the
// first substring match at its centre without checking what is on top, so a
// click that hit a modal or a covering element still passed. This one
// resolves the hit target the way a real click would (same ResolveHitTarget
// walk, including modal gates) and retries while a different element covers
// the centre.
template <typename InputAction>
struct HandleClickLabelCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_label"))
      return;
    if (!cmd.has_args(1)) { cmd.fail("click_label requires text"); return; }
    if (render_checkpoint_pending(cmd)) return;
    const std::string &text = cmd.arg(0);
    // Preserve main's click_label contract on top of the unified resolver:
    // in= (and the PR's under= alias) may appear anywhere after the text,
    // and any other trailing argument fails loudly instead of being ignored.
    std::string scope_name, scope_err;
    auto extra = split_scope_props(cmd, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (!extra.empty()) {
      cmd.fail(std::format("click_label: unexpected argument '{}'", extra[0]));
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "click_label", scope_name, true, scope)) return;
    auto res = resolve_ui_single(text, scope, UIMatchKind::TextExact,
                                 UIVisibility::Rendered);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("No UI with text: {}", text));
      return;
    }
    Entity &target = res.entity.asE();
    if (!target.has_any<ui::HasClickListener, ui::HasDragListener>()) {
      cmd.fail(std::format("click_label \"{}\": element has no click listener", text));
      return;
    }
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) { cmd.fail("UIContext not found"); return; }
    const auto rect = get_screen_rect(target);
    const Vector2Type centre{rect.x + rect.width / 2, rect.y + rect.height / 2};
    const auto saved_pos = ctx->mouse.pos;
    const EntityID saved_hot = ctx->hot_id;
    ctx->mouse.pos = centre;
    ui::ResolveHitTarget<InputAction> resolver;
    resolver.context = ctx;
    resolver.resolve();
    const EntityID winner = ctx->hot_id;
    ctx->mouse.pos = saved_pos;
    ctx->hot_id = saved_hot;
    if (winner == target.id) {
      test_input::simulate_click(centre.x, centre.y);
      cmd.consume();
      return;
    }
    if (winner == ctx->ROOT) {
      cmd.retry_with(std::format("click_label \"{}\": nothing hit-testable at its centre", text));
      return;
    }
    std::string cover = std::format("entity {}", winner);
    auto cover_opt = ui::UICollectionHolder::getEntityForID(winner);
    if (cover_opt.valid() && cover_opt.asE().template has<ui::UIComponentDebug>())
      cover = cover_opt.asE().template get<ui::UIComponentDebug>().name();
    cmd.retry_with(std::format("click_label \"{}\": centre is covered by '{}'", text, cover));
  }
};

template <typename InputAction>
struct HandleClickButtonCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_button"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("click_button requires button text"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "click_button", scope_name, false, scope)) return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::TextSubstring, UIVisibility::Rendered,
        [](Entity &e) { return e.has<ui::HasClickListener>(); });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("Button not found: {}", pos[0]));
      return;
    }
    auto rect = get_screen_rect(res.entity.asE());
    test_input::simulate_click(rect.x + rect.width / 2,
                               rect.y + rect.height / 2);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleToggleCheckboxCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("toggle_checkbox"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) { cmd.fail("toggle_checkbox requires name"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "toggle_checkbox", scope_name, false, scope))
      return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Rendered,
        [](Entity &e) { return e.has<ui::HasCheckboxState>(); });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("Checkbox not found: {}", pos[0]));
      return;
    }
    auto rect = get_screen_rect(res.entity.asE());
    test_input::simulate_click(rect.x + rect.width / 2,
                               rect.y + rect.height / 2);
    cmd.consume();
  }
};

// slider() puts the caller's name on the outer entity and the state on a
// child, so no entity has both. Find the state under the named widget.
template <typename StateT>
inline OptEntity find_state_under(Entity &named) {
  if (named.has<StateT>())
    return OptEntity{named};
  if (!named.has<ui::UIComponent>())
    return {};
  for (EntityID child_id : named.get<ui::UIComponent>().children) {
    OptEntity child = ui::UICollectionHolder::getEntityForID(child_id);
    if (!child.valid())
      continue;
    if (OptEntity found = find_state_under<StateT>(child.asE()); found.valid())
      return found;
  }
  return {};
}

// set_slider: press the handle centre, move to the target, release.
template <typename InputAction>
struct HandleSetSliderCommand : System<PendingE2ECommand> {
  int phase = 0; // 0=press, 1=move, 2=release
  int owner_line = -1;
  float target_x = 0.f;
  float target_y = 0.f;

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("set_slider"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) { cmd.fail("set_slider requires name and value"); return; }
    float value = 0.f;
    try {
      value = std::stof(pos[1]);
    } catch (...) {
      cmd.fail(std::format("Invalid slider value: {}", pos[1]));
      return;
    }
    if (render_checkpoint_pending(cmd)) return;

    if (owner_line != cmd.line_number) {
      owner_line = cmd.line_number;
      phase = 0;
    }

    if (phase == 1) {
      test_input::set_mouse_position(target_x, target_y);
      phase = 2;
      cmd.retry();
      return;
    }
    if (phase == 2) {
      test_input::simulate_mouse_release();
      phase = 0;
      cmd.consume();
      return;
    }

    EntityID scope = -1;
    if (!scope_or_done(cmd, "set_slider", scope_name, false, scope)) return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Rendered,
        [](Entity &e) {
          return find_state_under<ui::HasSliderState>(e).valid();
        });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("Slider not found: {}", pos[0]));
      return;
    }
    OptEntity target = find_state_under<ui::HasSliderState>(res.entity.asE());
    Entity &entity = target.asE();
    auto rect = get_screen_rect(entity);
    const float pct = std::clamp(value, 0.f, 1.f);
    const float current =
        std::clamp(entity.get<ui::HasSliderState>().value, 0.f, 1.f);
    const float handle_w = rect.width * 0.25f;
    const float travel = rect.width - handle_w;
    const float grab_x = rect.x + handle_w * 0.5f + current * travel;
    target_x = std::min(rect.x + handle_w * 0.5f + pct * travel,
                        rect.x + rect.width - 0.5f);
    target_y = rect.y + rect.height / 2;
    press_mouse_at(grab_x, target_y);
    phase = 1;
    cmd.retry();
  }
};

template <typename InputAction>
struct HandleSelectDropdownCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("select_dropdown"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) { cmd.fail("select_dropdown requires name and option"); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "select_dropdown", scope_name, false, scope))
      return;
    if (render_checkpoint_pending(cmd)) return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Rendered,
        [](Entity &e) { return e.has<ui::HasDropdownState>(); });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.fail(std::format("Dropdown not found: {}", pos[0]));
      return;
    }
    auto rect = get_screen_rect(res.entity.asE());
    test_input::simulate_click(rect.x + rect.width / 2,
                               rect.y + rect.height / 2);
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleExpectCheckboxCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_checkbox"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) { cmd.fail("expect_checkbox requires name and state"); return; }
    bool expected =
        pos[1] == "true" || pos[1] == "checked" || pos[1] == "1";
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_checkbox", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Any,
        [](Entity &e) { return e.has<ui::HasCheckboxState>(); });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("Checkbox not found: {}", pos[0]));
      return;
    }
    auto &checkbox = res.entity->template get<ui::HasCheckboxState>();
    if (checkbox.on == expected) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format("Checkbox '{}' is {}, expected {}", pos[0],
                                 checkbox.on ? "checked" : "unchecked",
                                 pos[1]));
    }
  }
};

template <typename InputAction>
struct HandleExpectAccessibleCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_accessible")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_accessible requires a name or name=<accessible name>");
      return;
    }
    std::map<std::string, std::string> want;
    size_t first_prop = 0;
    std::string debug_name;
    if (cmd.arg(0).find('=') == std::string::npos) {
      debug_name = cmd.arg(0);
      first_prop = 1;
    }
    for (size_t i = first_prop; i < cmd.args.size(); ++i) {
      const std::string &raw = cmd.arg(i);
      auto eq = raw.find('=');
      if (eq == std::string::npos || eq == 0) {
        cmd.fail(std::format("expect_accessible: expected prop=value, got '{}'",
                             raw));
        return;
      }
      want[raw.substr(0, eq)] = raw.substr(eq + 1);
    }
    if (!debug_name.empty() && want.empty()) want["name"] = debug_name;
    auto query = ui_query()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::HasAccessibility>()
                     .whereLambda([&](const Entity &e) {
                       if (!e.get<ui::UIComponent>().was_rendered_to_screen)
                         return false;
                       if (!debug_name.empty())
                         return e.has<ui::UIComponentDebug>() &&
                                e.get<ui::UIComponentDebug>().name() ==
                                    debug_name;
                       auto it = want.find("name");
                       return it == want.end() ||
                              e.get<ui::HasAccessibility>().name == it->second;
                     })
                     .first();
    for (Entity &entity : query.gen()) {
      auto &a = entity.get<ui::HasAccessibility>();
      for (auto &[prop, expected] : want) {
        std::string actual;
        if (prop == "name") actual = a.name;
        else if (prop == "role") actual = ui::accessible_role_name(a.role);
        else if (prop == "value") actual = a.value;
        else if (prop == "description") actual = a.description;
        else {
          cmd.fail(std::format("expect_accessible: unknown prop '{}'", prop));
          return;
        }
        if (actual != expected) {
          cmd.retry_with(std::format(
              "expect_accessible {}={} but got {} (role={} name=\"{}\" "
              "value=\"{}\")",
              prop, expected, actual, ui::accessible_role_name(a.role),
              a.name, a.value));
          return;
        }
      }
      cmd.consume();
      return;
    }
    cmd.retry_with(std::format("expect_accessible: no element found for '{}'",
                               debug_name.empty() ? want["name"] : debug_name));
  }
};

// Handle 'expect_slider name value [tolerance]' - asserts slider value
template <typename InputAction>
struct HandleExpectSliderCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_slider"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) { cmd.fail("expect_slider requires name and value"); return; }
    float expected = 0.f;
    try {
      expected = std::stof(pos[1]);
    } catch (...) {
      cmd.fail(std::format("Invalid slider value: {}", pos[1]));
      return;
    }
    float tolerance = 0.01f;
    if (pos.size() > 2) {
      try {
        tolerance = std::stof(pos[2]);
      } catch (...) {
      }
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_slider", scope_name, true, scope)) return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Any,
        [](Entity &e) {
          return find_state_under<ui::HasSliderState>(e).valid();
        });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("Slider not found: {}", pos[0]));
      return;
    }
    OptEntity target = find_state_under<ui::HasSliderState>(res.entity.asE());
    auto &slider = target.asE().get<ui::HasSliderState>();
    if (std::abs(slider.value - expected) <= tolerance) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format("Slider '{}' is {}, expected {}", pos[0],
                                 slider.value, expected));
    }
  }
};

// Handle 'action ActionName' - triggers a semantic input action
// Example: action WidgetLeft, action TextBackspace, action WidgetPress
template <typename InputAction>
struct HandleActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("action"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("action requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    // Parse action name using magic_enum
    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(std::format("Unknown action: {}. Valid actions: {}, ...",
                           cmd.arg(0),
                           magic_enum::enum_names<InputAction>()[0]));
      return;
    }

    ctx->defer([ctx, action = *action] { ctx->last_action = action; });
    cmd.consume();
  }
};

// Handle 'hold ActionName' - holds an action down (sets in bitset)
template <typename InputAction>
struct HandleHoldActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("hold"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("hold requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(std::format("Unknown action: {}", cmd.arg(0)));
      return;
    }

    auto idx = magic_enum::enum_index(*action);
    if (idx) {
      ctx->all_actions[*idx] = true;
    }
    cmd.consume();
  }
};

// Handle 'release ActionName' - releases a held action
template <typename InputAction>
struct HandleReleaseActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("release"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("release requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(std::format("Unknown action: {}", cmd.arg(0)));
      return;
    }

    auto idx = magic_enum::enum_index(*action);
    if (idx) {
      ctx->all_actions[*idx] = false;
    }
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleExpectInputTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_input_text"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) {
      cmd.fail("expect_input_text requires component name and expected text");
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_input_text", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Any,
        [](Entity &e) {
          return e.has<text_input::HasTextInputState>() ||
                 e.has<text_input::HasTextAreaState>();
        });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("Text input not found: {}", pos[0]));
      return;
    }
    Entity &entity = res.entity.asE();
    const std::string actual =
        entity.has<text_input::HasTextInputState>()
            ? entity.get<text_input::HasTextInputState>().text()
            : entity.get<text_input::HasTextAreaState>().text();
    if (actual == pos[1]) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format(
          "Input '{}' text is \"{}\", expected \"{}\"", pos[0], actual, pos[1]));
    }
  }
};

// than which byte offsets. Offsets from a click depend on font metrics, so an
// offset assertion on a clicked selection is really an assertion about the
// font; this one says "double clicking there selected that word" and stays
// true whatever the face measures.
template <typename InputAction>
struct HandleExpectSelectedTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_selected_text"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 2, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 2) {
      cmd.fail("expect_selected_text requires name and expected text");
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_selected_text", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Any,
        [](Entity &e) {
          return e.has<text_input::HasTextInputState>() ||
                 e.has<text_input::HasTextAreaState>();
        });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("Text field not found: {}", pos[0]));
      return;
    }
    Entity &entity = res.entity.asE();
    const std::string got =
        entity.has<text_input::HasTextAreaState>()
            ? entity.get<text_input::HasTextAreaState>().selected_text()
            : entity.get<text_input::HasTextInputState>().selected_text();
    if (got == pos[1]) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format("'{}' has \"{}\" selected, expected \"{}\"",
                                 pos[0], got, pos[1]));
    }
  }
};

// Handle 'expect_clipboard "expected"' - asserts the clipboard contents.
// Reads whatever clipboard::get_text() returns (test provider when one is
// installed, otherwise the backend clipboard), so Copy paths are assertable
// without leaving the runner.
struct HandleExpectClipboardCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_clipboard")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_clipboard requires expected text");
      return;
    }
    const std::string actual = clipboard::get_text();
    if (actual == cmd.arg(0)) {
      cmd.consume();
      return;
    }
    cmd.retry_with(std::format("clipboard is \"{}\", expected \"{}\"", actual, cmd.arg(0)));
  }
};

// Clear a field's multi-click bookkeeping so the next click starts a new
// gesture rather than extending the last one.
inline void reset_multi_click(const std::string &name) {
  auto query = ui_query()
                   .whereHasComponent<ui::UIComponentDebug>()
                   .whereLambda([&](const Entity &e) {
                     return e.get<ui::UIComponentDebug>().name() == name &&
                            (e.has<text_input::HasTextInputState>() ||
                             e.has<text_input::HasTextAreaState>());
                   })
                   .first();
  for (Entity &e : query.gen()) {
    if (e.has<text_input::HasTextAreaState>()) {
      auto &s = e.get<text_input::HasTextAreaState>();
      s.click_count = 0;
      s.last_click_time = -1.f;
    } else {
      auto &s = e.get<text_input::HasTextInputState>();
      s.click_count = 0;
      s.last_click_time = -1.f;
    }
  }
}

//
// Clicking a named element at an offset rather than at raw screen coordinates:
// a layout change then retargets the click instead of silently landing on
// empty space. dx/dy are from the element's top left; omitted means centre.

namespace multi_click_ui_detail {
inline int phase = 0;
inline float x = 0.f, y = 0.f;
inline void reset() {
  phase = 0;
  x = y = 0.f;
}
} // namespace multi_click_ui_detail

template <typename InputAction, int Clicks>
struct HandleMultiClickUICommand : System<PendingE2ECommand> {
  static constexpr const char *name_for() {
    return Clicks == 3 ? "triple_click_ui" : "double_click_ui";
  }

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is(name_for()))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.empty()) {
      cmd.fail(std::format("{} requires a component name", name_for()));
      return;
    }
    using namespace multi_click_ui_detail;

    if (phase == 0) {
      EntityID scope = -1;
      if (!scope_or_done(cmd, name_for(), scope_name, false, scope)) return;
      std::vector<std::string> off(pos.begin() + 1, pos.end());
      auto r = element_point(pos[0], off, true, scope);
      if (!r.error.empty()) { cmd.fail(r.error); return; }
      if (!r.point) {
        cmd.fail(std::format("UI component not found: {}", pos[0]));
        return;
      }
      x = r.point->x;
      y = r.point->y;
      reset_multi_click(pos[0]);
    }

    // Each click needs the previous one's auto-release to land first, so the
    // clicks sit on every third phase.
    if (phase % 3 == 0 && phase / 3 < Clicks)
      test_input::simulate_click(x, y);
    phase++;
    if (phase / 3 >= Clicks && phase % 3 == 0) {
      phase = 0;
      cmd.consume();
    } else {
      cmd.retry();
    }
  }
};

struct HandlePointerUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() ||
        (!cmd.is("mouse_down_ui") && !cmd.is("mouse_move_ui") &&
         !cmd.is("scroll_wheel_ui")))
      return;
    const bool wheel = cmd.is("scroll_wheel_ui");
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, wheel ? 3 : 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < (wheel ? 3u : 1u)) {
      cmd.fail(wheel ? "scroll_wheel_ui requires a name and dx dy"
                     : std::format("{} requires a component name", cmd.name));
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, cmd.name, scope_name, false, scope)) return;
    std::vector<std::string> off(pos.begin() + 1, pos.end());
    auto r = element_point(pos[0], off, !wheel, scope);
    if (!r.error.empty()) { cmd.fail(r.error); return; }
    if (!r.point) {
      cmd.fail(std::format("UI component not found: {}", pos[0]));
      return;
    }
    if (cmd.is("mouse_down_ui")) {
      press_mouse_at(r.point->x, r.point->y);
    } else {
      test_input::set_mouse_position(r.point->x, r.point->y);
      if (wheel) {
        float dx = 0.f, dy = 0.f;
        try {
          dx = std::stof(pos[1]);
          dy = std::stof(pos[2]);
        } catch (...) {
        }
        input_injector::set_mouse_wheel(dx, dy);
      }
    }
    cmd.consume();
  }
};

template <typename InputAction>
struct HandleExpectInputSelectionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_input_selection"))
      return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 3, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 3) {
      cmd.fail(
          "expect_input_selection requires name, start, and end positions");
      return;
    }
    int expected_start = 0, expected_end = 0;
    try {
      expected_start = std::stoi(pos[1]);
      expected_end = std::stoi(pos[2]);
    } catch (...) {
      cmd.fail("expect_input_selection: start and end must be integers");
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_input_selection", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(
        pos[0], scope, UIMatchKind::DebugName, UIVisibility::Any,
        [](Entity &e) {
          return e.has<text_input::HasTextInputState>() ||
                 e.has<text_input::HasTextAreaState>();
        });
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry_with(std::format("Text input not found: {}", pos[0]));
      return;
    }
    Entity &entity = res.entity.asE();
    const bool multiline = entity.has<text_input::HasTextAreaState>();
    const auto sel_start =
        multiline
            ? entity.get<text_input::HasTextAreaState>().selection_start()
            : entity.get<text_input::HasTextInputState>().selection_start();
    const auto sel_end =
        multiline
            ? entity.get<text_input::HasTextAreaState>().selection_end()
            : entity.get<text_input::HasTextInputState>().selection_end();
    auto actual_start = static_cast<int>(sel_start);
    auto actual_end = static_cast<int>(sel_end);
    if (actual_start == expected_start && actual_end == expected_end) {
      cmd.consume();
    } else {
      cmd.retry_with(std::format(
          "Input '{}' selection is [{}, {}), expected [{}, {})", pos[0],
          actual_start, actual_end, expected_start, expected_end));
    }
  }
};

// ============================================================================
// dump_ui command - XML tree dump of rendered UI hierarchy
// ============================================================================

inline std::string xml_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    default: out += c; break;
    }
  }
  return out;
}

inline std::string truncate_text(const std::string &s, size_t max_len = 80) {
  if (s.size() <= max_len) return s;
  return s.substr(0, max_len) + "...";
}

inline std::string format_dim(float v) {
  if (std::fabs(v - std::round(v)) < 1e-4f)
    return std::format("{}", std::lroundf(v));
  std::string s = std::format("{:.5f}", v);
  s.erase(s.find_last_not_of('0') + 1, std::string::npos);
  if (!s.empty() && s.back() == '.') s.pop_back();
  return s;
}

inline void dump_ui_node(std::string &out, const Entity &entity, int depth) {
  if (depth > 64) return;

  const auto &cmp = entity.get<ui::UIComponent>();
  if (!cmp.was_rendered_to_screen) return;

  std::string indent(depth * 2, ' ');
  RectangleType rect = get_screen_rect(entity);

  out += indent + "<ui";

  if (entity.has<ui::UIComponentDebug>()) {
    const auto &dbg = entity.get<ui::UIComponentDebug>();
    if (dbg.type == ui::UIComponentDebug::Type::custom)
      out += " name=\"" + xml_escape(dbg.name_value) + "\"";
  }

  out += std::format(" x=\"{}\" y=\"{}\" w=\"{}\" h=\"{}\"",
                     format_dim(rect.x), format_dim(rect.y),
                     format_dim(rect.width), format_dim(rect.height));

  if (entity.has<ui::HasLabel>())
    out += " text=\"" + xml_escape(truncate_text(entity.get<ui::HasLabel>().label)) + "\"";

  if (entity.has<ui::HasAccessibility>()) {
    const auto &a = entity.get<ui::HasAccessibility>();
    if (a.role != ui::AccessibleRole::None)
      out += std::format(" role=\"{}\"", ui::accessible_role_name(a.role));
    if (!a.name.empty())
      out += " accessible_name=\"" + xml_escape(truncate_text(a.name)) + "\"";
    if (!a.value.empty())
      out += " accessible_value=\"" + xml_escape(truncate_text(a.value)) + "\"";
    if (!a.description.empty())
      out += " accessible_description=\"" +
             xml_escape(truncate_text(a.description)) + "\"";
  }
  const auto sides = [&](const char *attr, const auto &v) {
    const float t = v[ui::Axis::top], r = v[ui::Axis::right],
                b = v[ui::Axis::bottom], l = v[ui::Axis::left];
    if (t != 0.f || r != 0.f || b != 0.f || l != 0.f)
      out += std::format(" {}=\"{} {} {} {}\"", attr, t, r, b, l);
  };
  sides("padding", cmp.computed_padd);
  sides("margin", cmp.computed_margin);
  if (cmp.gap != 0.f)
    out += std::format(" gap=\"{}\"", cmp.gap);

  if (entity.has<ui::HasScrollView>()) {
    const auto &sv = entity.get<ui::HasScrollView>();
    out += std::format(" scroll_x=\"{}\" scroll_y=\"{}\"",
                       format_dim(sv.scroll_offset.x),
                       format_dim(sv.scroll_offset.y));
  }

  if (cmp.should_hide)
    out += " hidden=\"true\"";

  std::vector<const Entity *> valid_children;
  for (EntityID child_id : cmp.children) {
    OptEntity opt = ui::UICollectionHolder::getEntityForID(child_id);
    if (!opt.valid()) continue;
    const Entity &child = opt.asE();
    if (!child.has<ui::UIComponent>()) continue;
    if (!child.get<ui::UIComponent>().was_rendered_to_screen) continue;
    valid_children.push_back(&child);
  }

  if (valid_children.empty()) {
    out += "/>\n";
  } else {
    out += ">\n";
    for (const Entity *child : valid_children)
      dump_ui_node(out, *child, depth + 1);
    out += indent + "</ui>\n";
  }
}

inline bool parse_hex_color(const std::string &s, Color &out) {
  std::string_view hex = s;
  hex.remove_prefix(!hex.empty() && hex.front() == '#');
  const char *end = hex.data() + hex.size();
  unsigned int value = 0;
  if ((hex.size() != 6 && hex.size() != 8) ||
      std::from_chars(hex.data(), end, value, 16).ptr != end)
    return false;
  const unsigned int shift = (8 - static_cast<unsigned int>(hex.size())) * 4;
  value = (value << shift) | (0xffu >> (8 - shift));
  out = Color{static_cast<unsigned char>((value >> 24) & 0xff),
              static_cast<unsigned char>((value >> 16) & 0xff),
              static_cast<unsigned char>((value >> 8) & 0xff),
              static_cast<unsigned char>(value & 0xff)};
  return true;
}

inline std::string format_hex_color(const Color &c) {
  return std::format("#{:02x}{:02x}{:02x}{:02x}", c.r, c.g, c.b, c.a);
}

inline bool is_within_scope(const Entity &entity, EntityID scope) {
  EntityID current = entity.id;
  for (int depth = 0; depth < 64 && current != -1; ++depth) {
    if (current == scope)
      return true;
    OptEntity parent = ui::UICollectionHolder::getEntityForID(current);
    if (!parent.valid() || !parent.asE().has<ui::UIComponent>())
      return false;
    current = parent.asE().get<ui::UIComponent>().parent;
  }
  return false;
}

// True when the element's rect is at least partially visible: inside the
// screen viewport and inside every clipping ancestor's rect. The rendered
// flag is not enough: visibility marking only checks dimensions, so a row
// scrolled out of its viewport still counts as rendered. Same 1px threshold
// as the visible-text registry.
inline bool is_partially_visible(const Entity &entity) {
  if (!entity.has<ui::UIComponent>()) return false;
  const RectangleType rect = entity.get<ui::UIComponent>().rect();
  if (rect.width <= 0.f || rect.height <= 0.f) return false;
  auto *pcr = EntityHelper::get_singleton_cmp<window_manager::ProvidesCurrentResolution>();
  const float vw = pcr ? static_cast<float>(pcr->width()) : 1280.f;
  const float vh = pcr ? static_cast<float>(pcr->height()) : 720.f;
  const float left = std::max(rect.x, 0.f);
  const float top = std::max(rect.y, 0.f);
  const float right = std::min(rect.x + rect.width, vw);
  const float bottom = std::min(rect.y + rect.height, vh);
  if (right - left < 1.f || bottom - top < 1.f) return false;
  const auto [has_clip, clip] = ui::detail::compute_intersected_clip_rect(entity);
  if (!has_clip) return true;
  const float cleft = std::max(rect.x, clip.x);
  const float ctop = std::max(rect.y, clip.y);
  const float cright = std::min(rect.x + rect.width, clip.x + clip.width);
  const float cbottom = std::min(rect.y + rect.height, clip.y + clip.height);
  return cright - cleft >= 1.f && cbottom - ctop >= 1.f;
}

// Parse "w=399" into {"w", "399"}, returns false if malformed
inline bool parse_prop_assertion(const std::string &arg, std::string &prop,
                                  std::string &expected) {
  auto eq = arg.find('=');
  if (eq == std::string::npos || eq == 0) return false;
  prop = arg.substr(0, eq);
  expected = arg.substr(eq + 1);
  return true;
}

// Check a single property assertion against an entity.
// Returns empty string on match, error message on mismatch.
inline std::string check_ui_property(const Entity &entity,
                                      const std::string &prop,
                                      const std::string &expected) {
  const auto &cmp = entity.get<ui::UIComponent>();
  RectangleType rect = get_screen_rect(entity);

  long actual_int = 0;
  float actual_float = 0.f;
  bool is_int_prop = true;

  if (prop == "x") { actual_float = rect.x; actual_int = std::lroundf(rect.x); }
  else if (prop == "y") { actual_float = rect.y; actual_int = std::lroundf(rect.y); }
  else if (prop == "w") { actual_float = rect.width; actual_int = std::lroundf(rect.width); }
  else if (prop == "h") { actual_float = rect.height; actual_int = std::lroundf(rect.height); }
  else if (prop == "hidden") {
    std::string actual_str = cmp.should_hide ? "true" : "false";
    if (actual_str != expected)
      return std::format("{}={} but got {}", prop, expected, actual_str);
    return "";
  }
  else if (prop == "text") {
    is_int_prop = false;
    if (!entity.has<ui::HasLabel>())
      return std::format("text={} but element has no label", expected);
    const auto &label = entity.get<ui::HasLabel>().label;
    if (label != expected)
      return std::format("text=\"{}\" but got \"{}\"", expected, label);
    return "";
  }
  else if (prop == "accessible_name" || prop == "role" ||
           prop == "accessible_value" || prop == "accessible_description") {
    is_int_prop = false;
    if (!entity.has<ui::HasAccessibility>())
      return std::format("{}={} but element has no accessibility semantics",
                         prop, expected);
    const auto &a = entity.get<ui::HasAccessibility>();
    std::string actual = prop == "role" ? ui::accessible_role_name(a.role)
                         : prop == "accessible_name" ? a.name
                         : prop == "accessible_value" ? a.value
                                                      : a.description;
    if (actual != expected)
      return std::format("{}=\"{}\" but got \"{}\"", prop, expected, actual);
    return "";
  }
  else if (prop == "scroll_x" || prop == "scroll_y") {
    if (!entity.has<ui::HasScrollView>())
      return std::format("{}={} but element has no scroll view", prop, expected);
    actual_float = prop == "scroll_x"
                       ? entity.get<ui::HasScrollView>().scroll_offset.x
                       : entity.get<ui::HasScrollView>().scroll_offset.y;
    actual_int = std::lroundf(actual_float);
  }
  else if (prop == "bg" || prop == "fg") {
    Color want;
    if (!parse_hex_color(expected, want))
      return std::format("{}={} is not a #rrggbb[aa] colour", prop, expected);
    Color actual;
    if (prop == "bg") {
      if (!entity.has<HasColor>())
        return std::format("bg={} but element has no colour", expected);
      actual = entity.get<HasColor>().color();
    } else {
      if (!entity.has<ui::HasLabel>())
        return std::format("fg={} but element has no label", expected);
      actual = ui::detail::resolve_label_color(
          entity.get<ui::HasLabel>(), ui::imm::ThemeDefaults::get().get_theme());
    }
    // Match what rendering draws: the colour above is the pre-opacity value,
    // and the fill / label paths scale alpha by the effective opacity (own
    // HasOpacity times ancestors'). Hover is not applied: it needs the live
    // pointer state and context theme, which this check cannot see.
    const float opacity = ui::detail::compute_effective_opacity(entity);
    if (opacity < 1.0f) actual = colors::opacity_pct(actual, opacity);
    if (actual.r != want.r || actual.g != want.g || actual.b != want.b ||
        actual.a != want.a)
      return std::format("{}={} but got {}", prop, format_hex_color(want),
                         format_hex_color(actual));
    return "";
  }
  else {
    return std::format("unknown property '{}'", prop);
  }

  if (is_int_prop) {
    if (expected.find_first_of(".eE") != std::string::npos) {
      const float expected_float = std::stof(expected);
      if (std::fabs(actual_float - expected_float) > 0.01f)
        return std::format("{}={} but got {}", prop, expected,
                           format_dim(actual_float));
      return "";
    }
    long expected_int = std::stol(expected);
    if (actual_int != expected_int)
      return std::format("{}={} but got {}", prop, expected_int, actual_int);
  }
  return "";
}

struct HandleAssertUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("assert_ui")) return;
    if (!cmd.has_args(2)) {
      cmd.fail("assert_ui requires: <name> <prop>=<value> [...]");
      return;
    }
    if (render_checkpoint_pending(cmd)) return;

    std::string name = cmd.args[0];
    std::string scope_name, scope_err;
    auto props = split_scope_props(cmd, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "assert_ui", scope_name, true, scope)) return;
    auto res = resolve_ui_single(name, scope, UIMatchKind::DebugName,
                                 UIVisibility::Rendered);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry();  // Element may not be rendered yet
      return;
    }
    Entity &entity = res.entity.asE();
    for (const std::string &p : props) {
      std::string prop, expected;
      if (!parse_prop_assertion(p, prop, expected)) {
        cmd.fail(std::format("assert_ui: malformed assertion '{}'", p));
        return;
      }
      std::string err = check_ui_property(entity, prop, expected);
      if (!err.empty()) {
        cmd.retry_with(std::format("assert_ui '{}': {}", name, err));
        return;
      }
    }
    cmd.consume();
  }
};

struct HandleExpectPointInCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_point_in")) return;
    std::string scope_name, scope_err;
    auto pos = strip_trailing_scope(cmd, 0, 3, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    if (pos.size() < 3) {
      cmd.fail("expect_point_in requires: <name> <x> <y>");
      return;
    }
    float x = 0.f;
    float y = 0.f;
    try {
      auto [sw, sh] = e2e_screen_size();
      x = cmd.coord_arg(1, sw);
      y = cmd.coord_arg(2, sh);
    } catch (...) {
      cmd.fail(std::format("expect_point_in: invalid coordinate '{}' '{}'",
                           pos[1], pos[2]));
      return;
    }
    if (render_checkpoint_pending(cmd)) return;
    EntityID scope = -1;
    if (!scope_or_done(cmd, "expect_point_in", scope_name, true, scope))
      return;
    auto res = resolve_ui_single(pos[0], scope, UIMatchKind::DebugName,
                                 UIVisibility::Rendered);
    if (!res.ambiguous.empty()) { cmd.fail(res.ambiguous); return; }
    if (!res.entity) {
      cmd.retry();
      return;
    }
    Entity &entity = res.entity.asE();
    RectangleType rect = get_screen_rect(entity);
    if (ui::is_mouse_inside(input::MousePosition{x, y}, rect)) {
      cmd.consume();
      return;
    }
    cmd.retry_with(std::format(
        "expect_point_in '{}': point ({:g},{:g}) is outside rect "
        "({:g},{:g} {:g}x{:g})",
        pos[0], x, y, rect.x, rect.y, rect.width, rect.height));
  }
};

struct HandleAssertUITextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("assert_ui_text")) return;
    if (!cmd.has_args(2)) {
      cmd.fail("assert_ui_text requires: \"<text>\" <prop>=<value> [...]");
      return;
    }
    if (render_checkpoint_pending(cmd)) return;

    std::string text = cmd.args[0];
    std::string scope_name, scope_err;
    auto rest = split_scope_props(cmd, 1, scope_name, scope_err);
    if (!scope_err.empty()) { cmd.fail(scope_err); return; }
    bool absent = false;
    std::vector<std::string> props;
    for (const std::string &tok : rest) {
      if (tok == "absent") {
        absent = true;
        continue;
      }
      std::string prop, expected;
      if (!parse_prop_assertion(tok, prop, expected)) {
        cmd.fail(std::format("assert_ui_text: malformed '{}'", tok));
        return;
      }
      props.push_back(tok);
    }
    if (absent && !props.empty()) {
      cmd.fail("assert_ui_text: 'absent' cannot combine with property checks");
      return;
    }
    EntityID scope = -1;
    if (!scope_or_done(cmd, "assert_ui_text", scope_name, true, scope))
      return;
    auto candidates = resolve_ui_candidates(
        text, scope, UIMatchKind::TextExact, UIVisibility::RenderedPartiallyVisible);
    if (absent) {
      if (cmd.frames_alive == 0) {
        cmd.retry();  // Let a frame render; absence before that proves nothing.
        return;
      }
      if (candidates.empty()) {
        cmd.consume();
        return;
      }
      cmd.retry_with(std::format("assert_ui_text \"{}\": {} matching element(s) still present",
                                 text, candidates.size()));
      return;
    }
    if (candidates.empty()) {
      cmd.retry();
      return;
    }
    if (candidates.size() > 1) {
      std::string list;
      for (Entity &e : candidates) {
        if (!list.empty()) list += "; ";
        list += describe_candidate(e);
      }
      cmd.fail(std::format("{} elements match '{}'; scope it with under=<scope> "
                           "(in=<scope> accepted as alias) — candidates: {}",
                           candidates.size(), text, list));
      return;
    }
    Entity &entity = candidates[0].get();
    for (const std::string &tok : props) {
      std::string prop, expected;
      parse_prop_assertion(tok, prop, expected);
      std::string err = check_ui_property(entity, prop, expected);
      if (!err.empty()) {
        cmd.retry_with(std::format("assert_ui_text \"{}\": {}", text, err));
        return;
      }
    }
    cmd.consume();
  }
};

// expect_no_ui <debug_name>: fails if a rendered element has the name.
struct HandleExpectNoUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_no_ui")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_no_ui requires: <debug_name>");
      return;
    }
    if (render_checkpoint_pending(cmd)) return;
    if (cmd.frames_alive == 0) {
      cmd.retry();
      return;
    }
    const std::string name = cmd.args[0];
    auto opt = ui_query()
        .whereHasComponent<ui::UIComponent>()
        .whereHasComponent<ui::UIComponentDebug>()
        .whereLambda([&](const Entity &e) {
          return e.get<ui::UIComponentDebug>().name_value == name &&
                 e.get<ui::UIComponent>().was_rendered_to_screen;
        })
        .gen_first();
    if (opt.has_value()) {
      cmd.retry_with(std::format("expect_no_ui: '{}' still present (expected it to be gone)", name));
      return;
    }
    cmd.consume();
  }
};

// What holds focus and what set it. dump_ui says WHICH widget is focused; it
// could not say WHY, so a screen whose Tab order broke had nothing to read.
template <typename InputAction>
struct HandleDumpFocusCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("dump_focus")) return;
    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("dump_focus: UIContext not found");
      return;
    }
    std::string holder = "nothing";
    if (ctx->focus_id != ctx->ROOT) {
      auto opt = ui::UICollectionHolder::getEntityForID(ctx->focus_id);
      holder = (opt.valid() && opt.asE().template has<ui::UIComponentDebug>())
                   ? opt.asE().template get<ui::UIComponentDebug>().name()
                   : std::format("entity {}", ctx->focus_id);
    }
    log_info("[E2E] dump_focus: '{}' holds focus, set by {}", holder,
             ctx->focus_origin());
    cmd.consume();
  }
};

struct HandleDumpUICommand : System<PendingE2ECommand> {
  using DumpFn = std::function<void(const std::string &, const std::string &)>;

  explicit HandleDumpUICommand(DumpFn fn = nullptr)
      : dump_fn_(std::move(fn)) {}

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("dump_ui")) return;
    if (!cmd.has_args(1)) { cmd.fail("dump_ui requires a name argument"); return; }
    if (render_checkpoint_pending(cmd)) return;

    std::string dump_name = cmd.args[0];
    std::string subtree_root = cmd.has_args(2) ? cmd.args[1] : "";

    std::string xml;
    xml.reserve(8192);

    if (!subtree_root.empty()) {
      auto opt = ui_query()
          .whereHasComponent<ui::UIComponent>()
          .whereHasComponent<ui::UIComponentDebug>()
          .whereLambda([&](const Entity &e) {
            return e.get<ui::UIComponentDebug>().name_value.find(subtree_root)
                   != std::string::npos &&
                   e.get<ui::UIComponent>().was_rendered_to_screen;
          })
          .gen_first();
      if (opt.has_value()) {
        dump_ui_node(xml, opt.asE(), 0);
      }
    } else {
      for (Entity &e : ui_query()
          .whereHasComponent<ui::UIComponent>()
          .whereLambda([&](const Entity &e) {
            return e.get<ui::UIComponent>().parent == -1;
          })
          .gen()) {
        auto &cmp = e.get<ui::UIComponent>();
        if (cmp.was_rendered_to_screen) {
          dump_ui_node(xml, e, 0);
        } else {
          // Root is a container (e.g., UIContext entity); dump its rendered
          // children directly
          for (EntityID child_id : cmp.children) {
            OptEntity opt = ui::UICollectionHolder::getEntityForID(child_id);
            if (!opt.valid()) continue;
            Entity &child = opt.asE();
            if (!child.has<ui::UIComponent>()) continue;
            if (!child.get<ui::UIComponent>().was_rendered_to_screen) continue;
            dump_ui_node(xml, child, 0);
          }
        }
      }
    }

    // No sink means log it: a dump that goes nowhere reads as a broken command.
    if (dump_fn_) dump_fn_(dump_name, xml);
    else log_info("[E2E] dump_ui '{}':\n{}", dump_name, xml);
    cmd.consume();
  }

private:
  DumpFn dump_fn_;
};

// Register all UI command handlers.
// dump_fn receives (name, xml) for `dump_ui`; without one the dump is logged.
template <typename InputAction>
void register_ui_commands(SystemManager &sm,
                          HandleDumpUICommand::DumpFn dump_fn = nullptr) {
  // Semantic actions (preferred - works with your InputAction enum)
  sm.register_update_system(
      std::make_unique<HandleActionCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleHoldActionCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleReleaseActionCommand<InputAction>>());

  // Component interactions (auto-find by name/text)
  sm.register_update_system(
      std::make_unique<HandleClickUICommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleRightClickUICommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleRightClickTextCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleClickTextCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleClickButtonCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleFocusUICommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleToggleCheckboxCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleSetSliderCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleSelectDropdownCommand<InputAction>>());

  // Raw key fallbacks (for edge cases)
  sm.register_update_system(std::make_unique<HandleTabCommand>());
  sm.register_update_system(std::make_unique<HandleShiftTabCommand>());
  sm.register_update_system(std::make_unique<HandleShiftTabRelease>());
  sm.register_update_system(std::make_unique<HandleEnterCommand>());
  sm.register_update_system(std::make_unique<HandleEscapeCommand>());
  sm.register_update_system(std::make_unique<HandleArrowCommand>());

  // Assertions
  sm.register_update_system(
      std::make_unique<HandleExpectFocusedCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectCheckboxCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectSliderCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectAccessibleCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectInputTextCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectInputSelectionCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectSelectedTextCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectClipboardCommand>());
  sm.register_update_system(
      std::make_unique<HandleExpectNoFocusCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectHoveredCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleClickLabelCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleMultiClickUICommand<InputAction, 2>>());
  sm.register_update_system(
      std::make_unique<HandleMultiClickUICommand<InputAction, 3>>());
  sm.register_update_system(std::make_unique<HandlePointerUICommand>());

  // UI property assertions
  sm.register_update_system(std::make_unique<HandleAssertUICommand>());
  sm.register_update_system(std::make_unique<HandleAssertUITextCommand>());
  sm.register_update_system(std::make_unique<HandleExpectPointInCommand>());
  sm.register_update_system(std::make_unique<HandleExpectNoUICommand>());

  // Diagnostics
  sm.register_update_system(
      std::make_unique<HandleDumpFocusCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleDumpUICommand>(std::move(dump_fn)));

  // Assertions over what was drawn. Registered here, in the function every
  // consumer already calls, because dump_ui shipped for months in a file
  // nobody registered and read as a typo when a script asked for it.
  capture::enable();
  sm.register_update_system(
      std::make_unique<draw_commands::HandleExpectDrawnCommand>());
  sm.register_update_system(
      std::make_unique<draw_commands::HandleExpectNotDrawnCommand>());
  sm.register_update_system(
      std::make_unique<draw_commands::HandleExpectDrawnAtCommand>());
  sm.register_update_system(
      std::make_unique<draw_commands::HandleDumpDrawsCommand>());
  sm.register_update_system(
      std::make_unique<draw_commands::HandleExpectDrawCallsBelowCommand>());
}

// All command packs in one call, in registration order.
template <typename InputAction>
void register_full_pack(SystemManager &sm,
                        HandleDumpUICommand::DumpFn dump_fn = nullptr) {
  register_builtin_handlers(sm);
  register_ui_commands<InputAction>(sm, std::move(dump_fn));
  perf_commands::register_perf_commands(sm);
  register_unknown_handler(sm);
  register_cleanup(sm);
}

} // namespace ui_commands
} // namespace testing
} // namespace afterhours
