// Action Binding Plugin for Afterhours
// Provides remappable keyboard shortcuts with modifier key support.
// Useful for: games with rebindable controls, productivity apps, accessibility.
//
// To integrate into Afterhours:
// 1. Add this as a plugin in src/plugins/action_binding.h
// 2. Users define their own ActionEnum for their game/app

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace afterhours {
namespace input {

/// Modifier key flags
enum class Modifiers : uint8_t {
  None  = 0,
  Ctrl  = 1 << 0,
  Shift = 1 << 1,
  Alt   = 1 << 2,
};

// Bitwise operators for Modifiers
inline Modifiers operator|(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(
      static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(Modifiers a, Modifiers b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/// Key binding with modifiers
struct KeyBinding {
  int key = 0;
  Modifiers modifiers = Modifiers::None;

  bool operator==(const KeyBinding& other) const {
    return key == other.key && modifiers == other.modifiers;
  }
  
  // Convenience constructors
  static KeyBinding simple(int k) { 
    return KeyBinding{k, Modifiers::None}; 
  }
  
  static KeyBinding ctrl(int k) { 
    return KeyBinding{k, Modifiers::Ctrl}; 
  }
  
  static KeyBinding shift(int k) { 
    return KeyBinding{k, Modifiers::Shift}; 
  }
  
  static KeyBinding ctrl_shift(int k) { 
    return KeyBinding{k, Modifiers::Ctrl | Modifiers::Shift}; 
  }
  
  static KeyBinding alt(int k) { 
    return KeyBinding{k, Modifiers::Alt}; 
  }
};

/// Hash function for KeyBinding
struct KeyBindingHash {
  std::size_t operator()(const KeyBinding& k) const {
    return std::hash<int>()(k.key) ^ 
           (std::hash<uint8_t>()(static_cast<uint8_t>(k.modifiers)) << 4);
  }
};

/// Action map that binds key combinations to named actions
/// ActionEnum should be your enum of game/app actions
template<typename ActionEnum>
class ActionMap {
public:
  ActionMap() = default;

  /// Bind a key combination to an action
  void bind(const KeyBinding& binding, ActionEnum action) {
    bindings_[binding] = action;
    // Also track reverse mapping for UI
    action_to_binding_[action] = binding;
  }

  /// Unbind a key combination
  void unbind(const KeyBinding& binding) {
    auto it = bindings_.find(binding);
    if (it != bindings_.end()) {
      action_to_binding_.erase(it->second);
      bindings_.erase(it);
    }
  }

  /// Check if an action was triggered this frame
  /// Uses function pointers to decouple from specific input backend
  template<typename IsKeyPressedFn, typename IsKeyDownFn>
  bool is_action_pressed(ActionEnum action, 
                         IsKeyPressedFn is_key_pressed,
                         IsKeyDownFn is_key_down) const {
    for (const auto& [binding, act] : bindings_) {
      if (act == action && is_binding_pressed(binding, is_key_pressed, is_key_down)) {
        return true;
      }
    }
    return false;
  }

  /// Check if an action is currently held
  template<typename IsKeyDownFn>
  bool is_action_down(ActionEnum action, IsKeyDownFn is_key_down) const {
    for (const auto& [binding, act] : bindings_) {
      if (act == action && is_binding_down(binding, is_key_down)) {
        return true;
      }
    }
    return false;
  }

  /// Get the binding for an action (for settings UI)
  std::optional<KeyBinding> get_binding(ActionEnum action) const {
    auto it = action_to_binding_.find(action);
    if (it != action_to_binding_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  /// Get all bindings
  const std::unordered_map<KeyBinding, ActionEnum, KeyBindingHash>& 
  get_all_bindings() const {
    return bindings_;
  }

  /// Clear all bindings
  void clear() {
    bindings_.clear();
    action_to_binding_.clear();
  }

private:
  template<typename IsKeyPressedFn, typename IsKeyDownFn>
  bool is_binding_pressed(const KeyBinding& binding,
                          IsKeyPressedFn is_key_pressed,
                          IsKeyDownFn is_key_down) const {
    if (!check_modifiers(binding.modifiers, is_key_down)) {
      return false;
    }
    return is_key_pressed(binding.key);
  }

  template<typename IsKeyDownFn>
  bool is_binding_down(const KeyBinding& binding, IsKeyDownFn is_key_down) const {
    if (!check_modifiers(binding.modifiers, is_key_down)) {
      return false;
    }
    return is_key_down(binding.key);
  }

  template<typename IsKeyDownFn>
  bool check_modifiers(Modifiers required, IsKeyDownFn is_key_down) const {
    bool ctrl_down = is_key_down(KEY_LEFT_CONTROL) || is_key_down(KEY_RIGHT_CONTROL);
    bool shift_down = is_key_down(KEY_LEFT_SHIFT) || is_key_down(KEY_RIGHT_SHIFT);
    bool alt_down = is_key_down(KEY_LEFT_ALT) || is_key_down(KEY_RIGHT_ALT);

    bool need_ctrl = (required & Modifiers::Ctrl);
    bool need_shift = (required & Modifiers::Shift);
    bool need_alt = (required & Modifiers::Alt);

    return (ctrl_down == need_ctrl) && 
           (shift_down == need_shift) && 
           (alt_down == need_alt);
  }

  std::unordered_map<KeyBinding, ActionEnum, KeyBindingHash> bindings_;
  std::unordered_map<ActionEnum, KeyBinding> action_to_binding_;

  // Key codes (raylib values)
  static constexpr int KEY_LEFT_CONTROL = 341;
  static constexpr int KEY_RIGHT_CONTROL = 345;
  static constexpr int KEY_LEFT_SHIFT = 340;
  static constexpr int KEY_RIGHT_SHIFT = 344;
  static constexpr int KEY_LEFT_ALT = 342;
  static constexpr int KEY_RIGHT_ALT = 346;
};

//-----------------------------------------------------------------------------
// Formatting utilities for settings UI
//-----------------------------------------------------------------------------

/// Format modifiers as string
inline std::string format_modifiers(Modifiers mods) {
  std::string result;
  if (mods & Modifiers::Ctrl) result += "Ctrl+";
  if (mods & Modifiers::Alt) result += "Alt+";
  if (mods & Modifiers::Shift) result += "Shift+";
  return result;
}

/// Format a key code as human-readable string (basic set)
inline std::string key_name(int key_code) {
  // Letters A-Z
  if (key_code >= 65 && key_code <= 90) {
    return std::string(1, static_cast<char>(key_code));
  }
  // Numbers 0-9
  if (key_code >= 48 && key_code <= 57) {
    return std::string(1, static_cast<char>(key_code));
  }
  // Common keys
  switch (key_code) {
    case 32: return "Space";
    case 256: return "Escape";
    case 257: return "Enter";
    case 258: return "Tab";
    case 259: return "Backspace";
    case 261: return "Delete";
    case 262: return "Right";
    case 263: return "Left";
    case 264: return "Down";
    case 265: return "Up";
    case 266: return "Page Up";
    case 267: return "Page Down";
    case 268: return "Home";
    case 269: return "End";
    case 290: return "F1";
    case 291: return "F2";
    case 292: return "F3";
    case 293: return "F4";
    case 294: return "F5";
    case 295: return "F6";
    case 296: return "F7";
    case 297: return "F8";
    case 298: return "F9";
    case 299: return "F10";
    case 300: return "F11";
    case 301: return "F12";
    default: return "?";
  }
}

/// Format a binding as "Ctrl+Shift+S" style string
inline std::string format_binding(const KeyBinding& binding) {
  return format_modifiers(binding.modifiers) + key_name(binding.key);
}

} // namespace input

//-----------------------------------------------------------------------------
// Example usage:
//-----------------------------------------------------------------------------
/*
// Define your game's actions
enum class GameAction {
  MoveUp, MoveDown, MoveLeft, MoveRight,
  Jump, Attack, Interact,
  Pause, Save, Load,
  COUNT
};

// Create and configure action map
input::ActionMap<GameAction> actions;

// Bind keys
actions.bind(input::KeyBinding::simple(KEY_W), GameAction::MoveUp);
actions.bind(input::KeyBinding::simple(KEY_SPACE), GameAction::Jump);
actions.bind(input::KeyBinding::ctrl(KEY_S), GameAction::Save);

// Check in update loop (using raylib functions)
if (actions.is_action_pressed(GameAction::Jump, IsKeyPressed, IsKeyDown)) {
  player.jump();
}

// Display in settings UI
for (auto action : {GameAction::Save, GameAction::Load}) {
  if (auto binding = actions.get_binding(action)) {
    DrawText(format_binding(*binding).c_str(), x, y, 16, WHITE);
  }
}
*/

} // namespace afterhours

