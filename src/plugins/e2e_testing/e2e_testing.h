// E2E Testing Framework for Afterhours
// ECS-native testing framework with input injection, script DSL, and UI
// assertions.
//
// Quick Start:
//   #include <afterhours/src/plugins/e2e_testing/e2e_testing.h>
//   using namespace afterhours::testing;
//
//   // Register handlers
//   register_all_handlers(system_manager);
//
//   // Or with UI plugin integration:
//   register_builtin_handlers(system_manager);
//   ui_commands::register_ui_commands<MyInputAction>(system_manager);
//   register_unknown_handler(system_manager);
//   register_cleanup(system_manager);
//
// Architecture:
//   input_injector  - Low-level synthetic key/mouse state
//   test_input      - High-level input queue with frame awareness
//   VisibleTextRegistry - Track rendered text for assertions
//   PendingE2ECommand   - ECS component for command dispatch
//   E2ERunner           - Script DSL parser that creates command entities
//
// Custom commands: Create systems that handle PendingE2ECommand
// Semantic actions: Use 'action WidgetLeft' instead of 'key LEFT' (via
// ui_commands.h)
#pragma once

#include "../../core/key_codes.h"
#include "command_handlers.h"
#include "concepts.h"

#include "input_injector.h"
#include "pending_command.h"
#include "runner.h"
#include "test_input.h"
#include "visible_text.h"

namespace afterhours {
namespace testing {

// Register a system to run BEFORE built-in handlers (to override)
template <IsSystem S>
void register_before_builtins(SystemManager &sm, std::unique_ptr<S> sys) {
  sm.register_update_system(std::move(sys));
}

// Register built-in command handlers (type, click, key, wait, expect_text,
// etc.)
inline void register_builtin_handlers(SystemManager &sm) {
  sm.register_update_system(std::make_unique<HandleKeyReleaseSystem>());  // Must be first to release before new presses
  sm.register_update_system(std::make_unique<HandleTypeCommand>());
  sm.register_update_system(std::make_unique<HandleKeyCommand>());
  sm.register_update_system(std::make_unique<HandleClickCommand>());
  sm.register_update_system(std::make_unique<HandleDoubleClickCommand>());
  sm.register_update_system(std::make_unique<HandleDragCommand>());
  sm.register_update_system(std::make_unique<HandleMouseMoveCommand>());
  sm.register_update_system(std::make_unique<HandleMouseDownCommand>());
  sm.register_update_system(std::make_unique<HandleMouseUpCommand>());
  sm.register_update_system(std::make_unique<HandleDragToCommand>());
  sm.register_update_system(std::make_unique<HandleWaitCommand>());
  sm.register_update_system(std::make_unique<HandleWaitFramesCommand>());
  sm.register_update_system(std::make_unique<HandleExpectTextCommand>());
  // Note: screenshot and reset_test_state need callbacks, registered separately
}

// Register a system to run AFTER built-in handlers (to extend or add custom)
template <IsSystem S>
void register_after_builtins(SystemManager &sm, std::unique_ptr<S> sys) {
  sm.register_update_system(std::move(sys));
}

// Register unknown command warning (call after all your custom handlers)
inline void register_unknown_handler(SystemManager &sm) {
  sm.register_update_system(std::make_unique<HandleUnknownCommand>());
}

// Register cleanup system (call last - removes processed commands)
inline void register_cleanup(SystemManager &sm) {
  sm.register_update_system(std::make_unique<E2ECommandCleanupSystem>());
}

// Register all handlers at once (builtins + unknown + cleanup)
inline void register_all_handlers(SystemManager &sm) {
  register_builtin_handlers(sm);
  register_unknown_handler(sm);
  register_cleanup(sm);
}

//=============================================================================
// REGISTRATION ORDER
//=============================================================================
//
// 1. register_builtin_handlers(sm)     - Built-in: type, click, key, wait, etc.
// 2. [your custom command handlers]    - App-specific commands
// 3. ui_commands::register_ui_commands<InputAction>(sm)  - If using UI plugin
// 4. register_unknown_handler(sm)      - Warns about unhandled commands
// 5. register_cleanup(sm)              - Cleanup (must be last)
//
// Or just: register_all_handlers(sm)   - If you don't need custom commands
//
//=============================================================================
// UI PLUGIN INTEGRATION (optional)
//=============================================================================
//
// If using the UI plugin, include ui_commands.h separately:
//   #include <afterhours/src/plugins/e2e_testing/ui_commands.h>
//   ui_commands::register_ui_commands<MyInputAction>(sm);
//
// Semantic actions (preferred):  action WidgetLeft, hold WidgetLeft, release
// Component commands:  click_ui, click_button, toggle_checkbox, set_slider
// Assertions:  expect_focused, expect_checkbox, expect_slider
// Raw key fallbacks:  tab, enter, escape, arrow

} // namespace testing
} // namespace afterhours
