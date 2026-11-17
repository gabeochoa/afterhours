#define AFTER_HOURS_USE_NONE 1

#include <cstdio>
#include <string>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/ui/component_init.h"
#include "../../../../src/plugins/ui/imm_components.h"
#include "../../../../src/plugins/ui/rendering.h"

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

struct Harness {
  EntityCollection &coll;
  UIContext<InputAction> *ctx = nullptr;
  Entity *root_entity = nullptr;

  Harness() : coll(UICollectionHolder::get().collection) {
    clear_existing_ui_elements();
    if (!EntityHelper::has_singleton<TextMeasureCache>()) {
      Entity &tm = EntityHelper::createPermanentEntity();
      auto &cache = tm.addComponent<TextMeasureCache>();
      cache.set_measure_function(
          [](std::string_view text, std::string_view, float font_size,
             float) -> Vector2Type {
            return {static_cast<float>(text.size()) * font_size * 0.5f,
                    font_size};
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
    set_measure_text_fn([](const char *text, float font_size, float) {
      return Vector2Type{
          static_cast<float>(std::string_view(text ? text : "").size()) *
              font_size * 0.5f,
          font_size};
    });
    Entity &ctx_entity = coll.createEntity();
    ctx = &ctx_entity.addComponent<UIContext<InputAction>>();
    ctx->screen_width = 800;
    ctx->screen_height = 600;
    root_entity = &coll.createEntity();
    root_entity->addComponent<UIComponent>(root_entity->id);
    root_entity->addComponent<AutoLayoutRoot>();
    auto &root_cmp = root_entity->get<UIComponent>();
    root_cmp.set_desired_width(pixels(800));
    root_cmp.set_desired_height(pixels(600));
  }

  ~Harness() {
    for (const auto &e : coll.get_entities())
      if (e)
        e->cleanup = true;
    coll.cleanup();
    clear_existing_ui_elements();
  }

  void layout_only() {
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
    AutoLayout::autolayout(root_entity->get<UIComponent>(), {800, 600},
                           mapping, false, 1.0f);
  }
};

static int built_with(bool use_default, size_t overscan) {
  Harness h;
  int built = 0;
  auto config = ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(200)})
                    .with_debug_name("orders");
  if (!use_default)
    config.with_virtual_list_overscan(overscan);
  virtual_list(
      *h.ctx, mk(*h.root_entity, 0), 2000, 20.f,
      [&](size_t i, Entity &row) {
        built++;
        div(*h.ctx, mk(row, 0),
            ComponentConfig{}
                .with_size(ComponentSize{percent(1.0f), pixels(20)})
                .with_label("Order " + std::to_string(i)));
      },
      config);
  h.layout_only();
  return built;
}

int main() {
  const int by_default = built_with(true, 0);
  const int tight = built_with(false, 0);
  const int roomy = built_with(false, 12);
  std::printf("default overscan built %d rows, overscan 0 built %d, "
              "overscan 12 built %d of 2000\n",
              by_default, tight, roomy);
  return (tight < by_default && by_default < roomy) ? 0 : 1;
}
