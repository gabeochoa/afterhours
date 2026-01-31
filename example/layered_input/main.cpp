// example/layered_input/main.cpp
//
// Demonstrates the layered input plugin which allows different
// key bindings per game state (e.g., menu vs gameplay).
//

#include <cstdio>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../../src/plugins/input_system.h"

// Example layer enum (projects define their own)
enum class GameLayer {
  Menu,
  Playing,
  Paused
};

// Example action enum
enum Action {
  MoveUp = 0,
  MoveDown,
  Confirm,
  Cancel,
  Pause
};

int main(int, char**) {
  using namespace afterhours;

  // Create system manager
  SystemManager systems;

  // Define layered mappings
  std::map<GameLayer, std::map<int, input::ValidInputs>> mapping;

#ifdef AFTER_HOURS_USE_RAYLIB
  // Menu layer: arrows for navigation, enter for confirm
  mapping[GameLayer::Menu][Action::MoveUp] = {raylib::KEY_UP};
  mapping[GameLayer::Menu][Action::MoveDown] = {raylib::KEY_DOWN};
  mapping[GameLayer::Menu][Action::Confirm] = {raylib::KEY_ENTER};

  // Playing layer: WASD for movement, ESC for pause
  mapping[GameLayer::Playing][Action::MoveUp] = {raylib::KEY_W};
  mapping[GameLayer::Playing][Action::MoveDown] = {raylib::KEY_S};
  mapping[GameLayer::Playing][Action::Pause] = {raylib::KEY_ESCAPE};

  // Paused layer: only ESC to unpause (no movement)
  mapping[GameLayer::Paused][Action::Pause] = {raylib::KEY_ESCAPE};
#else
  // Non-raylib placeholder mappings using raw key codes
  mapping[GameLayer::Menu][Action::MoveUp] = {265};     // KEY_UP
  mapping[GameLayer::Menu][Action::MoveDown] = {264};   // KEY_DOWN
  mapping[GameLayer::Menu][Action::Confirm] = {257};    // KEY_ENTER

  mapping[GameLayer::Playing][Action::MoveUp] = {87};   // KEY_W
  mapping[GameLayer::Playing][Action::MoveDown] = {83}; // KEY_S
  mapping[GameLayer::Playing][Action::Pause] = {256};   // KEY_ESCAPE

  mapping[GameLayer::Paused][Action::Pause] = {256};    // KEY_ESCAPE
#endif

  // Create singleton entity with layered input
  Entity& singleton = EntityHelper::createEntity();
  layered_input<GameLayer>::add_singleton_components(
      singleton, mapping, GameLayer::Menu);

  // Register systems
  layered_input<GameLayer>::register_update_systems(systems);

  // Demonstrate layer switching
  auto* mapper = singleton.get_ptr<ProvidesLayeredInputMapping<GameLayer>>();

  printf("Initial layer: %d (Menu)\n", static_cast<int>(mapper->get_active_layer()));

  // Switch to Playing layer
  mapper->set_active_layer(GameLayer::Playing);
  printf("After switch: %d (Playing)\n", static_cast<int>(mapper->get_active_layer()));

  // In Playing layer, MoveUp should return WASD bindings
  const auto& bindings = mapper->get_bindings(Action::MoveUp);
  printf("MoveUp has %zu bindings in Playing layer\n", bindings.size());

  // Switch to Paused - MoveUp should have no bindings
  mapper->set_active_layer(GameLayer::Paused);
  const auto& paused_bindings = mapper->get_bindings(Action::MoveUp);
  printf("MoveUp has %zu bindings in Paused layer\n", paused_bindings.size());

  // Demonstrate runtime binding modification
  printf("\nDemonstrating runtime binding modification...\n");

  // Add a new binding to the Paused layer
#ifdef AFTER_HOURS_USE_RAYLIB
  mapper->set_binding(GameLayer::Paused, Action::Confirm, {raylib::KEY_SPACE});
#else
  mapper->set_binding(GameLayer::Paused, Action::Confirm, {32}); // KEY_SPACE
#endif

  const auto& new_bindings = mapper->get_bindings(Action::Confirm);
  printf("After adding Confirm binding to Paused: %zu bindings\n", new_bindings.size());

  // Clear the binding
  mapper->clear_binding(GameLayer::Paused, Action::Confirm);
  const auto& cleared_bindings = mapper->get_bindings(Action::Confirm);
  printf("After clearing Confirm binding: %zu bindings\n", cleared_bindings.size());

  printf("\nLayered input example completed successfully!\n");
  return 0;
}
