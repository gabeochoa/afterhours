#pragma once

// Focus return that survives rebuilt controls and closed scopes.
//
// Modal::previously_focused_element and HasMenuState::restore_focus hold a
// raw entity id. An immediate-mode screen that rebuilds its invoker under a
// new id, or closes the panel the invoker lived in, leaves that id pointing
// at nothing -- or at whatever reused it. A FocusToken names the control
// logically instead: its UI collection, the scope that owns it, and a
// control id within that scope (both from a FocusKey the app attaches).
//
//   button.ent().addComponentIfMissing<FocusKey>("inventory", "slot:12");
//   returns.opened(ctx, popover.id, FocusToken{.scope = "inventory",
//                                              .control = "grid"});
//   ...
//   returns.closed(ctx, popover.id);       // when it closes
//   returns.resolve(ctx);                  // each frame, after layout
//
// resolve() restores the token only when its owning scope still exists and
// the control resolves to an entity that can take focus -- a wrapper such as
// a text input hands focus to its first focusable descendant. Otherwise the
// explicit fallback, otherwise nothing. Overlays nest as a stack. A close
// caused by clicking elsewhere does not steal focus back from what was
// clicked.

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "../../ecs.h"
#include "systems.h"

namespace afterhours {
namespace ui {

/// A control's logical identity: the scope that owns it and its id within
/// that scope. An entity with an empty control is the scope's own
/// destination, e.g. a list region whose focusable child varies.
struct FocusKey : BaseComponent {
  std::string scope;
  std::string control;
  FocusKey() = default;
  FocusKey(std::string scope_, std::string control_ = {})
      : scope(std::move(scope_)), control(std::move(control_)) {}
};

struct FocusToken {
  const EntityCollection *collection = nullptr; // null: the UI collection
  std::string scope;
  std::string control;
  // What held focus when captured, used only for a token with no scope.
  EntityID entity = -1;

  [[nodiscard]] bool empty() const { return scope.empty() && entity < 0; }
};

enum struct FocusReturnResult {
  Nothing,     // no overlay closed since the last resolve
  Restored,    // focus is back on the token's control
  FellBack,    // the token was gone or ineligible; the fallback has focus
  Kept,        // focus had already moved outside the overlay (outside click)
  Unavailable, // neither the token nor the fallback resolved
};

/// The token for whatever holds focus: the nearest FocusKey at or above it,
/// or the raw entity when nothing names it.
template <typename InputAction>
FocusToken capture_focus(UIContext<InputAction> &ctx) {
  FocusToken token{&UICollectionHolder::get().collection, {}, {}, ctx.focus_id};
  EntityID current = ctx.focus_id;
  for (int depth = 0; current >= 0 && depth < 64; depth++) {
    OptEntity opt = UICollectionHolder::getEntityForID(current);
    if (!opt.valid())
      break;
    if (opt.asE().has<FocusKey>()) {
      token.scope = opt.asE().get<FocusKey>().scope;
      token.control = opt.asE().get<FocusKey>().control;
      return token;
    }
    if (!opt.asE().has<UIComponent>())
      break;
    current = opt.asE().get<UIComponent>().parent;
  }
  return token;
}

struct FocusReturnStack : BaseComponent {
  struct Entry {
    EntityID overlay = -1;
    FocusToken target;
    FocusToken fallback;
  };
  struct Closing {
    Entry entry;
    EntityID focus_at_close = -1;
    bool focus_was_inside = false;
  };
  std::vector<Entry> open;
  std::vector<Closing> closing;
  // Host overrides. resolver finds a control the FocusKey scan cannot (-1
  // when it has nothing); owner_alive decides whether a scope still exists.
  std::function<EntityID(const FocusToken &)> resolver;
  std::function<bool(std::string_view scope)> owner_alive;

  /// An overlay opened: remember what had focus, and where to go instead.
  template <typename InputAction>
  void opened(UIContext<InputAction> &ctx, EntityID overlay, FocusToken fallback = {}) {
    if (std::none_of(open.begin(), open.end(),
                     [&](const Entry &e) { return e.overlay == overlay; }))
      open.push_back({overlay, capture_focus(ctx), std::move(fallback)});
  }

  /// An overlay closed. Any overlays opened from inside it close with it.
  template <typename InputAction>
  void closed(UIContext<InputAction> &ctx, EntityID overlay) {
    const auto at = std::find_if(open.begin(), open.end(),
                                 [&](const Entry &e) { return e.overlay == overlay; });
    if (at == open.end())
      return;
    const bool inside =
        ctx.focus_id == ctx.ROOT || ctx.contains_in_subtree(overlay, ctx.focus_id);
    closing.push_back({*at, ctx.focus_id, inside});
    open.erase(at, open.end());
  }

  /// Apply closes, once rebuilt controls are laid out (after the UI
  /// post-layout systems and click handling). Returns the last outcome.
  template <typename InputAction>
  FocusReturnResult resolve(UIContext<InputAction> &ctx) {
    FocusReturnResult result = FocusReturnResult::Nothing;
    for (const auto &c : closing) {
      const EntityID overlay = c.entry.overlay;
      // Something outside took focus -- the click that dismissed it.
      if (!(ctx.focus_id == ctx.ROOT || ctx.contains_in_subtree(overlay, ctx.focus_id) ||
            (c.focus_was_inside && ctx.focus_id == c.focus_at_close))) {
        result = FocusReturnResult::Kept;
        continue;
      }
      EntityID to = destination(ctx, c.entry.target);
      result = FocusReturnResult::Restored;
      if (to < 0) {
        to = destination(ctx, c.entry.fallback);
        result = FocusReturnResult::FellBack;
      }
      if (to < 0) {
        result = FocusReturnResult::Unavailable;
        continue;
      }
      ctx.set_focus(to, ctx.focus_modality);
    }
    closing.clear();
    return result;
  }

  /// Where a token would put focus now, or -1.
  template <typename InputAction>
  EntityID destination(UIContext<InputAction> &ctx, const FocusToken &token) const {
    auto &ui = UICollectionHolder::get().collection;
    if (token.empty() || (token.collection && token.collection != &ui))
      return -1;
    const auto rendered = [](const Entity &e) {
      return e.has<UIComponent>() && e.get<UIComponent>().was_rendered_to_screen &&
             !e.get<UIComponent>().should_hide;
    };
    EntityID found = -1;
    if (token.scope.empty()) {
      found = token.entity;
    } else {
      bool owner = false;
      if (resolver)
        found = resolver(token);
      for (Entity &e : EntityQuery(ui, {.ignore_temp_warning = true})
                           .whereHasComponent<FocusKey>()
                           .gen()) {
        const auto &key = e.get<FocusKey>();
        if (key.scope != token.scope || !rendered(e))
          continue;
        owner = true;
        if (found < 0 && key.control == token.control)
          found = e.id;
      }
      if (owner_alive)
        owner = owner_alive(token.scope);
      if (!owner)
        return -1;
    }
    return eligible(ctx, found, 0);
  }

private:
  // The entity itself if it can take focus, else its first descendant that
  // can: a composite control's inner field, a region's focusable child.
  template <typename InputAction>
  static EntityID eligible(UIContext<InputAction> &ctx, EntityID id, int depth) {
    OptEntity opt = UICollectionHolder::getEntityForID(id);
    if (id < 0 || depth > 16 || !opt.valid() || !opt.asE().has<UIComponent>())
      return -1;
    if (can_be_focused(ctx, opt.asE()))
      return id;
    for (const EntityID child : opt.asE().get<UIComponent>().children)
      if (const EntityID hit = eligible(ctx, child, depth + 1); hit >= 0)
        return hit;
    return -1;
  }
};

} // namespace ui
} // namespace afterhours
