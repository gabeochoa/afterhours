#include "ui_test_harness.h"

#include <array>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

struct PaginationFixture {
  ImmTestHarness h;
  std::array<std::string_view, 5> options{"1", "2", "3", "4", "5"};
  size_t index = 2;
  Entity *self = nullptr;

  ElementResult build() {
    h.begin_frame();
    auto result = pagination(h.context(), mk(h.root(), 0), options, index,
                             ComponentConfig{}.with_size(
                                 ComponentSize{pixels(400), pixels(40)}));
    h.layout_only();
    self = &result.ent();
    return result;
  }

  EntityID child_id(size_t pos) {
    return self->get<UIComponent>().children[pos];
  }

  bool click_child(size_t pos) {
    Entity &target =
        UICollectionHolder::getEntityForID(child_id(pos)).asE();
    target.get<HasClickListener>().down = true;
    bool changed = static_cast<bool>(build());
    target.get<HasClickListener>().down = false;
    return changed;
  }
};

} // namespace

TEST(pagination_next_walks_forward_and_wraps) {
  PaginationFixture p;
  CHECK(!p.build());
  CHECK(p.index == 2);
  CHECK(p.click_child(6));
  CHECK(p.index == 3);
  CHECK(p.click_child(6));
  CHECK(p.index == 4);
  CHECK(p.click_child(6));
  CHECK(p.index == 0);
  CHECK(!p.build());
  CHECK(p.index == 0);
}

TEST(pagination_previous_walks_backward_and_wraps) {
  PaginationFixture p;
  CHECK(!p.build());
  CHECK(p.index == 2);
  CHECK(p.click_child(0));
  CHECK(p.index == 1);
  CHECK(p.click_child(0));
  CHECK(p.index == 0);
  CHECK(p.click_child(0));
  CHECK(p.index == 4);
  CHECK(p.click_child(0));
  CHECK(p.index == 3);
  CHECK(!p.build());
  CHECK(p.index == 3);
}

TEST(pagination_page_buttons_select_their_own_page) {
  PaginationFixture p;
  CHECK(!p.build());
  CHECK(p.index == 2);
  CHECK(p.click_child(4));
  CHECK(p.index == 3);
  CHECK(p.h.context().has_focus(p.child_id(4)));
  CHECK(p.click_child(1));
  CHECK(p.index == 0);
  CHECK(p.h.context().has_focus(p.child_id(1)));
  CHECK(p.click_child(5));
  CHECK(p.index == 4);
  CHECK(p.h.context().has_focus(p.child_id(5)));
  CHECK(p.click_child(3));
  CHECK(p.index == 2);
  CHECK(p.h.context().has_focus(p.child_id(3)));
  CHECK(p.click_child(2));
  CHECK(p.index == 1);
  CHECK(p.h.context().has_focus(p.child_id(2)));
  CHECK(!p.build());
  CHECK(p.index == 1);
}

int main() { return ui_test::run_registered_tests("pagination tests"); }
