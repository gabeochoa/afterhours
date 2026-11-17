#include <cassert>
#include <cmath>
#include <iostream>

#include "../../../../ah.h"
#include "../../../../src/plugins/toast.h"

using namespace afterhours;

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
  Entity &resolution_entity = EntityHelper::createPermanentEntity();
  resolution_entity.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{800, 600});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      resolution_entity);

  Entity &context_entity = EntityHelper::createEntity();
  auto &context = context_entity.addComponent<ui::UIContext<InputAction>>();
  context.screen_width = 800;
  context.screen_height = 600;
  EntityHelper::registerSingleton<ui::UIContext<InputAction>>(context_entity);

  auto notice = toast::send_info(context, "Saved", 2.0f);
  EntityHelper::merge_entity_arrays();
  assert(notice.ent().has<ui::HasOpacity>());

  toast::ToastLayoutSystem<InputAction> layout;
  toast::ToastUpdateSystem update;
  auto step = [&](float dt) {
    update.for_each_with(notice.ent(), notice.ent().get<toast::Toast>(), dt);
    layout.once(0.f);
    const float opacity = notice.ent().get<ui::HasOpacity>().value;
    std::cout << "elapsed " << notice.ent().get<toast::Toast>().elapsed
              << "s opacity " << opacity << "\n";
    return opacity;
  };

  layout.once(0.f);
  const float fresh = notice.ent().get<ui::HasOpacity>().value;
  std::cout << "elapsed 0s opacity " << fresh << "\n";
  assert(std::fabs(fresh - 1.0f) < 0.001f);
  assert(std::fabs(step(1.0f) - 0.96875f) < 0.001f);
  assert(step(0.9f) < 0.3f);
  assert(std::fabs(step(0.1f)) < 0.001f);
  std::cout << "scheduled toast fades from 1 to 0 over its lifetime\n";
  return 0;
}
