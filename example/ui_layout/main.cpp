

#include "../shared/vector.h"
#include <iostream>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <math.h>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stack>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/autolayout.h"
#include "../../src/plugins/ui.h"

#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"
#include "../../vendor/trompeloeil/trompeloeil.hpp"

using namespace afterhours;
using namespace afterhours::ui;

static UIComponent &make_component(Entity &entity) {
  auto &cmp = entity.addComponent<ui::UIComponent>(entity.id);
  EntityHelper::merge_entity_arrays();
  return cmp;
}

auto &to_cmp(Entity &entity) { return AutoLayout::to_cmp_static(entity.id); }
auto to_rect(Entity &entity) { return to_cmp(entity).rect(); }
auto to_bounds(Entity &entity) { return to_cmp(entity).bounds(); }

void run_(Entity &root_element) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }
  ui::AutoLayout::autolayout(root_element.get<UIComponent>(), {1280, 720},
                             components);
}

void print_tree(Entity &root_ent, ui::UIComponent &root) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }
  ui::AutoLayout::autolayout(root, {1280, 720}, components);
  ui::print_debug_autolayout_tree(root_ent, root);
}

std::string to_string(const RectangleType &rect) {
  std::ostringstream ss;
  ss << "Rect (";
  ss << rect.x;
  ss << ", ";
  ss << rect.y;
  ss << ") ";
  ss << rect.width;
  ss << "x";
  ss << rect.height;
  return ss.str();
}

std::ostream &operator<<(std::ostream &os, const RectangleType &data) {
  os << to_string(data);
  os << std::endl;
  return os;
}

bool compareRect(RectangleType a, RectangleType b) {
  const auto close = [](float a, float b) { return std::abs(b - a) < 0.001f; };
  if (!close(a.x, b.x)) {
    // std::cout << "x" << std::endl;
    return false;
  }
  if (!close(a.y, b.y)) {
    // std::cout << "y" << std::endl;
    return false;
  }
  if (!close(a.width, b.width)) {
    // std::cout << "w" << std::endl;
    return false;
  }
  if (!close(a.height, b.height)) {
    // std::cout << "h" << std::endl;
    return false;
  }
  return true;
}

void expect(bool b, const std::string &msg, Entity &root) {
  if (!b) {
    run_(root);
    print_tree(root, root.get<ui::UIComponent>());
    std::cout << msg << std::endl;
  }
  assert(b);
}

struct RectMatcher : public Catch::MatcherBase<RectangleType> {
  RectangleType a;
  RectMatcher(RectangleType a_) : a(a_) {}

  bool match(RectangleType const &in) const override {
    return (in.x == a.x &&         //
            in.y == a.y &&         //
            in.width == a.width && //
            in.height == a.height);
  }

  std::string describe() const override {
    std::ostringstream ss;
    Entity &ent =
        EntityQuery().whereHasComponent<AutoLayoutRoot>().gen_first_enforce();

    std::cout << "=====" << std::endl;
    print_tree(ent, to_cmp(ent));
    std::cout << "=====" << std::endl;

    ss << "matches " << to_string(a);
    return ss.str();
  }
};

auto &make_sophie() {
  auto &sophie = EntityHelper::createEntity();
  {
    sophie.addComponent<ui::AutoLayoutRoot>();
    make_component(sophie)
        // TODO figure out how to update this
        // when resolution changes
        .set_desired_width(pixels(1280.f))
        .set_desired_height(pixels(720.f));
  }
  return sophie;
}

TEST_CASE("root test") {
  auto &sophie = make_sophie();
  run_(sophie);
  CHECK_THAT(sophie.get<ui::UIComponent>().rect(), RectMatcher(Rectangle{
                                                       //
                                                       .x = 0,
                                                       .y = 0,
                                                       .width = 1280,
                                                       .height = 720 //
                                                   }));
}

TEST_CASE("default test") {
  auto &sophie = make_sophie();

  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_parent(sophie);
  }

  auto &div = EntityHelper::createEntity();
  {
    make_component(div)
        .set_desired_width(pixels(100.f))
        .set_desired_height(percent(0.5f))
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(div.get<ui::UIComponent>().rect(), RectMatcher(Rectangle{
                                                    //
                                                    .x = 0,
                                                    .y = 50,
                                                    .width = 100,
                                                    .height = 360 //
                                                }));
}

TEST_CASE("top padding") {
  auto &sophie = make_sophie();

  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_padding(pixels(10.f), Axis::top)
        .set_parent(sophie);
  }

  auto &div = EntityHelper::createEntity();
  {
    make_component(div)
        .set_desired_width(pixels(100.f))
        .set_desired_height(percent(0.5f))
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 + 10,
                                }));

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  .x = 0,
                                  .y = 10,
                                  .width = 100,
                                  .height = 50,
                              }));

  CHECK_THAT(to_bounds(div), RectMatcher(Rectangle{
                                 .x = 0,
                                 .y = 50 + 10,
                                 .width = 100,
                                 .height = 360,
                             }));
  CHECK_THAT(to_rect(div), RectMatcher(Rectangle{
                               .x = 0,
                               .y = 50 + 10,
                               .width = 100,
                               .height = 360,
                           }));
}

std::array<RefEntity, 3> grandparent_setup(Entity &sophie, Axis axis) {

  auto &div = EntityHelper::createEntity();
  {
    make_component(div)
        .set_desired_width(pixels(100.f))
        .set_desired_height(percent(0.5f))
        .set_desired_padding(pixels(10.f), axis)
        .set_parent(sophie);
  }

  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(percent(1.f))
        .set_desired_height(percent(0.5f))
        .set_parent(div);
  }

  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(percent(1.f))
        .set_desired_height(percent(0.5f))
        .set_parent(child);
  }

  run_(sophie);
  return {{div, child, button}};
}

TEST_CASE("top padding with grandparent") {
  auto &sophie = make_sophie();
  auto [div, child, button] = grandparent_setup(sophie, Axis::top);

  auto div_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 360 + 10};
  auto div_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = 360};
  CHECK_THAT(to_bounds(div), RectMatcher(div_bounds));
  CHECK_THAT(to_rect(div), RectMatcher(div_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 10, .width = 100, .height = 180};
  auto child_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = 180};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));

  auto button_bounds = Rectangle{.x = 0, .y = 10, .width = 100, .height = 90};
  auto button_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = 90};
  CHECK_THAT(to_bounds(button), RectMatcher(button_bounds));
  CHECK_THAT(to_rect(button), RectMatcher(button_rect));
}

TEST_CASE("bottom padding with grandparent") {
  auto &sophie = make_sophie();
  auto [div, child, button] = grandparent_setup(sophie, Axis::bottom);

  auto div_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 360 + 10};
  auto div_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 360};
  CHECK_THAT(to_bounds(div), RectMatcher(div_bounds));
  CHECK_THAT(to_rect(div), RectMatcher(div_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 180};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 180};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));

  auto button_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 90};
  auto button_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 90};
  CHECK_THAT(to_bounds(button), RectMatcher(button_bounds));
  CHECK_THAT(to_rect(button), RectMatcher(button_rect));
}

TEST_CASE("left padding with grandparent") {
  auto &sophie = make_sophie();
  auto [div, child, button] = grandparent_setup(sophie, Axis::left);

  auto div_bounds = Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = 360};
  auto div_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = 360};
  CHECK_THAT(to_bounds(div), RectMatcher(div_bounds));
  CHECK_THAT(to_rect(div), RectMatcher(div_rect));

  auto child_bounds = Rectangle{.x = 10, .y = 0, .width = 100, .height = 180};
  auto child_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = 180};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));

  auto button_bounds = Rectangle{.x = 10, .y = 0, .width = 100, .height = 90};
  auto button_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = 90};
  CHECK_THAT(to_bounds(button), RectMatcher(button_bounds));
  CHECK_THAT(to_rect(button), RectMatcher(button_rect));
}

TEST_CASE("right padding with grandparent") {
  auto &sophie = make_sophie();
  auto [div, child, button] = grandparent_setup(sophie, Axis::right);

  auto div_bounds = Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = 360};
  auto div_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 360};
  CHECK_THAT(to_bounds(div), RectMatcher(div_bounds));
  CHECK_THAT(to_rect(div), RectMatcher(div_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 180};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 180};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));

  auto button_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = 90};
  auto button_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = 90};
  CHECK_THAT(to_bounds(button), RectMatcher(button_bounds));
  CHECK_THAT(to_rect(button), RectMatcher(button_rect));
}

TEST_CASE("vertical padding") {
  auto &sophie = make_sophie();

  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_padding(pixels(10.f), Axis::Y)
        .set_parent(sophie);
  }

  auto &div = EntityHelper::createEntity();
  {
    make_component(div)
        .set_desired_width(pixels(100.f))
        .set_desired_height(percent(0.5f))
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  .x = 0,
                                  .y = 10,
                                  .width = 100,
                                  .height = 50,
                              }));

  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 70,
                                }));
  CHECK_THAT(to_rect(div), RectMatcher(Rectangle{
                               //
                               .x = 0,
                               .y = 70,
                               .width = 100,
                               .height = 360 //
                           }));
}

TEST_CASE("horizontal padding") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_padding(pixels(10.f), Axis::X)
        .set_parent(sophie);
  }
  run_(sophie);
  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 10,
                                  .y = 0,
                                  .width = 100,
                                  .height = 50 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 120,
                                    .height = 50 //
                                }));
}

TEST_CASE("left margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::left)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 10,
                                  .y = 0,
                                  .width = 90,
                                  .height = 50 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}

TEST_CASE("right margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::right)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 0,
                                  .y = 0,
                                  .width = 90,
                                  .height = 50 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}

TEST_CASE("horizontal margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::X)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 10,
                                  .y = 0,
                                  .width = 80,
                                  .height = 50 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}

TEST_CASE("top margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::top)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 0,
                                  .y = 10,
                                  .width = 100,
                                  .height = 40 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}

TEST_CASE("bottom margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::bottom)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 0,
                                  .y = 0,
                                  .width = 100,
                                  .height = 40 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}

TEST_CASE("vertical margin") {
  auto &sophie = make_sophie();
  auto &button = EntityHelper::createEntity();
  {
    make_component(button)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::Y)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(button), RectMatcher(Rectangle{
                                  //
                                  .x = 0,
                                  .y = 10,
                                  .width = 100,
                                  .height = 30 //
                              }));
  CHECK_THAT(to_bounds(button), RectMatcher(Rectangle{
                                    //
                                    .x = 0,
                                    .y = 0,
                                    .width = 100,
                                    .height = 50 //
                                }));
}
