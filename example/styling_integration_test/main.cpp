#include <source_location>

#include "../shared/vector.h"

#include "../../ah.h"
#include "../../src/plugins/autolayout.h"
#define AFTER_HOURS_IMM_UI
#include "../../src/plugins/ui.h"

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

} // namespace afterhours

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction {};

// Simple test to verify styling defaults integration
struct StylingIntegrationTest : System<UIContext<InputAction>> {

  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {

    // Get the global styling defaults singleton
    auto &styling_defaults = UIStylingDefaults::get();

    // Set up button styling defaults
    auto button_defaults =
        ComponentConfig{}
            .with_padding(Spacing::sm)
            .with_size(ComponentSize{pixels(140.f), pixels(40.f)})
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(8.f);

    // Register the defaults
    styling_defaults.set_component_config(ComponentType::Button,
                                          button_defaults);

    printf("Setting up button styling defaults\n");

    // Create a container div
    auto container = imm::div(context, mk(entity));

    // Test: Create button with minimal config - should use defaults
    auto test_button = imm::button(context, mk(container.ent()),
                                   ComponentConfig{}.with_label("Test Button"));

    printf("Created button with minimal config - styling defaults applied\n");

    // Verify the defaults are accessible
    if (styling_defaults.has_component_defaults(ComponentType::Button)) {
      auto button_config =
          styling_defaults.get_component_config(ComponentType::Button);
      if (button_config.has_value()) {
        printf("Button defaults verified - size: %.1fx%.1f, padding: %.1fpx\n",
               button_config->size.x_axis.value,
               button_config->size.y_axis.value,
               button_config->padding.top.value);
      }
    }

    // Test merging functionality
    auto test_config = ComponentConfig{}.with_label("Merged Button");
    auto merged_config = styling_defaults.merge_with_defaults(
        ComponentType::Button, test_config);
    printf("Merged config test - label: '%s', size: %.1fx%.1f\n",
           merged_config.label.c_str(), merged_config.size.x_axis.value,
           merged_config.size.y_axis.value);
  }
};

int main(int, char **) {
  using namespace afterhours;

  {
    auto &entity = EntityHelper::createEntity();
    ui::add_singleton_components<InputAction>(entity);
    entity.addComponent<ui::AutoLayoutRoot>();
    entity.addComponent<ui::UIComponentDebug>("styling_integration_test");
    entity.addComponent<ui::UIComponent>(entity.id);
  }

  SystemManager systems;

  ui::enforce_singletons<InputAction>(systems);

  systems.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  systems.register_update_system(std::make_unique<StylingIntegrationTest>());
  systems.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());

  for (int i = 0; i < 2; i++) {
    systems.run(1.f);
  }

  return 0;
}
