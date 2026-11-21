#include <cstdio>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/e2e_testing/test_input.h>
#include <afterhours/src/plugins/ui/list_menu.h>

using namespace afterhours;
using list_menu::Input;
using list_menu::State;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static Input key(bool Input::*f) { Input i; i.*f = true; return i; }

int main() {
  {
    State s;
    auto r = list_menu::update_list_menu(s, 3, Input{});
    check(s.selected == 0 && !r.moved, "initial selection is first row");
    r = list_menu::update_list_menu(s, 3, key(&Input::down));
    check(s.selected == 1 && r.moved, "down moves to next row");
    list_menu::update_list_menu(s, 3, key(&Input::up));
    check(s.selected == 0, "up moves back");
  }
  {
    State s; s.selected = 2;
    list_menu::update_list_menu(s, 3, key(&Input::down));
    check(s.selected == 0, "down wraps past last row");
    list_menu::update_list_menu(s, 3, key(&Input::up));
    check(s.selected == 2, "up wraps past first row");
    s.wrap = false;
    list_menu::update_list_menu(s, 3, key(&Input::down));
    check(s.selected == 2, "clamp: down at last row stays");
    s.selected = 0;
    list_menu::update_list_menu(s, 3, key(&Input::up));
    check(s.selected == 0, "clamp: up at first row stays");
  }
  {
    auto enabled = [](int i) { return i != 1 && i != 3; };
    State s;
    list_menu::update_list_menu(s, 5, key(&Input::down), enabled);
    check(s.selected == 2, "down skips disabled row 1");
    list_menu::update_list_menu(s, 5, key(&Input::down), enabled);
    check(s.selected == 4, "down skips disabled row 3");
    list_menu::update_list_menu(s, 5, key(&Input::down), enabled);
    check(s.selected == 0, "wrap skips disabled rows");
    list_menu::update_list_menu(s, 5, key(&Input::end), enabled);
    check(s.selected == 4, "end selects last enabled row");
    s.selected = 1;
    list_menu::update_list_menu(s, 5, Input{}, enabled);
    check(s.selected == 0, "selection on disabled row resets to first enabled");
    State none;
    auto r = list_menu::update_list_menu(none, 3, key(&Input::down),
                                          [](int) { return false; });
    check(none.selected == -1 && !r.activated, "all disabled: no selection");
  }
  {
    State s; s.page_size = 3;
    list_menu::update_list_menu(s, 10, key(&Input::page_down));
    check(s.selected == 3, "page down moves page_size rows");
    list_menu::update_list_menu(s, 10, key(&Input::page_down));
    check(s.selected == 6, "page down again");
    list_menu::update_list_menu(s, 10, key(&Input::page_up));
    check(s.selected == 3, "page up moves back");
    list_menu::update_list_menu(s, 10, key(&Input::end));
    check(s.selected == 9, "end selects last row");
    list_menu::update_list_menu(s, 10, key(&Input::home));
    check(s.selected == 0, "home selects first row");
    s.wrap = false; s.selected = 8;
    list_menu::update_list_menu(s, 10, key(&Input::page_down));
    check(s.selected == 9, "clamped page down stops at last row");
  }
  {
    State s;
    auto r = list_menu::update_list_menu(s, 3, key(&Input::activate));
    check(r.activated && s.selected == 0, "activate fires on selected row");
    r = list_menu::update_list_menu(s, 0, key(&Input::activate));
    check(!r.activated && s.selected == -1, "empty list: no activation");
    r = list_menu::update_list_menu(s, 3, Input{});
    check(!r.activated, "no activate input: no activation");
  }
  {
    State s; s.selected = 4;
    list_menu::update_list_menu(s, 2, Input{});
    check(s.selected == 0, "shrunk list resets selection to first row");
  }
  {
    State s;
    testing::test_input::reset_all();
    testing::input_injector::set_key_down(keys::DOWN);
    testing::input_injector::reset_frame();
    list_menu::update_list_menu(s, 3);
    testing::test_input::reset_all();
    check(s.selected == 1, "key overload: Down moves selection");
    testing::input_injector::set_key_down(keys::ENTER);
    testing::input_injector::reset_frame();
    auto r = list_menu::update_list_menu(s, 3);
    testing::test_input::reset_all();
    check(r.activated && s.selected == 1, "key overload: Enter activates");
  }

  printf("list_menu_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
