// escape_owner_test.cpp
// R-019: one Escape press has exactly one owner. Stacked layers are
// menu > modal > field (field lives inside the modal): each press peels
// the topmost layer only -- menu first, then the field blur, then the
// modal. The modal watcher is an update system and runs before the imm
// build that owns menus/dropdowns/fields, so it must defer to focus
// inside a temporary layer instead of also acting on the same press.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/modal.h>
#include <afterhours/src/plugins/ui/menu.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

void ensure_modal_singleton() {
  if (!EntityHelper::has_singleton<modal::ModalRoot>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    modal::detail::init_singleton(e);
  }
}

struct Fixture {
  ImmTestHarness h;
  modal::ModalCloseWatcherSystem<ui_test::TestInputAction> watcher;
  bool modal_open = true;
  bool menu_open = true;
  std::string text;

  Fixture() {
    ensure_modal_singleton();
    modal::detail::get_modal_root().modal_stack.clear();
    EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(
        h.context_entity());
  }

  // Modal containing a text field, with a context menu built inside the
  // modal panel (menu over modal, field inside modal).
  EntityID panel_id = -1;
  EntityID menu_panel_id = -1;
  EntityID field_id = -1;

  void emit() {
    auto panel = afterhours::modal(
        h.context(), mk(h.root(), 0), modal_open,
        ModalConfig{}.with_size(pixels(480), pixels(320))
            .with_show_close_button(false));
    panel_id = panel.ent().id;
    auto field = afterhours::ui::imm::text_input(h.context(), mk(panel.ent(), 0), text,
        ComponentConfig{}.with_size({pixels(300), pixels(40)}));
    field_id = -1;
    for (auto cid : field.cmp().children) {
      auto found = UICollectionHolder::getEntityForID(cid);
      if (found && found.asE().has<UIComponentDebug>() &&
          found.asE().get<UIComponentDebug>().name() == "text_input_field")
        field_id = cid;
    }
    menu_panel_id = -1;
    const std::vector<MenuItem> items = {MenuItem{"Open", "", false, false},
                                         MenuItem{"Save", "", false, false}};
    context_menu(h.context(), mk(panel.ent(), 1), items, Vector2Type{100, 200},
                 menu_open, ComponentConfig{}.with_size(
                                ComponentSize{pixels(180), pixels(28)}));
    // The menu state knows its panel; capture it for focus control.
    for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
      if (e && e->has<HasMenuState>())
        menu_panel_id = e->get<HasMenuState>().panel;
    }
  }

  // One frame, real-system order: update systems first, then imm build.
  void frame() {
    h.begin_frame();
    watcher.once(0);
    emit();
    h.layout_only();
  }

  ~Fixture() {
    EntityHelper::get_default_collection().singletonMap.erase(
        components::get_type_id<UIContext<ui_test::TestInputAction>>());
  }
};

} // namespace

// Stacked: menu over modal over field. Each Escape press closes exactly
// the topmost layer, in peel order.
TEST(escape_peels_menu_then_field_then_modal) {
  Fixture f;
  f.frame();
  f.frame();  // settle: modal + menu open (menu records its panel frame 2)
  CHECK(f.modal_open);
  CHECK(f.menu_open);
  CHECK(f.field_id >= 0);
  CHECK(f.menu_panel_id >= 0);

  // Press 1: focus inside the menu panel -> menu closes, modal + field stay.
  f.h.context().set_focus(f.menu_panel_id);
  f.h.context().last_action = ui_test::TestInputAction::MenuBack;
  f.frame();
  CHECK(!f.menu_open);
  CHECK(f.modal_open);
  CHECK(f.h.context().focus_id != f.h.context().FAKE);  // no blur happened

  // Press 2: focus inside the field -> field blurs, modal stays.
  f.h.context().set_focus(f.field_id);
  f.h.context().last_action = ui_test::TestInputAction::MenuBack;
  f.frame();
  CHECK(f.modal_open);  // one press, one layer: modal still open
  CHECK(f.h.context().focus_id == f.h.context().FAKE);
  CHECK(!f.h.context().pressed(ui_test::TestInputAction::MenuBack));

  // Press 3: focus outside any temporary layer -> modal closes.
  f.h.context().set_focus(f.h.root().id);
  f.h.context().last_action = ui_test::TestInputAction::MenuBack;
  f.frame();
  CHECK(!f.modal_open);
}

// With no menu, focus inside the field: Escape blurs only, the same
// press must not also close the modal beneath it.
TEST(escape_on_field_does_not_close_modal_same_press) {
  Fixture f;
  f.menu_open = false;
  f.frame();
  f.h.context().set_focus(f.field_id);
  f.h.context().last_action = ui_test::TestInputAction::MenuBack;
  f.frame();
  CHECK(f.modal_open);
  CHECK(f.h.context().focus_id == f.h.context().FAKE);
}

// With no temporary layer at all, Escape closes the modal as before.
TEST(escape_closes_modal_when_no_inner_layer) {
  Fixture f;
  f.menu_open = false;
  f.frame();
  f.h.context().set_focus(f.h.root().id);
  f.h.context().last_action = ui_test::TestInputAction::MenuBack;
  f.frame();
  CHECK(!f.modal_open);
  CHECK(!f.h.context().pressed(ui_test::TestInputAction::MenuBack));
}

int main() { return ui_test::run_registered_tests("escape_owner"); }
