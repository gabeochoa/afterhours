// Key Code Constants and Utilities
// GLFW/Raylib compatible integer key codes
#pragma once

#include <array>
#include <cstring>
#include <string>
#include <string_view>

namespace afterhours {

// Key codes - GLFW/Raylib compatible integer values
// These match both raylib's KEY_* and GLFW's GLFW_KEY_* constants
namespace keys {

constexpr int APOSTROPHE = 39;
constexpr int COMMA = 44;
constexpr int MINUS = 45;
constexpr int PERIOD = 46;
constexpr int SLASH = 47;
constexpr int ZERO = 48;
constexpr int ONE = 49;
constexpr int TWO = 50;
constexpr int THREE = 51;
constexpr int FOUR = 52;
constexpr int FIVE = 53;
constexpr int SIX = 54;
constexpr int SEVEN = 55;
constexpr int EIGHT = 56;
constexpr int NINE = 57;
constexpr int SEMICOLON = 59;
constexpr int EQUAL = 61;
constexpr int A = 65;
constexpr int B = 66;
constexpr int C = 67;
constexpr int D = 68;
constexpr int E = 69;
constexpr int F = 70;
constexpr int G = 71;
constexpr int H = 72;
constexpr int I = 73;
constexpr int J = 74;
constexpr int K = 75;
constexpr int L = 76;
constexpr int M = 77;
constexpr int N = 78;
constexpr int O = 79;
constexpr int P = 80;
constexpr int Q = 81;
constexpr int R = 82;
constexpr int S = 83;
constexpr int T = 84;
constexpr int U = 85;
constexpr int V = 86;
constexpr int W = 87;
constexpr int X = 88;
constexpr int Y = 89;
constexpr int Z = 90;
constexpr int LEFT_BRACKET = 91;
constexpr int BACKSLASH = 92;
constexpr int RIGHT_BRACKET = 93;
constexpr int GRAVE = 96;
constexpr int SPACE = 32;
constexpr int ESCAPE = 256;
constexpr int ENTER = 257;
constexpr int TAB = 258;
constexpr int BACKSPACE = 259;
constexpr int INSERT = 260;
constexpr int DELETE_KEY = 261;
constexpr int RIGHT = 262;
constexpr int LEFT = 263;
constexpr int DOWN = 264;
constexpr int UP = 265;
constexpr int PAGE_UP = 266;
constexpr int PAGE_DOWN = 267;
constexpr int HOME = 268;
constexpr int END = 269;
constexpr int CAPS_LOCK = 280;
constexpr int SCROLL_LOCK = 281;
constexpr int NUM_LOCK = 282;
constexpr int PRINT_SCREEN = 283;
constexpr int PAUSE = 284;
constexpr int F1 = 290;
constexpr int F2 = 291;
constexpr int F3 = 292;
constexpr int F4 = 293;
constexpr int F5 = 294;
constexpr int F6 = 295;
constexpr int F7 = 296;
constexpr int F8 = 297;
constexpr int F9 = 298;
constexpr int F10 = 299;
constexpr int F11 = 300;
constexpr int F12 = 301;
constexpr int LEFT_SHIFT = 340;
constexpr int LEFT_CONTROL = 341;
constexpr int LEFT_ALT = 342;
constexpr int LEFT_SUPER = 343;
constexpr int RIGHT_SHIFT = 344;
constexpr int RIGHT_CONTROL = 345;
constexpr int RIGHT_ALT = 346;
constexpr int RIGHT_SUPER = 347;
constexpr int KB_MENU = 348;
constexpr int KP_0 = 320;
constexpr int KP_1 = 321;
constexpr int KP_2 = 322;
constexpr int KP_3 = 323;
constexpr int KP_4 = 324;
constexpr int KP_5 = 325;
constexpr int KP_6 = 326;
constexpr int KP_7 = 327;
constexpr int KP_8 = 328;
constexpr int KP_9 = 329;
constexpr int KP_DECIMAL = 330;
constexpr int KP_DIVIDE = 331;
constexpr int KP_MULTIPLY = 332;
constexpr int KP_SUBTRACT = 333;
constexpr int KP_ADD = 334;
constexpr int KP_ENTER = 335;
constexpr int KP_EQUAL = 336;

} // namespace keys

// Mouse button codes — match raylib/GLFW values
namespace mouse_buttons {
constexpr int LEFT = 0;
constexpr int RIGHT = 1;
constexpr int MIDDLE = 2;
constexpr int SIDE = 3;
constexpr int EXTRA = 4;
constexpr int FORWARD = 5;
constexpr int BACK = 6;
} // namespace mouse_buttons

// Gamepad button codes — match raylib values
namespace gamepad_buttons {
constexpr int UNKNOWN = 0;
constexpr int LEFT_FACE_UP = 1;
constexpr int LEFT_FACE_RIGHT = 2;
constexpr int LEFT_FACE_DOWN = 3;
constexpr int LEFT_FACE_LEFT = 4;
constexpr int RIGHT_FACE_UP = 5;
constexpr int RIGHT_FACE_RIGHT = 6;
constexpr int RIGHT_FACE_DOWN = 7;
constexpr int RIGHT_FACE_LEFT = 8;
constexpr int LEFT_TRIGGER_1 = 9;
constexpr int LEFT_TRIGGER_2 = 10;
constexpr int RIGHT_TRIGGER_1 = 11;
constexpr int RIGHT_TRIGGER_2 = 12;
constexpr int MIDDLE_LEFT = 13;
constexpr int MIDDLE = 14;
constexpr int MIDDLE_RIGHT = 15;
constexpr int LEFT_THUMB = 16;
constexpr int RIGHT_THUMB = 17;
} // namespace gamepad_buttons

// Gamepad axis codes — match raylib values
namespace gamepad_axes {
constexpr int LEFT_X = 0;
constexpr int LEFT_Y = 1;
constexpr int RIGHT_X = 2;
constexpr int RIGHT_Y = 3;
constexpr int LEFT_TRIGGER = 4;
constexpr int RIGHT_TRIGGER = 5;
} // namespace gamepad_axes

namespace detail {

inline std::string to_upper(const std::string &s) {
  std::string result = s;
  for (char &c : result) {
    if (c >= 'a' && c <= 'z')
      c -= 32;
  }
  return result;
}

} // namespace detail

// Map string name to key code (case-insensitive)
// Accepts: "A", "a", "ENTER", "enter", "CTRL", etc.
inline int key_from_name(const std::string &name) {
  // Handle single-char symbols directly (before uppercasing)
  if (name.length() == 1) {
    char c = name[0];
    if (c >= 'a' && c <= 'z')
      return keys::A + (c - 'a');
    if (c >= 'A' && c <= 'Z')
      return keys::A + (c - 'A');
    if (c >= '0' && c <= '9')
      return keys::ZERO + (c - '0');
    // Punctuation symbols
    switch (c) {
    case ' ':
      return keys::SPACE;
    case '\'':
      return keys::APOSTROPHE;
    case ',':
      return keys::COMMA;
    case '-':
      return keys::MINUS;
    case '.':
      return keys::PERIOD;
    case '/':
      return keys::SLASH;
    case ';':
      return keys::SEMICOLON;
    case '=':
      return keys::EQUAL;
    case '[':
      return keys::LEFT_BRACKET;
    case ']':
      return keys::RIGHT_BRACKET;
    case '\\':
      return keys::BACKSLASH;
    case '`':
      return keys::GRAVE;
    }
  }

  // Normalize to uppercase for consistent lookup
  std::string key = detail::to_upper(name);

  // clang-format off
  static constexpr std::array<std::pair<std::string_view, int>, 85> key_map = {{
      // Function keys
      {"F1", keys::F1}, {"F2", keys::F2}, {"F3", keys::F3}, {"F4", keys::F4},
      {"F5", keys::F5}, {"F6", keys::F6}, {"F7", keys::F7}, {"F8", keys::F8},
      {"F9", keys::F9}, {"F10", keys::F10}, {"F11", keys::F11}, {"F12", keys::F12},

      // Navigation
      {"UP", keys::UP}, {"DOWN", keys::DOWN}, {"LEFT", keys::LEFT}, {"RIGHT", keys::RIGHT},
      {"HOME", keys::HOME}, {"END", keys::END}, {"INSERT", keys::INSERT},
      {"PAGE_UP", keys::PAGE_UP}, {"PAGEUP", keys::PAGE_UP},
      {"PAGE_DOWN", keys::PAGE_DOWN}, {"PAGEDOWN", keys::PAGE_DOWN},

      // Editing
      {"ENTER", keys::ENTER}, {"RETURN", keys::ENTER},
      {"TAB", keys::TAB},
      {"BACKSPACE", keys::BACKSPACE},
      {"DELETE", keys::DELETE_KEY}, {"DEL", keys::DELETE_KEY},
      {"SPACE", keys::SPACE},
      {"ESCAPE", keys::ESCAPE}, {"ESC", keys::ESCAPE},

      // Modifiers
      {"SHIFT", keys::LEFT_SHIFT}, {"LSHIFT", keys::LEFT_SHIFT}, {"LEFT_SHIFT", keys::LEFT_SHIFT},
      {"RSHIFT", keys::RIGHT_SHIFT}, {"RIGHT_SHIFT", keys::RIGHT_SHIFT},
      {"CTRL", keys::LEFT_CONTROL}, {"CONTROL", keys::LEFT_CONTROL},
      {"LCTRL", keys::LEFT_CONTROL}, {"LEFT_CONTROL", keys::LEFT_CONTROL},
      {"RCTRL", keys::RIGHT_CONTROL}, {"RIGHT_CONTROL", keys::RIGHT_CONTROL},
      {"ALT", keys::LEFT_ALT}, {"LALT", keys::LEFT_ALT}, {"LEFT_ALT", keys::LEFT_ALT},
      {"RALT", keys::RIGHT_ALT}, {"RIGHT_ALT", keys::RIGHT_ALT},
      {"SUPER", keys::LEFT_SUPER}, {"WIN", keys::LEFT_SUPER}, {"CMD", keys::LEFT_SUPER},

      // Punctuation (named)
      {"APOSTROPHE", keys::APOSTROPHE}, {"COMMA", keys::COMMA},
      {"MINUS", keys::MINUS}, {"PERIOD", keys::PERIOD}, {"SLASH", keys::SLASH},
      {"SEMICOLON", keys::SEMICOLON}, {"EQUAL", keys::EQUAL},
      {"LEFT_BRACKET", keys::LEFT_BRACKET}, {"RIGHT_BRACKET", keys::RIGHT_BRACKET},
      {"BACKSLASH", keys::BACKSLASH}, {"GRAVE", keys::GRAVE},

      // Lock keys
      {"CAPS_LOCK", keys::CAPS_LOCK}, {"CAPSLOCK", keys::CAPS_LOCK},
      {"SCROLL_LOCK", keys::SCROLL_LOCK}, {"NUM_LOCK", keys::NUM_LOCK},
      {"PRINT_SCREEN", keys::PRINT_SCREEN}, {"PAUSE", keys::PAUSE},

      // Keypad
      {"KP_0", keys::KP_0}, {"KP_1", keys::KP_1}, {"KP_2", keys::KP_2},
      {"KP_3", keys::KP_3}, {"KP_4", keys::KP_4}, {"KP_5", keys::KP_5},
      {"KP_6", keys::KP_6}, {"KP_7", keys::KP_7}, {"KP_8", keys::KP_8},
      {"KP_9", keys::KP_9}, {"KP_DECIMAL", keys::KP_DECIMAL},
      {"KP_DIVIDE", keys::KP_DIVIDE}, {"KP_MULTIPLY", keys::KP_MULTIPLY},
      {"KP_SUBTRACT", keys::KP_SUBTRACT}, {"KP_ADD", keys::KP_ADD},
      {"KP_ENTER", keys::KP_ENTER}, {"KP_EQUAL", keys::KP_EQUAL},
  }};
  // clang-format on

  for (const auto &[name_sv, code] : key_map) {
    if (name_sv == key)
      return code;
  }
  return 0;
}

// Key combo with modifiers
struct KeyCombo {
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool super = false; // Win key / Cmd on Mac
  int key = 0;
};

// Parse a key combo string like "CTRL+SHIFT+A" or "ALT+F4"
inline KeyCombo parse_key_combo(const std::string &str) {
  KeyCombo combo;
  std::string s = str;

  static constexpr std::pair<const char *, bool KeyCombo::*> modifiers[] = {
      {"CTRL+", &KeyCombo::ctrl},
      {"ctrl+", &KeyCombo::ctrl},
      {"CMD+", &KeyCombo::ctrl}, // Mac convention: Cmd = Ctrl for shortcuts
      {"cmd+", &KeyCombo::ctrl},
      {"SHIFT+", &KeyCombo::shift},
      {"shift+", &KeyCombo::shift},
      {"ALT+", &KeyCombo::alt},
      {"alt+", &KeyCombo::alt},
      {"OPTION+", &KeyCombo::alt}, // Mac name for Alt
      {"option+", &KeyCombo::alt},
      {"SUPER+", &KeyCombo::super},
      {"super+", &KeyCombo::super},
      {"WIN+", &KeyCombo::super},
      {"win+", &KeyCombo::super},
      {"META+", &KeyCombo::super},
      {"meta+", &KeyCombo::super},
  };

  bool found = true;
  while (found) {
    found = false;
    for (const auto &[prefix, member] : modifiers) {
      if (s.find(prefix) == 0) {
        combo.*member = true;
        s = s.substr(std::strlen(prefix));
        found = true;
        break;
      }
    }
  }

  combo.key = key_from_name(s);
  return combo;
}

} // namespace afterhours
