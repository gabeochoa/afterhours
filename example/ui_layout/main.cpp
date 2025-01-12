

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

constexpr static float WIDTH = 1280.f;
constexpr static float HEIGHT = 720.f;

constexpr static float W2 = WIDTH / 2.f;
constexpr static float W4 = WIDTH / 4.f;
constexpr static float W8 = WIDTH / 8.f;

constexpr static float H2 = HEIGHT / 2.f;
constexpr static float H4 = HEIGHT / 4.f;
constexpr static float H8 = HEIGHT / 8.f;

auto &to_cmp(Entity &entity) { return AutoLayout::to_cmp_static(entity.id); }
auto to_rect(Entity &entity) { return to_cmp(entity).rect(); }
auto to_bounds(Entity &entity) { return to_cmp(entity).bounds(); }

void run_(Entity &root_element) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }
  ui::AutoLayout::autolayout(root_element.get<UIComponent>(),
                             {(int)WIDTH, (int)HEIGHT}, components);
}

void print_tree(Entity &root_ent, ui::UIComponent &root) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }
  ui::AutoLayout::autolayout(root, {(int)WIDTH, (int)HEIGHT}, components);
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

    ss << "should match reference: \n" << to_string(a);
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
        .set_desired_width(pixels(WIDTH))
        .set_desired_height(pixels(HEIGHT));
  }
  return sophie;
}

enum struct ApplicationLocation {
  Grandparent,
  Parent,
  Child,
};

std::array<RefEntity, 3> grandparent_setup_padding(
    Entity &sophie, Axis axis, Size padding_size,
    ApplicationLocation location = ApplicationLocation::Grandparent) {

  auto &div = EntityHelper::createEntity();
  {
    auto &c = make_component(div)
                  .set_desired_width(pixels(100.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(sophie);

    if (location == ApplicationLocation::Grandparent)
      c.set_desired_padding(padding_size, axis);
  }

  auto &parent = EntityHelper::createEntity();
  {
    auto &c = make_component(parent)
                  .set_desired_width(percent(1.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(div);

    if (location == ApplicationLocation::Parent)
      c.set_desired_padding(padding_size, axis);
  }

  auto &child = EntityHelper::createEntity();
  {
    auto &c = make_component(child)
                  .set_desired_width(percent(1.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(parent);

    if (location == ApplicationLocation::Child)
      c.set_desired_padding(padding_size, axis);
  }

  run_(sophie);
  return {{div, parent, child}};
}

std::array<RefEntity, 3> grandparent_setup_margin(
    Entity &sophie, Axis axis, Size margin_size,
    ApplicationLocation location = ApplicationLocation::Grandparent) {

  auto &grandparent = EntityHelper::createEntity();
  {
    auto &c = make_component(grandparent)
                  .set_desired_width(pixels(100.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(sophie);

    if (location == ApplicationLocation::Grandparent)
      c.set_desired_margin(margin_size, axis);
  }

  auto &parent = EntityHelper::createEntity();
  {
    auto &c = make_component(parent)
                  .set_desired_width(percent(1.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(grandparent);

    if (location == ApplicationLocation::Parent)
      c.set_desired_margin(margin_size, axis);
  }

  auto &child = EntityHelper::createEntity();
  {
    auto &c = make_component(child)
                  .set_desired_width(percent(1.f))
                  .set_desired_height(percent(0.5f))
                  .set_parent(parent);

    if (location == ApplicationLocation::Child)
      c.set_desired_margin(margin_size, axis);
  }

  run_(sophie);
  return {{grandparent, parent, child}};
}

TEST_CASE("root test") {
  auto &sophie = make_sophie();
  run_(sophie);
  CHECK_THAT(sophie.get<ui::UIComponent>().rect(), RectMatcher(Rectangle{
                                                       //
                                                       .x = 0,
                                                       .y = 0,
                                                       .width = WIDTH,
                                                       .height = HEIGHT //
                                                   }));
}

TEST_CASE("default test") {
  auto &sophie = make_sophie();

  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
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
                                                    .height = H2 //
                                                }));
}

TEST_CASE("top padding") {
  auto &sophie = make_sophie();

  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_padding(pixels(10.f), Axis::top)
        .set_parent(sophie);
  }

  auto &grandparent = EntityHelper::createEntity();
  {
    make_component(grandparent)
        .set_desired_width(pixels(100.f))
        .set_desired_height(percent(0.5f))
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 + 10,
                               }));

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 .x = 0,
                                 .y = 10,
                                 .width = 100,
                                 .height = 50,
                             }));

  CHECK_THAT(to_bounds(grandparent), RectMatcher(Rectangle{
                                         .x = 0,
                                         .y = 50 + 10,
                                         .width = 100,
                                         .height = H2,
                                     }));
  CHECK_THAT(to_rect(grandparent), RectMatcher(Rectangle{
                                       .x = 0,
                                       .y = 50 + 10,
                                       .width = 100,
                                       .height = H2,
                                   }));
}

TEST_CASE("top pixel padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::top, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 + 10};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 10, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 10, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom pixel padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::bottom, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 + 10};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left pixel padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::left, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 10, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 10, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 10, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right pixel padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::right, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top pixel padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::top, pixels(10.f), ApplicationLocation::Parent);
  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};

  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H4 + 10};
  auto parent_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = H4};

  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 10, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = H8};

  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left pixel padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::left, pixels(10.f), ApplicationLocation::Parent);
  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));
  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = H4};
  auto parent_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));
  auto child_bounds = Rectangle{.x = 10, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 10, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}
TEST_CASE("bottom pixel padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::bottom, pixels(10.f), ApplicationLocation::Parent);
  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));
  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H4 + 10};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));
  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right pixel padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::right, pixels(10.f), ApplicationLocation::Parent);
  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));
  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + 10, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));
  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::top, percent(0.05f));

  float gap = 36;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 + gap};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = gap, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = gap, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = gap, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = gap, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::left, percent(0.05f));
  float gap = 64;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = gap, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = gap, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = gap, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = gap, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = gap, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::bottom, percent(0.05f));
  float gap = 36;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 + gap};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));
  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));
  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}
TEST_CASE("right percent padding on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_padding(sophie, Axis::right, percent(0.05f));
  float gap = 64;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));
  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));
  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::top, percent(0.05f), ApplicationLocation::Parent);
  float gap = 18;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H4 + gap};
  auto parent_rect = Rectangle{.x = 0, .y = gap, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = gap, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = gap, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::left, percent(0.05f), ApplicationLocation::Parent);
  float gap = 5;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H4};
  auto parent_rect = Rectangle{.x = gap, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = gap, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = gap, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::bottom, percent(0.05f), ApplicationLocation::Parent);
  float gap = 18;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H4 + gap};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right percent padding on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::right, percent(0.05f), ApplicationLocation::Parent);
  float gap = 5;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent padding on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::top, percent(0.05f), ApplicationLocation::Child);
  float gap = 9;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H8 + gap};
  auto child_rect = Rectangle{.x = 0, .y = gap, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent padding on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::left, percent(0.05f), ApplicationLocation::Child);
  float gap = 5;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H8};
  auto child_rect = Rectangle{.x = gap, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent padding on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::bottom, percent(0.05f), ApplicationLocation::Child);
  float gap = 9;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H8 + gap};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right percent padding on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_padding(
      sophie, Axis::right, percent(0.05f), ApplicationLocation::Child);
  float gap = 5;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  // TODO should the child bounds go outside its parent bounds?
  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 + gap, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("vertical padding") {
  auto &sophie = make_sophie();

  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
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

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 .x = 0,
                                 .y = 10,
                                 .width = 100,
                                 .height = 50,
                             }));

  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 70,
                               }));
  CHECK_THAT(to_rect(div), RectMatcher(Rectangle{
                               .x = 0,
                               .y = 70,
                               .width = 100,
                               .height = H2,
                           }));
}

TEST_CASE("horizontal padding") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_padding(pixels(10.f), Axis::X)
        .set_parent(sophie);
  }
  run_(sophie);
  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 10,
                                 .y = 0,
                                 .width = 100,
                                 .height = 50 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 120,
                                   .height = 50 //
                               }));
}

TEST_CASE("left margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::left)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 10,
                                 .y = 0,
                                 .width = 90,
                                 .height = 50 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("right margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::right)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 0,
                                 .y = 0,
                                 .width = 90,
                                 .height = 50 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("horizontal margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::X)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 10,
                                 .y = 0,
                                 .width = 80,
                                 .height = 50 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("top margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::top)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 0,
                                 .y = 10,
                                 .width = 100,
                                 .height = 40 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("bottom margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::bottom)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 0,
                                 .y = 0,
                                 .width = 100,
                                 .height = 40 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("vertical margin") {
  auto &sophie = make_sophie();
  auto &child = EntityHelper::createEntity();
  {
    make_component(child)
        .set_desired_width(pixels(100.f))
        .set_desired_height(pixels(50.f))
        .set_desired_margin(pixels(10.f), Axis::Y)
        .set_parent(sophie);
  }

  run_(sophie);

  CHECK_THAT(to_rect(child), RectMatcher(Rectangle{
                                 //
                                 .x = 0,
                                 .y = 10,
                                 .width = 100,
                                 .height = 30 //
                             }));
  CHECK_THAT(to_bounds(child), RectMatcher(Rectangle{
                                   //
                                   .x = 0,
                                   .y = 0,
                                   .width = 100,
                                   .height = 50 //
                               }));
}

TEST_CASE("top pixel margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::top, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = H2 - 10};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = (H2 - 10) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = (H2 - 10) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = 10, .width = 100, .height = std::ceil((H2 - 10) / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = 10, .width = 100, .height = std::ceil((H2 - 10) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left pixel margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::left, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = (H2) / 2.f};
  auto parent_rect =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = (H2) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 10, .y = 0, .width = 100 - 10, .height = std::ceil((H2) / 4.f)};
  auto child_rect = Rectangle{
      .x = 10, .y = 0, .width = 100 - 10, .height = std::ceil((H2) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom pixel margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::bottom, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 - 10};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H2 - 10) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H2 - 10) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = 0, .width = 100, .height = std::ceil((H2 - 10) / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = 0, .width = 100, .height = std::ceil((H2 - 10) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right pixel margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::right, pixels(10.f));

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = (H2) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = (H2) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = 0, .width = 100 - 10, .height = std::ceil((H2) / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = 0, .width = 100 - 10, .height = std::ceil((H2) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top pixel margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::top, pixels(10.f), ApplicationLocation::Parent);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = (H4 - 10)};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = (H4 - 10) / 2.f};
  auto child_rect =
      Rectangle{.x = 0, .y = 10, .width = 100, .height = (H4 - 10) / 2.f};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left pixel margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::left, pixels(10.f), ApplicationLocation::Parent);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = H4 / 2.f};
  auto child_rect =
      Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = H4 / 2.f};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom pixel margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::bottom, pixels(10.f), ApplicationLocation::Parent);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4 - 10};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H4 - 10) / 2.f};
  auto child_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H4 - 10) / 2.f};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right pixel margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::right, pixels(10.f), ApplicationLocation::Parent);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = H4 / 2.f};
  auto child_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = H4 / 2.f};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top pixel margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::top, pixels(10.f), ApplicationLocation::Child);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 10, .width = 100, .height = H8 - 10};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left pixel margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::left, pixels(10.f), ApplicationLocation::Child);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 10, .y = 0, .width = 100 - 10, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom pixel margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::bottom, pixels(10.f), ApplicationLocation::Child);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8 - 10};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right pixel margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::right, pixels(10.f), ApplicationLocation::Child);

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100 - 10, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::top, percent(0.1f));

  float gap = HEIGHT * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = H2 - gap};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = (H2 - gap) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = (H2 - gap) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = gap, .width = 100, .height = std::ceil((H2 - gap) / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = gap, .width = 100, .height = std::ceil((H2 - gap) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::left, percent(0.1f));

  float gap = WIDTH * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = gap, .y = 0, .width = 100 - gap, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = gap, .y = 0, .width = 100 - gap, .height = (H2) / 2.f};
  auto parent_rect =
      Rectangle{.x = gap, .y = 0, .width = 100 - gap, .height = (H2) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = gap, .y = 0, .width = 100 - gap, .height = std::ceil(H2 / 4.f)};
  auto child_rect = Rectangle{
      .x = gap, .y = 0, .width = 100 - gap, .height = std::ceil(H2 / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::bottom, percent(0.1f));

  float gap = HEIGHT * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 - gap};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H2 - gap) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H2 - gap) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = 0, .width = 100, .height = std::ceil((H2 - gap) / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = 0, .width = 100, .height = std::ceil((H2 - gap) / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right percent margin on grandparent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] =
      grandparent_setup_margin(sophie, Axis::right, percent(0.1f));

  float gap = WIDTH * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - gap, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100 - gap, .height = (H2) / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - gap, .height = (H2) / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{
      .x = 0, .y = 0, .width = 100 - gap, .height = std::ceil(H2 / 4.f)};
  auto child_rect = Rectangle{
      .x = 0, .y = 0, .width = 100 - gap, .height = std::ceil(H2 / 4.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::top, percent(0.1f), ApplicationLocation::Parent);

  float gap = H2 * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = (H2 / 2.f) - gap};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0,
                                .y = gap,
                                .width = 100,
                                .height = std::ceil(((H2 / 2.f) - gap) / 2.f)};
  auto child_rect = Rectangle{.x = 0,
                              .y = gap,
                              .width = 100,
                              .height = std::ceil(((H2 / 2.f) - gap) / 2.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::left, percent(0.1f), ApplicationLocation::Parent);

  float gap = 100 * 0.1f;

  auto gp_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto gp_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(gp_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(gp_rect));

  auto p_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  auto p_rect =
      Rectangle{.x = gap, .y = 0, .width = 100 - gap, .height = H2 / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(p_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(p_rect));

  auto child_bounds = Rectangle{.x = gap,
                                .y = 0,
                                .width = 100 - gap,
                                .height = std::ceil((H2 / 2.f) / 2.f)};
  auto child_rect = Rectangle{.x = gap,
                              .y = 0,
                              .width = 100 - gap,
                              .height = std::ceil((H2 / 2.f) / 2.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::bottom, percent(0.1f), ApplicationLocation::Parent);

  float gap = H2 * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = (H2 / 2.f) - gap};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0,
                                .y = 0,
                                .width = 100,
                                .height = std::ceil(((H2 / 2.f) - gap) / 2.f)};
  auto child_rect = Rectangle{.x = 0,
                              .y = 0,
                              .width = 100,
                              .height = std::ceil(((H2 / 2.f) - gap) / 2.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right percent margin on parent") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::right, percent(0.1f), ApplicationLocation::Parent);

  float gap = 100 * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100 - gap, .height = H2 / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0,
                                .y = 0,
                                .width = 100 - gap,
                                .height = std::ceil((H2 / 2.f) / 2.f)};
  auto child_rect = Rectangle{.x = 0,
                              .y = 0,
                              .width = 100 - gap,
                              .height = std::ceil((H2 / 2.f) / 2.f)};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("top percent margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::top, percent(0.1f), ApplicationLocation::Child);

  float gap = H4 * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect =
      Rectangle{.x = 0, .y = gap, .width = 100, .height = (H8)-gap};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("left percent margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::left, percent(0.1f), ApplicationLocation::Child);

  float gap = (100) * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  auto parent_rect =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 2.f};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2 / 4.f};
  auto child_rect =
      Rectangle{.x = gap, .y = 0, .width = 100 - gap, .height = H2 / 4.f};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("bottom percent margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::bottom, percent(0.1f), ApplicationLocation::Child);

  float gap = H4 * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8 - gap};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}

TEST_CASE("right percent margin on child") {
  auto &sophie = make_sophie();
  auto [grandparent, parent, child] = grandparent_setup_margin(
      sophie, Axis::right, percent(0.1f), ApplicationLocation::Child);

  float gap = (100) * 0.1f;

  auto grandparent_bounds =
      Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  auto grandparent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H2};
  CHECK_THAT(to_bounds(grandparent), RectMatcher(grandparent_bounds));
  CHECK_THAT(to_rect(grandparent), RectMatcher(grandparent_rect));

  auto parent_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  auto parent_rect = Rectangle{.x = 0, .y = 0, .width = 100, .height = H4};
  CHECK_THAT(to_bounds(parent), RectMatcher(parent_bounds));
  CHECK_THAT(to_rect(parent), RectMatcher(parent_rect));

  auto child_bounds = Rectangle{.x = 0, .y = 0, .width = 100, .height = H8};
  auto child_rect = Rectangle{.x = 0, .y = 0, .width = 100 - gap, .height = H8};
  CHECK_THAT(to_bounds(child), RectMatcher(child_bounds));
  CHECK_THAT(to_rect(child), RectMatcher(child_rect));
}
