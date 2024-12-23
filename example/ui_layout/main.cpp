

#include <iostream>
#include "../shared/vector.h"

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

}


int main(int, char **) {
  using namespace afterhours;

  auto& div = EntityHelper::createEntity();
  {
      ui::UIComponent& divCmp = div.addComponent<ui::UIComponent>();
      divCmp.desired[0] = ui::Size{
          .dim = ui::Dim::Children,
      };
      divCmp.desired[1] = ui::Size{
          .dim = ui::Dim::Children,
      };
  }

  auto& left = EntityHelper::createEntity();
  {
      ui::UIComponent& cmp = left.addComponent<ui::UIComponent>();
      cmp.desired[0] = ui::Size{
          .dim = ui::Dim::Pixels,
          .value = 100.f,
      };
      cmp.desired[1] = ui::Size{
          .dim = ui::Dim::Pixels,
          .value = 100.f,
      };
  }
  div.get<ui::UIComponent>().children.push_back(left.id);

  auto& right = EntityHelper::createEntity();
  {
      ui::UIComponent& cmp = right.addComponent<ui::UIComponent>();
      cmp.desired[0] = ui::Size{
          .dim = ui::Dim::Pixels,
          .value = 100.f,
      };
      cmp.desired[1] = ui::Size{
          .dim = ui::Dim::Pixels,
          .value = 100.f,
      };
  }
  div.get<ui::UIComponent>().children.push_back(right.id);

  ui::AutoLayout::autolayout(div.get<ui::UIComponent>());
  ui::AutoLayout::print_tree(div.get<ui::UIComponent>());
  

  return 0;
}
