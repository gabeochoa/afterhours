#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"

using namespace afterhours;
using namespace afterhours::ui;

int main(int, char **) {
  std::vector<std::unique_ptr<Entity>> entities;
  auto make_ui = [&](Size w, Size h) -> Entity & {
    entities.push_back(std::make_unique<Entity>());
    Entity &e = *entities.back();
    auto &cmp = e.addComponent<UIComponent>(e.id);
    cmp.set_desired_width(w);
    cmp.set_desired_height(h);
    return e;
  };

  Entity &root = make_ui(pixels(736), pixels(100));
  Entity &child = make_ui(percent(1.0f), pixels(50));
  child.get<UIComponent>().set_desired_margin(Margin{.left = pixels(16)});
  root.get<UIComponent>().add_child(child.id);
  child.get<UIComponent>().set_parent(root.id);

  EntityID max_id = 0;
  for (auto &e : entities)
    max_id = std::max(max_id, e->id);
  std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
  for (auto &e : entities)
    mapping[static_cast<size_t>(e->id)] = e.get();

  AutoLayout::autolayout(root.get<UIComponent>(),
                         window_manager::Resolution{1280, 720}, mapping);

  auto r = child.get<UIComponent>().rect();
  std::printf("percent(1.0) child with 16px left margin: x=%.0f width=%.0f "
              "right=%.0f (parent content width 736)\n",
              r.x, r.width, r.x + r.width);
  bool fits = std::fabs(r.x - 16.f) < 0.5f &&
              std::fabs(r.width - 720.f) < 0.5f &&
              std::fabs(r.x + r.width - 736.f) < 0.5f;
  std::printf("%s\n", fits ? "fits inside the parent" : "OVERFLOWS the parent");
  return fits ? 0 : 1;
}
