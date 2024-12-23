

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
      autolayout::UIComponent& divCmp = div.addComponent<autolayout::UIComponent>();
      divCmp.desired[0] = autolayout::Size{
          .dim = autolayout::Dim::Children,
      };
      divCmp.desired[1] = autolayout::Size{
          .dim = autolayout::Dim::Children,
      };
  }

  auto& left = EntityHelper::createEntity();
  {
      autolayout::UIComponent& cmp = left.addComponent<autolayout::UIComponent>();
      cmp.desired[0] = autolayout::Size{
          .dim = autolayout::Dim::Pixels,
          .value = 100.f,
      };
      cmp.desired[1] = autolayout::Size{
          .dim = autolayout::Dim::Pixels,
          .value = 100.f,
      };
  }
  div.get<autolayout::UIComponent>().children.push_back(left.id);

  auto& right = EntityHelper::createEntity();
  {
      autolayout::UIComponent& cmp = right.addComponent<autolayout::UIComponent>();
      cmp.desired[0] = autolayout::Size{
          .dim = autolayout::Dim::Pixels,
          .value = 100.f,
      };
      cmp.desired[1] = autolayout::Size{
          .dim = autolayout::Dim::Pixels,
          .value = 100.f,
      };
  }
  div.get<autolayout::UIComponent>().children.push_back(right.id);

  autolayout::AutoLayout::autolayout(div.get<autolayout::UIComponent>());
  autolayout::AutoLayout::print_tree(div.get<autolayout::UIComponent>());
  

  return 0;
}
