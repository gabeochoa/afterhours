#include <iostream>
#include <vector>

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

struct EntityIDConflictTest : System<UIContext<InputAction>> {

  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {

    // This reproduces the issue described in the feedback
    // When mk() is called in a loop without proper index management,
    // it can cause std::bad_optional_access crashes due to entity ID conflicts
    
    auto parent = imm::div(context, mk(entity));
    
    // PROBLEM: Using mk() in a loop without index management
    // This can cause entity ID conflicts because all calls to mk() 
    // from the same source location will generate the same hash
    std::cout << "Creating buttons without proper index management..." << std::endl;
    
    try {
      for (int i = 0; i < 5; i++) {
        // This is the problematic pattern - all these calls use the same source location
        // so they generate the same hash and entity ID conflicts
        auto button = imm::button(context, mk(parent.ent()));
        if (button) {
          std::cout << "Button " << i << " created successfully" << std::endl;
        }
      }
    } catch (const std::exception& e) {
      std::cout << "Exception caught: " << e.what() << std::endl;
    }
    
    // CORRECT PATTERN: Using manual index management
    std::cout << "\nCreating buttons with manual index management..." << std::endl;
    try {
      for (int i = 0; i < 5; i++) {
        // This is the correct pattern - passing a unique index to avoid conflicts
        auto button = imm::button(context, mk(parent.ent(), i));
        if (button) {
          std::cout << "Button " << i << " created successfully with index " << i << std::endl;
        }
      }
    } catch (const std::exception& e) {
      std::cout << "Exception caught: " << e.what() << std::endl;
    }
  }
};

int main(int, char **) {
  using namespace afterhours;

  {
    auto &entity = EntityHelper::createEntity();
    ui::add_singleton_components<InputAction>(entity);
    entity.addComponent<ui::AutoLayoutRoot>();
    entity.addComponent<ui::UIComponentDebug>("singleton");
    entity.addComponent<ui::UIComponent>(entity.id);
  }

  SystemManager systems;

  ui::enforce_singletons<InputAction>(systems);

  systems.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  systems.register_update_system(std::make_unique<EntityIDConflictTest>());
  systems.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());

  // Run multiple frames to see the issue
  for (int i = 0; i < 3; i++) {
    std::cout << "\n=== Frame " << i << " ===" << std::endl;
    systems.run(1.f);
  }

  return 0;
} 
