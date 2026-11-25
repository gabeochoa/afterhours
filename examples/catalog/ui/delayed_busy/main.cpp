
#include "../../../../ah.h"
#include "../../../../src/plugins/ui/component_init.h"
#include "../../../../src/plugins/ui/imm_components.h"

#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct ExampleInputAction {
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

static bool bar_built(DelayedBusy &busy, float value) {
  EntityCollection &coll = UICollectionHolder::get().collection;

  Entity &ctx_entity = coll.createEntity();
  auto &ctx = ctx_entity.addComponent<UIContext<ExampleInputAction>>();
  ctx.screen_width = 800;
  ctx.screen_height = 600;

  Entity &root = coll.createEntity();
  root.addComponent<UIComponent>(root.id);
  root.addComponent<AutoLayoutRoot>();
  root.get<UIComponent>().set_desired_width(pixels(800));
  root.get<UIComponent>().set_desired_height(pixels(600));

  delayed_busy(ctx, mk(root, 0), busy, value,
               ComponentConfig{}.with_size(
                   ComponentSize{pixels(200), pixels(28)})
                   .with_debug_name("busy"));

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
  AutoLayout::autolayout(root.get<UIComponent>(), {800, 600}, mapping, false,
                         1.0f);

  for (const auto &e : coll.get_entities())
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "progress_track")
      return true;
  return false;
}

int main() {
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

  int failures = 0;
  auto expect = [&](bool want, const char *what) {
    std::printf("%s\n", what);
    if (!want) {
      std::printf("  FAIL\n");
      failures++;
    }
  };

  DelayedBusy busy;
  busy.start();
  expect(!bar_built(busy, 0.5f), "op starts: nothing drawn yet");
  busy.update(0.1f);
  expect(!bar_built(busy, 0.5f), "0.10s in (delay 0.25s): still nothing");
  busy.update(0.16f);
  expect(bar_built(busy, 0.5f), "0.26s in: progress bar appears");
  busy.finish();
  expect(!busy.visible(), "op completes: indicator hides");

  return failures == 0 ? 0 : 1;
}
