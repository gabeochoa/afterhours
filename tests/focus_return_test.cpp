// focus_return_test.cpp
// Closing an overlay returns focus to the control that opened it, named
// logically so a rebuilt control still counts, only while its owner exists
// and it can take focus, through nested overlays, and never over a control
// the user just clicked.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/focus_return.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
struct Screen {
  ImmTestHarness h;
  FocusReturnStack returns;
  bool panel = true, dialog = false, nested = false, wrapped = false;
  int invoker_slot = 0;
  std::function<void()> extra; // more of the screen, built after the rest
  EntityID invoker = -1, search = -1, dialog_root = -1, ok = -1, nested_root = -1,
           nested_button = -1;

  struct Key { std::string scope; std::string control = {}; };
  EntityID box(Entity &parent, int slot, Key key, bool focusable) {
    auto e = div(h.context(), mk(parent, slot), ComponentConfig{}.with_size({pixels(40), pixels(20)}));
    if (!key.scope.empty()) e.ent().addComponentIfMissing<FocusKey>(key.scope, key.control);
    if (focusable) e.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
    return e.ent().id;
  }
  Entity &ent(EntityID id) { return UICollectionHolder::getEntityForIDEnforce(id); }

  void frame() {
    auto &root = h.root();
    h.begin_frame();
    if (panel) {
      const EntityID p = box(root, 0, {"panel"}, false);
      if (wrapped) {
        // A composite control: the named wrapper cannot take focus, its
        // second child can; the first is decoration.
        invoker = box(ent(p), invoker_slot, {"panel", "open"}, false);
        box(ent(invoker), 0, {}, false);
        box(ent(invoker), 1, {}, true);
      } else {
        invoker = box(ent(p), invoker_slot, {"panel", "open"}, true);
      }
    }
    const EntityID toolbar = box(root, 1, {"toolbar"}, false);
    search = box(ent(toolbar), 0, {"toolbar", "search"}, true);
    if (dialog) {
      dialog_root = box(root, 2, {"dialog"}, false);
      ok = box(ent(dialog_root), 0, {"dialog", "ok"}, true);
      if (nested) {
        nested_root = box(ent(dialog_root), 1, {"confirm"}, false);
        nested_button = box(ent(nested_root), 0, {"confirm", "yes"}, true);
      }
    }
    if (extra) extra();
    h.layout_only();
    // Only what this frame built counts as on screen, as
    // TrackIfComponentWillBeRendered would mark it.
    for (const auto &e : h.coll.get_entities())
      if (e && e->has<UIComponent>()) e->get<UIComponent>().was_rendered_to_screen = false;
    const std::function<void(EntityID)> mark = [&](EntityID id) {
      ent(id).get<UIComponent>().was_rendered_to_screen = true;
      for (const EntityID child : ent(id).get<UIComponent>().children) mark(child);
    };
    mark(root.id);
  }
  UIContext<ui_test::TestInputAction> &ctx() { return h.context(); }
  FocusReturnResult close(EntityID overlay) {
    returns.closed(ctx(), overlay);
    frame();
    return returns.resolve(ctx());
  }
};
} // namespace

TEST(a_rebuilt_invoker_gets_focus_back) {
  for (const bool wrapped : {false, true}) {
    Screen s;
    s.wrapped = wrapped;
    s.frame();
    const EntityID old = s.invoker;
    s.ctx().set_focus(wrapped ? s.ent(old).get<UIComponent>().children[1] : old);
    s.dialog = true;
    s.frame();
    s.returns.opened(s.ctx(), s.dialog_root);
    s.ctx().set_focus(s.ok);
    s.dialog = false;
    s.invoker_slot = 7; // the panel rebuilt its button under a new entity
    const EntityID dialog = s.dialog_root;
    CHECK(s.close(dialog) == FocusReturnResult::Restored);
    CHECK(s.invoker != old);
    const EntityID expected = wrapped ? s.ent(s.invoker).get<UIComponent>().children[1] : s.invoker;
    CHECK(s.ctx().focus_id == expected);
  }
}

TEST(a_closed_owner_falls_back_explicitly_or_not_at_all) {
  for (const bool with_fallback : {true, false}) {
    Screen s;
    s.frame();
    s.ctx().set_focus(s.invoker);
    s.dialog = true;
    s.frame();
    s.returns.opened(s.ctx(), s.dialog_root,
                     with_fallback ? FocusToken{nullptr, "toolbar", "search"} : FocusToken{});
    s.ctx().set_focus(s.ok);
    s.dialog = false;
    s.panel = false; // the action closed the panel that owned the invoker
    const EntityID ok = s.ok;
    const auto result = s.close(s.dialog_root);
    if (with_fallback) {
      CHECK(result == FocusReturnResult::FellBack);
      CHECK(s.ctx().focus_id == s.search);
    } else {
      CHECK(result == FocusReturnResult::Unavailable);
      CHECK(s.ctx().focus_id == ok); // left for the framework to drop
    }
  }
}

TEST(an_ineligible_invoker_is_not_focused) {
  Screen s;
  s.frame();
  s.ctx().set_focus(s.invoker);
  s.dialog = true;
  s.frame();
  s.returns.opened(s.ctx(), s.dialog_root, {nullptr, "toolbar", "search"});
  s.ctx().set_focus(s.ok);
  s.dialog = false;
  // Still on screen, but something else owns input now.
  s.ctx().add_input_gate("busy", [&](EntityID id) { return id != s.invoker; });
  CHECK(s.close(s.dialog_root) == FocusReturnResult::FellBack);
  CHECK(s.ctx().focus_id == s.search);
}

TEST(nested_overlays_return_one_level_at_a_time) {
  Screen s;
  s.frame();
  s.ctx().set_focus(s.invoker);
  s.dialog = true;
  s.frame();
  s.returns.opened(s.ctx(), s.dialog_root);
  s.ctx().set_focus(s.ok);
  s.nested = true;
  s.frame();
  s.returns.opened(s.ctx(), s.nested_root);
  s.ctx().set_focus(s.nested_button);

  s.nested = false;
  CHECK(s.close(s.nested_root) == FocusReturnResult::Restored);
  CHECK(s.ctx().focus_id == s.ok);
  s.dialog = false;
  CHECK(s.close(s.dialog_root) == FocusReturnResult::Restored);
  CHECK(s.ctx().focus_id == s.invoker);

  // Closing the outer one closes the inner with it, back to the invoker.
  s.dialog = s.nested = true;
  s.frame();
  s.returns.opened(s.ctx(), s.dialog_root);
  s.ctx().set_focus(s.ok);
  s.returns.opened(s.ctx(), s.nested_root);
  s.ctx().set_focus(s.nested_button);
  s.dialog = s.nested = false;
  CHECK(s.close(s.dialog_root) == FocusReturnResult::Restored);
  CHECK(s.ctx().focus_id == s.invoker);
  CHECK(s.returns.open.empty());
}

TEST(dismissal_by_an_outside_click_keeps_the_click) {
  Screen s;
  s.frame();
  s.ctx().set_focus(s.invoker);
  s.dialog = true;
  s.frame();
  s.returns.opened(s.ctx(), s.dialog_root);
  s.ctx().set_focus(s.ok);
  // The click lands on the toolbar, which dismisses the overlay -- in either
  // order relative to the close.
  for (const bool click_first : {true, false}) {
    if (click_first) s.ctx().set_focus(s.search);
    s.returns.closed(s.ctx(), s.dialog_root);
    if (!click_first) s.ctx().set_focus(s.search);
    s.frame();
    CHECK(s.returns.resolve(s.ctx()) == FocusReturnResult::Kept);
    CHECK(s.ctx().focus_id == s.search);
    s.ctx().set_focus(s.ok);
    s.returns.opened(s.ctx(), s.dialog_root);
    s.returns.open.back().target = capture_focus(s.ctx());
  }
}

TEST(a_host_resolver_finds_unnamed_rebuilt_controls) {
  Screen s;
  s.frame();
  EntityID dynamic = -1;
  s.returns.resolver = [&](const FocusToken &t) { return t.control == "row:9" ? dynamic : -1; };
  s.dialog = true;
  s.frame();
  s.returns.opened(s.ctx(), s.dialog_root);
  s.returns.open.back().target = {nullptr, "toolbar", "row:9"};
  s.ctx().set_focus(s.ok);
  s.dialog = false;
  s.extra = [&] { dynamic = s.box(s.h.root(), 9, {}, true); };
  CHECK(s.close(s.dialog_root) == FocusReturnResult::Restored);
  CHECK(s.ctx().focus_id == dynamic);
  // A token from another collection never resolves here.
  EntityCollection other;
  CHECK(s.returns.destination(s.ctx(), {&other, "toolbar", "search"}) == -1);
  CHECK(s.returns.destination(s.ctx(), {nullptr, "toolbar", "search"}) == s.search);
}

int main() { return ui_test::run_registered_tests("focus_return"); }
