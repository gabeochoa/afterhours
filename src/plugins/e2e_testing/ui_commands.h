// E2E Testing Framework - UI Plugin Integration
// Automatic command handlers for UI components
// Include this if you're using both e2e_testing and ui plugins
#pragma once

#include "../../core/key_codes.h"
#include "pending_command.h"
#include "test_input.h"

#include "../ui/auto_layout.h"
#include "../ui/components.h"
#include "../ui/context.h"

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

namespace afterhours {
namespace testing {
namespace ui_commands {

// Find a UI component by its debug name and return its center position
template <typename InputAction>
std::optional<Position> find_component_center(const std::string &name) {
  auto query = EntityQuery()
                   .whereHasComponent<ui::UIComponent>()
                   .whereHasComponent<ui::UIComponentDebug>()
                   .whereLambda([&](const Entity &e) {
                     return e.get<ui::UIComponentDebug>().name() == name &&
                            e.get<ui::UIComponent>().was_rendered_to_screen;
                   })
                   .first();

  for (Entity &entity : query.gen()) {
    auto &cmp = entity.get<ui::UIComponent>();
    float cx = cmp.rect().x + cmp.rect().width / 2;
    float cy = cmp.rect().y + cmp.rect().height / 2;
    return Position{cx, cy};
  }
  return std::nullopt;
}

// Find a UI component containing specific text
template <typename InputAction>
std::optional<Position> find_component_with_text(const std::string &text) {
  auto query = EntityQuery()
                   .whereHasComponent<ui::UIComponent>()
                   .whereHasComponent<ui::HasLabel>()
                   .whereLambda([&](const Entity &e) {
                     return e.get<ui::HasLabel>().label.find(text) !=
                                std::string::npos &&
                            e.get<ui::UIComponent>().was_rendered_to_screen;
                   })
                   .first();

  // TODO add a gen_lambda() to do this conversion
  for (Entity &entity : query.gen()) {
    auto &cmp = entity.get<ui::UIComponent>();
    float cx = cmp.rect().x + cmp.rect().width / 2;
    float cy = cmp.rect().y + cmp.rect().height / 2;
    return Position{cx, cy};
  }
  return std::nullopt;
}

// Handle 'click_ui name' - clicks UI component by debug name
template <typename InputAction>
struct HandleClickUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_ui"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("click_ui requires component name");
      return;
    }

    auto pos = find_component_center<InputAction>(cmd.arg(0));
    if (pos) {
      test_input::simulate_click(pos->x, pos->y);
      cmd.consume();
    } else {
      cmd.fail(fmt::format("UI component not found: {}", cmd.arg(0)));
    }
  }
};

// Handle 'click_text "text"' - clicks UI component containing text
template <typename InputAction>
struct HandleClickTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_text"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("click_text requires text");
      return;
    }

    auto pos = find_component_with_text<InputAction>(cmd.arg(0));
    if (pos) {
      test_input::simulate_click(pos->x, pos->y);
      cmd.consume();
    } else {
      cmd.fail(fmt::format("No UI with text: {}", cmd.arg(0)));
    }
  }
};

// Handle 'focus_ui name' - focuses UI component by debug name (for tabbing)
template <typename InputAction>
struct HandleFocusUICommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("focus_ui"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("focus_ui requires component name");
      return;
    }

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name &&
                              e.get<ui::UIComponent>().was_rendered_to_screen;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &cmp = entity.get<ui::UIComponent>();
      auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
      if (ctx) {
        ctx->set_focus(cmp.id);
        cmd.consume();
        return;
      }
    }
    cmd.fail(fmt::format("UI component not found: {}", cmd.arg(0)));
  }
};

// Handle 'tab' - simulates Tab key press
struct HandleTabCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("tab"))
      return;

    test_input::push_key(keys::TAB);
    cmd.consume();
  }
};

// Handle 'shift_tab' - simulates Shift+Tab for reverse tabbing
struct HandleShiftTabCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("shift_tab"))
      return;

    input_injector::set_key_down(keys::LEFT_SHIFT);
    test_input::push_key(keys::TAB);
    cmd.consume();
  }
};

// Handle 'enter' - simulates Enter key press (activate focused element)
struct HandleEnterCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("enter"))
      return;

    test_input::push_key(keys::ENTER);
    cmd.consume();
  }
};

// Handle 'escape' - simulates Escape key press
struct HandleEscapeCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("escape"))
      return;

    test_input::push_key(keys::ESCAPE);
    cmd.consume();
  }
};

// Handle 'arrow direction' - simulates arrow key
struct HandleArrowCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("arrow"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("arrow requires direction (up/down/left/right)");
      return;
    }

    const auto &dir = cmd.arg(0);
    int key = 0;
    if (dir == "up")
      key = keys::UP;
    else if (dir == "down")
      key = keys::DOWN;
    else if (dir == "left")
      key = keys::LEFT;
    else if (dir == "right")
      key = keys::RIGHT;
    else {
      cmd.fail(fmt::format("Invalid arrow direction: {}", dir));
      return;
    }

    test_input::push_key(key);
    cmd.consume();
  }
};

// Handle 'expect_focused name' - asserts a component is focused
template <typename InputAction>
struct HandleExpectFocusedCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_focused"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_focused requires component name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &cmp = entity.get<ui::UIComponent>();
      if (ctx->has_focus(cmp.id)) {
        cmd.consume();
        return;
      } else {
        cmd.fail(fmt::format("Component '{}' is not focused", name));
        return;
      }
    }
    cmd.fail(fmt::format("UI component not found: {}", cmd.arg(0)));
  }
};

// Handle 'click_button "text"' - clicks button by its label text
template <typename InputAction>
struct HandleClickButtonCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click_button"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("click_button requires button text");
      return;
    }

    const auto &text = cmd.arg(0);
    auto query =
        EntityQuery()
            .whereHasComponent<ui::UIComponent>()
            .whereHasComponent<ui::HasLabel>()
            .whereHasComponent<ui::HasClickListener>()
            .whereLambda([&](const Entity &e) {
              const auto &label = e.get<ui::HasLabel>().label;
              return (label == text || label.find(text) != std::string::npos) &&
                     e.get<ui::UIComponent>().was_rendered_to_screen;
            })
            .first();

    for (Entity &entity : query.gen()) {
      auto &cmp = entity.get<ui::UIComponent>();
      float cx = cmp.rect().x + cmp.rect().width / 2;
      float cy = cmp.rect().y + cmp.rect().height / 2;
      test_input::simulate_click(cx, cy);
      cmd.consume();
      return;
    }
    cmd.fail(fmt::format("Button not found: {}", cmd.arg(0)));
  }
};

// Handle 'toggle_checkbox name' - toggles checkbox by debug name
template <typename InputAction>
struct HandleToggleCheckboxCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("toggle_checkbox"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("toggle_checkbox requires name");
      return;
    }

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereHasComponent<ui::HasCheckboxState>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name &&
                              e.get<ui::UIComponent>().was_rendered_to_screen;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &cmp = entity.get<ui::UIComponent>();
      float cx = cmp.rect().x + cmp.rect().width / 2;
      float cy = cmp.rect().y + cmp.rect().height / 2;
      test_input::simulate_click(cx, cy);
      cmd.consume();
      return;
    }
    cmd.fail(fmt::format("Checkbox not found: {}", cmd.arg(0)));
  }
};

// Handle 'set_slider name value' - sets slider value by debug name
template <typename InputAction>
struct HandleSetSliderCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("set_slider"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("set_slider requires name and value");
      return;
    }

    auto value = cmd.maybe_arg_as<float>(1);
    if (!value) {
      cmd.fail(fmt::format("Invalid slider value: {}", cmd.arg(1)));
      return;
    }

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereHasComponent<ui::HasSliderState>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name &&
                              e.get<ui::UIComponent>().was_rendered_to_screen;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &slider = entity.get<ui::HasSliderState>();
      auto &cmp = entity.get<ui::UIComponent>();
      // Calculate click position along slider for target value
      float pct = (*value - slider.min) / (slider.max - slider.min);
      pct = std::clamp(pct, 0.0f, 1.0f);
      float x = cmp.rect().x + cmp.rect().width * pct;
      float y = cmp.rect().y + cmp.rect().height / 2;
      test_input::simulate_click(x, y);
      cmd.consume();
      return;
    }
    cmd.fail(fmt::format("Slider not found: {}", cmd.arg(0)));
  }
};

// Handle 'select_dropdown name option' - selects dropdown option by debug name
template <typename InputAction>
struct HandleSelectDropdownCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("select_dropdown"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("select_dropdown requires name and option");
      return;
    }

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereHasComponent<ui::HasDropdownState>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name &&
                              e.get<ui::UIComponent>().was_rendered_to_screen;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &cmp = entity.get<ui::UIComponent>();
      // Click to open dropdown first
      float cx = cmp.rect().x + cmp.rect().width / 2;
      float cy = cmp.rect().y + cmp.rect().height / 2;
      test_input::simulate_click(cx, cy);
      // Note: Selecting the actual option requires another frame
      // TODO: Add wait + option click logic
      cmd.consume();
      return;
    }
    cmd.fail(fmt::format("Dropdown not found: {}", cmd.arg(0)));
  }
};

// Handle 'expect_checkbox name state' - asserts checkbox is checked/unchecked
template <typename InputAction>
struct HandleExpectCheckboxCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_checkbox"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("expect_checkbox requires name and state");
      return;
    }

    bool expected =
        cmd.arg(1) == "true" || cmd.arg(1) == "checked" || cmd.arg(1) == "1";
    const auto &name = cmd.arg(0);

    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereHasComponent<ui::HasCheckboxState>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &checkbox = entity.get<ui::HasCheckboxState>();
      if (checkbox.on == expected) {
        cmd.consume();
        return;
      } else {
        cmd.fail(fmt::format("Checkbox '{}' is {}, expected {}", name,
                             checkbox.on ? "checked" : "unchecked",
                             cmd.arg(1)));
        return;
      }
    }
    cmd.fail(fmt::format("Checkbox not found: {}", cmd.arg(0)));
  }
};

// Handle 'expect_slider name value [tolerance]' - asserts slider value
template <typename InputAction>
struct HandleExpectSliderCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_slider"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("expect_slider requires name and value");
      return;
    }

    auto expected = cmd.maybe_arg_as<float>(1);
    if (!expected) {
      cmd.fail(fmt::format("Invalid slider value: {}", cmd.arg(1)));
      return;
    }
    float tolerance = cmd.arg_as<float>(2, 0.01f);

    const auto &name = cmd.arg(0);
    auto query = EntityQuery()
                     .whereHasComponent<ui::UIComponent>()
                     .whereHasComponent<ui::UIComponentDebug>()
                     .whereHasComponent<ui::HasSliderState>()
                     .whereLambda([&](const Entity &e) {
                       return e.get<ui::UIComponentDebug>().name() == name;
                     })
                     .first();

    for (Entity &entity : query.gen()) {
      auto &slider = entity.get<ui::HasSliderState>();
      if (std::abs(slider.value - *expected) <= tolerance) {
        cmd.consume();
        return;
      } else {
        cmd.fail(fmt::format("Slider '{}' is {}, expected {}", name,
                             slider.value, *expected));
        return;
      }
    }
    cmd.fail(fmt::format("Slider not found: {}", cmd.arg(0)));
  }
};

// Handle 'action ActionName' - triggers a semantic input action
// Example: action WidgetLeft, action TextBackspace, action WidgetPress
template <typename InputAction>
struct HandleActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("action"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("action requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    // Parse action name using magic_enum
    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(fmt::format("Unknown action: {}. Valid actions: {}, ...",
                           cmd.arg(0),
                           magic_enum::enum_names<InputAction>()[0]));
      return;
    }

    ctx->last_action = *action;
    cmd.consume();
  }
};

// Handle 'hold ActionName' - holds an action down (sets in bitset)
template <typename InputAction>
struct HandleHoldActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("hold"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("hold requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(fmt::format("Unknown action: {}", cmd.arg(0)));
      return;
    }

    auto idx = magic_enum::enum_index(*action);
    if (idx) {
      ctx->all_actions[*idx] = true;
    }
    cmd.consume();
  }
};

// Handle 'release ActionName' - releases a held action
template <typename InputAction>
struct HandleReleaseActionCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("release"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("release requires action name");
      return;
    }

    auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    if (!ctx) {
      cmd.fail("UIContext not found");
      return;
    }

    auto action = magic_enum::enum_cast<InputAction>(cmd.arg(0));
    if (!action) {
      cmd.fail(fmt::format("Unknown action: {}", cmd.arg(0)));
      return;
    }

    auto idx = magic_enum::enum_index(*action);
    if (idx) {
      ctx->all_actions[*idx] = false;
    }
    cmd.consume();
  }
};

// Register all UI command handlers
template <typename InputAction> void register_ui_commands(SystemManager &sm) {
  // Semantic actions (preferred - works with your InputAction enum)
  sm.register_update_system(
      std::make_unique<HandleActionCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleHoldActionCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleReleaseActionCommand<InputAction>>());

  // Component interactions (auto-find by name/text)
  sm.register_update_system(
      std::make_unique<HandleClickUICommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleClickTextCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleClickButtonCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleFocusUICommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleToggleCheckboxCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleSetSliderCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleSelectDropdownCommand<InputAction>>());

  // Raw key fallbacks (for edge cases)
  sm.register_update_system(std::make_unique<HandleTabCommand>());
  sm.register_update_system(std::make_unique<HandleShiftTabCommand>());
  sm.register_update_system(std::make_unique<HandleEnterCommand>());
  sm.register_update_system(std::make_unique<HandleEscapeCommand>());
  sm.register_update_system(std::make_unique<HandleArrowCommand>());

  // Assertions
  sm.register_update_system(
      std::make_unique<HandleExpectFocusedCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectCheckboxCommand<InputAction>>());
  sm.register_update_system(
      std::make_unique<HandleExpectSliderCommand<InputAction>>());
}

} // namespace ui_commands
} // namespace testing
} // namespace afterhours
