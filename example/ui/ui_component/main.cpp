
#include <source_location>

#include "../../../ah.h"
#include "../../../src/plugins/autolayout.h"
#include "../../shared/vector.h"
#define AFTER_HOURS_IMM_UI
#include "../../../src/plugins/ui.h"

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

}  // namespace afterhours

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
};

UIComponent &_fetch_component(Entity &ent) {
    if (ent.is_missing<UIComponent>()) {
        // this is the first time we are looking in here
    }
    return ent.get<UIComponent>();
}

struct UITest : System<UIContext<InputAction>> {
    virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                               float) override {
        auto elem = imm::div(context, mk(entity));
        if (button(context, mk(elem.ent()))) {
            std::cout << "button clicked" << std::endl;
        }
    }
};

int main(int, char **) {
    using namespace afterhours;

    {
        auto &entity = EntityHelper::createEntity();
        entity.addComponent<UIContext<InputAction>>();
        EntityHelper::registerSingleton<UIContext<InputAction>>(entity);
        entity.addComponent<ui::FontManager>()
            .load_font(UIComponent::DEFAULT_FONT, get_default_font())
            .load_font(UIComponent::SYMBOL_FONT, get_default_font())
            .load_font(UIComponent::UNSET_FONT, get_unset_font());
        EntityHelper::registerSingleton<ui::FontManager>(entity);
        entity.addComponent<window_manager::ProvidesCurrentResolution>(
            window_manager::Resolution{1280, 720});
        EntityHelper::registerSingleton<
            window_manager::ProvidesCurrentResolution>(entity);
        entity.addComponent<ui::AutoLayoutRoot>();
        entity.addComponent<ui::UIComponentDebug>("singleton");
        entity.addComponent<ui::UIComponent>(entity.id)
            .set_desired_width(ui::screen_pct(1.f))
            .set_desired_height(ui::screen_pct(1.f));
    }

    SystemManager systems;

    ui::enforce_singletons<InputAction>(systems);

    systems.register_update_system(
        std::make_unique<ui::ClearUIComponentChildren>());
    systems.register_update_system(
        std::make_unique<ui::BeginUIContextManager<InputAction>>());
    systems.register_update_system(std::make_unique<UITest>());
    systems.register_update_system(
        std::make_unique<ui::EndUIContextManager<InputAction>>());

    systems.run(1.f);

    return 0;
}
