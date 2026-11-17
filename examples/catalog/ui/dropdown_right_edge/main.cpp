#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) &&    \
    !defined(AFTER_HOURS_USE_METAL) && !defined(AFTER_HOURS_USE_VULKAN) &&     \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/ui/component_init.h"
#include "../../../../src/plugins/ui/imm_components.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

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

  auto begin_frame = [&] {
    coll.merge_entity_arrays();
    for (const auto &e : coll.get_entities())
      if (e && e->has<UIComponent>())
        e->get<UIComponent>().children.clear();
  };
  auto layout = [&] {
    coll.merge_entity_arrays();
    auto &entities = coll.get_entities();
    EntityID max_id = 0;
    for (const auto &e : entities)
      if (e)
        max_id = std::max(max_id, e->id);
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (const auto &e : entities)
      if (e)
        mapping[static_cast<size_t>(e->id)] = e.get();
    AutoLayout::autolayout(root.get<UIComponent>(),
                           window_manager::Resolution{800, 600}, mapping,
                           false, 1.0f);
  };

  std::vector<std::string> opts = {"One", "Two", "Three", "Four"};
  size_t idx = 0;
  auto emit = [&] {
    dropdown(ctx, mk(root, 0), opts, idx,
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(160), pixels(30)})
                 .with_absolute_position(700.f, 50.f));
  };

  begin_frame();
  emit();
  layout();
  for (const auto &e : coll.get_entities())
    if (e && e->has<HasDropdownState>())
      e->get<HasDropdownState>().on = true;
  begin_frame();
  emit();
  layout();

  UIComponent *tray = nullptr;
  for (const auto &e : coll.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "dropdown_options_tray")
      tray = &e->get<UIComponent>();
  }

  int failures = 0;
  if (!tray) {
    std::printf("dropdown_options_tray not found\n");
    failures++;
  } else {
    const auto r = tray->rect();
    std::printf("trigger x=700 w=160, tray x=%.1f y=%.1f w=%.1f h=%.1f "
                "(screen 800x600)\n",
                r.x, r.y, r.width, r.height);
    if (r.x < 0.f || r.x + r.width > 800.f) {
      std::printf("tray runs off the right edge\n");
      failures++;
    }
  }

  for (const auto &e : coll.get_entities())
    if (e)
      e->cleanup = true;
  coll.cleanup();
  imm::clear_existing_ui_elements();

  if (failures != 0) {
    std::printf("dropdown_right_edge example: %d failures\n", failures);
    return 1;
  }
  std::printf("dropdown_right_edge example: ok\n");
  return 0;
}
