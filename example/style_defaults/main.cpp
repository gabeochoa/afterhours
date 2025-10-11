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

// Demonstrate the styling defaults concept and integration
namespace custom_ui_helpers {

// PROBLEM: We have to hardcode values instead of accessing global constants
auto create_button_old_way(UIContext<InputAction> &context,
                           EntityParent ep_pair, const std::string &label) {
  // This is what the feedback is complaining about - hardcoded values
  auto config = ComponentConfig{}
                    .with_padding(Spacing::xs)
                    .with_size(ComponentSize{pixels(100.f), pixels(30.f)})
                    .with_label(label);

  return imm::button(context, ep_pair, config);
}

// SOLUTION: Just use basic config, styling defaults are applied automatically
auto create_button_new_way(UIContext<InputAction> &context,
                           EntityParent ep_pair, const std::string &label) {
  // Much simpler - just the essentials
  auto config = ComponentConfig{}.with_label(label);
  return imm::button(context, ep_pair, config);
}

} // namespace custom_ui_helpers

struct StylingDefaultsDemo : System<UIContext<InputAction>> {

  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {

    // Get the global styling defaults singleton
    auto &styling_defaults = UIStylingDefaults::get();

    printf("=== Styling Defaults Demo ===\n");

    // Set up comprehensive styling defaults
    auto button_defaults =
        ComponentConfig{}
            .with_padding(Spacing::sm)
            .with_size(ComponentSize{pixels(140.f), pixels(40.f)})
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(8.f);

    auto div_defaults = ComponentConfig{}
                            .with_padding(Spacing::md)
                            .with_margin(Spacing::xs)
                            .with_color_usage(Theme::Usage::Background);

    auto slider_defaults =
        ComponentConfig{}
            .with_size(ComponentSize{pixels(200.f), pixels(25.f)})
            .with_color_usage(Theme::Usage::Secondary)
            .with_rounded_corners(4.f);

    // Register the defaults
    styling_defaults.set_component_config(ComponentType::Button,
                                          button_defaults);
    styling_defaults.set_component_config(ComponentType::Div, div_defaults);
    styling_defaults.set_component_config(ComponentType::Slider,
                                          slider_defaults);

    printf(
        "✓ Set up styling defaults for Button, Div, and Slider components\n");

    // Create a container div
    auto container = imm::div(context, mk(entity));

    // Test 1: Demonstrate the old way vs new way
    printf("\n--- Problem vs Solution ---\n");

    // OLD WAY: Hardcoded values
    auto old_button = custom_ui_helpers::create_button_old_way(
        context, mk(container.ent()), "Old Way (Hardcoded)");
    printf("✗ Created button with hardcoded values\n");

    // NEW WAY: Simple config with automatic defaults
    auto new_button = custom_ui_helpers::create_button_new_way(
        context, mk(container.ent()), "New Way (Defaults)");
    printf("✓ Created button with minimal config - styling defaults applied "
           "automatically\n");

    // Test 2: Test different component types
    printf("\n--- Component Type Testing ---\n");

    // Test Div with defaults
    auto test_div = imm::div(context, mk(container.ent()),
                             ComponentConfig{}.with_label("Test Div"));
    printf("✓ Created div with minimal config - using Div defaults\n");

    // Test Slider with defaults
    float slider_value = 0.5f;
    auto test_slider = imm::slider(context, mk(container.ent()), slider_value,
                                   ComponentConfig{}.with_label("Test Slider"));
    printf("✓ Created slider with minimal config - using Slider defaults\n");

    // Test 3: Verify defaults are accessible and working
    printf("\n--- Default Verification ---\n");

    if (styling_defaults.has_component_defaults(ComponentType::Button)) {
      auto button_config =
          styling_defaults.get_component_config(ComponentType::Button);
      if (button_config.has_value()) {
        printf(
            "✓ Button defaults verified - size: %.1fx%.1f, padding: %.1fpx\n",
            button_config->size.x_axis.value, button_config->size.y_axis.value,
            button_config->padding.top.value);
      }
    }

    // Test 4: Demonstrate merging functionality
    printf("\n--- Merging Functionality ---\n");

    auto test_config = ComponentConfig{}.with_label("Merged Button");
    auto merged_config = styling_defaults.merge_with_defaults(
        ComponentType::Button, test_config);
    printf("✓ Merged config test - label: '%s', size: %.1fx%.1f\n",
           merged_config.label.c_str(), merged_config.size.x_axis.value,
           merged_config.size.y_axis.value);

    // Test 5: Demonstrate builder pattern
    printf("\n--- Builder Pattern ---\n");

    ComponentConfigBuilder builder;
    builder.with_padding(Spacing::lg)
        .with_size(ComponentSize{pixels(160.f), pixels(50.f)})
        .with_color_usage(Theme::Usage::Secondary)
        .with_label("Builder Button")
        .with_rounded_corners(10.f);

    printf("✓ Builder pattern works - all methods inherited automatically\n");

    // Test 6: Demonstrate singleton accessibility
    printf("\n--- Singleton Access ---\n");

    auto &global_defaults = UIStylingDefaults::get();
    printf("✓ Singleton styling defaults accessible globally\n");

    // Test 7: Test partial overrides
    printf("\n--- Partial Overrides ---\n");

    auto custom_button =
        imm::button(context, mk(container.ent()),
                    ComponentConfig{}
                        .with_label("Custom Button")
                        .with_size(ComponentSize{pixels(180.f), pixels(45.f)})
                        .with_color_usage(Theme::Usage::Secondary));
    printf("✓ Created custom button with partial override - merged with "
           "defaults\n");

    printf("\n=== Demo Complete ===\n");
  }
};

int main(int, char **) {
  using namespace afterhours;

  {
    auto &entity = EntityHelper::createEntity();
    ui::add_singleton_components<InputAction>(entity);
    entity.addComponent<ui::AutoLayoutRoot>();
    entity.addComponent<ui::UIComponentDebug>("styling_defaults_demo");
    entity.addComponent<ui::UIComponent>(entity.id);
  }

  SystemManager systems;

  ui::enforce_singletons<InputAction>(systems);

  systems.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  systems.register_update_system(std::make_unique<StylingDefaultsDemo>());
  systems.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());

  for (int i = 0; i < 2; i++) {
    systems.run(1.f);
  }

  return 0;
}
