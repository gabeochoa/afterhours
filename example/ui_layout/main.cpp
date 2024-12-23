

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

namespace afterhours {

struct Transform : public BaseComponent {
  Transform(float x, float y) {
    position.x = x;
    position.y = y;
  }

  virtual ~Transform() {}

  [[nodiscard]] vec2 pos() const { return position; }
  void update(vec2 v) { position = v; }

private:
  vec2 position;
};

void run_and_print_tree(ui::UIComponent &root) {

  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }

  ui::AutoLayout::autolayout(root, components);
  ui::AutoLayout::print_tree(root);
}

} // namespace afterhours

std::ostream &operator<<(std::ostream &os, const RectangleType &data) {
  os << "x: " << data.x;
  os << ", y: " << data.y;
  os << ", width: " << data.width;
  os << ", height: " << data.height;
  os << std::endl;
  return os;
}

int main(int, char **) {
  using namespace afterhours;

  auto &div = EntityHelper::createEntity();
  {
    div.addComponent<ui::UIComponent>(div.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Children,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Children,
        });
  }

  auto &left = EntityHelper::createEntity();
  {
    left.addComponent<ui::UIComponent>(left.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = 100.f,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = 100.f,
        });
  }
  div.get<ui::UIComponent>().children.push_back(left.id);

  auto &right = EntityHelper::createEntity();
  {
    right.addComponent<ui::UIComponent>(right.id)
        .set_desired_width(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = 100.f,
        })
        .set_desired_height(ui::Size{
            .dim = ui::Dim::Pixels,
            .value = 100.f,
        });
  }
  div.get<ui::UIComponent>().children.push_back(right.id);

  run_and_print_tree(div.get<ui::UIComponent>());

  {
    auto &Sophie = EntityHelper::createEntity();
    {
      Sophie.addComponent<ui::AutoLayoutRoot>();
      Sophie.addComponent<ui::UIComponent>(Sophie.id)
          .set_desired_width(ui::Size{
              // TODO figure out how to update this
              // when resolution changes
              .dim = ui::Dim::Pixels,
              .value = 1280.f,
          })
          .set_desired_height(ui::Size{
              .dim = ui::Dim::Pixels,
              .value = 720.f,
          });
    }

    auto &button = EntityHelper::createEntity();
    {
      button.addComponent<ui::UIComponent>(button.id)
          .set_desired_width(ui::Size{
              // TODO figure out how to update this
              // when resolution changes
              .dim = ui::Dim::Pixels,
              .value = 100.f,
          })
          .set_desired_height(ui::Size{
              .dim = ui::Dim::Pixels,
              .value = 50.f,
          })
          .set_parent(Sophie.id);
      Sophie.get<ui::UIComponent>().add_child(button.id);
    }

    auto &div2 = EntityHelper::createEntity();
    {
      div2.addComponent<ui::UIComponent>(div2.id)
          .set_desired_width(ui::Size{
              // TODO figure out how to update this
              // when resolution changes
              .dim = ui::Dim::Pixels,
              .value = 100.f,
          })
          .set_desired_height(ui::Size{
              .dim = ui::Dim::Percent,
              .value = 0.5f,
          })
          .set_parent(Sophie.id);
      Sophie.get<ui::UIComponent>().add_child(div2.id);
    }

    run_and_print_tree(Sophie.get<ui::UIComponent>());

    std::cout << div2.get<ui::UIComponent>().rect();
    std::cout << " should be \n";
    std::cout << Rectangle{.x = 0, .y = 50, .width = 100, .height = 360}
              << std::endl;
  }

  return 0;
}
