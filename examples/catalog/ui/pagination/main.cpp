#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) &&    \
    !defined(AFTER_HOURS_USE_METAL) && !defined(AFTER_HOURS_USE_VULKAN) &&     \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif

#include "../../../../ah.h"
#include "../../../../src/plugins/ui/component_init.h"
#include "../../../../src/plugins/ui/imm_components.h"

#include <array>
#include <cstdio>
#include <string_view>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetUp,
  WidgetDown,
  WidgetLeft,
  WidgetRight,
  TextBackspace,
  TextDelete,
  TextHome,
  TextEnd,
  TextSelectAll,
  TextCopy,
  TextCut,
  TextPaste,
  TextUndo,
  TextRedo,
  TextSelectLeft,
  TextSelectRight,
  TextWordLeft,
  TextWordRight,
  TextDeleteWordBack,
  TextDeleteWordForward,
  MenuBack,
};

int main() {
  EntityCollection &coll = UICollectionHolder::get().collection;
  imm::clear_existing_ui_elements();

  if (!EntityHelper::has_singleton<TextMeasureCache>()) {
    Entity &tm = EntityHelper::createPermanentEntity();
    auto &cache = tm.addComponent<TextMeasureCache>();
    cache.set_measure_function([](std::string_view text, std::string_view,
                                  float font_size,
                                  float) -> Vector2Type {
      return {static_cast<float>(text.size()) * font_size * 0.5f, font_size};
    });
    EntityHelper::registerSingleton<TextMeasureCache>(tm);
  }
  if (!EntityHelper::has_singleton<
          window_manager::ProvidesCurrentResolution>()) {
    Entity &res = EntityHelper::createPermanentEntity();
    res.addComponent<window_manager::ProvidesCurrentResolution>(
        window_manager::Resolution{800, 600});
    EntityHelper::registerSingleton<
        window_manager::ProvidesCurrentResolution>(res);
  }

  Entity &ctx_entity = coll.createEntity();
  auto &ctx = ctx_entity.addComponent<UIContext<InputAction>>();
  ctx.screen_width = 800;
  ctx.screen_height = 600;

  Entity &root = coll.createEntity();
  root.addComponent<UIComponent>(root.id);
  root.addComponent<AutoLayoutRoot>();
  root.get<UIComponent>().set_desired_width(pixels(800));
  root.get<UIComponent>().set_desired_height(pixels(600));

  const std::array<std::string_view, 5> pages{"1", "2", "3", "4", "5"};
  size_t page = 0;
  Entity *pager = nullptr;

  auto build = [&]() {
    for (const auto &e : coll.get_entities())
      if (e && e->has<UIComponent>())
        e->get<UIComponent>().children.clear();
    auto result = pagination(ctx, mk(root, 0), pages, page,
                             ComponentConfig{}.with_size(
                                 ComponentSize{pixels(400), pixels(40)}));
    pager = &result.ent();
    return static_cast<bool>(result);
  };

  auto click_child = [&](size_t pos) {
    Entity &target = UICollectionHolder::getEntityForID(
                         pager->get<UIComponent>().children[pos])
                         .asE();
    target.get<HasClickListener>().down = true;
    build();
    target.get<HasClickListener>().down = false;
  };

  int failures = 0;
  auto expect = [&](size_t want, const char *what) {
    std::printf("%s -> page %zu\n", what, page + 1);
    if (page != want) {
      std::printf("expected page %zu\n", want + 1);
      failures++;
    }
  };

  build();
  expect(0, "start");
  click_child(6);
  expect(1, "next");
  click_child(6);
  expect(2, "next");
  click_child(0);
  expect(1, "previous");
  click_child(3);
  expect(2, "page button 3");
  click_child(5);
  expect(4, "page button 5");
  click_child(6);
  expect(0, "next wraps");
  click_child(0);
  expect(4, "previous wraps");

  for (const auto &e : coll.get_entities())
    if (e)
      e->cleanup = true;
  coll.cleanup();
  imm::clear_existing_ui_elements();

  if (failures != 0) {
    std::printf("pagination example: %d failures\n", failures);
    return 1;
  }
  std::printf("pagination example: ok\n");
  return 0;
}
