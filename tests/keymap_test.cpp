// keymap_test.cpp
// Covers ui::default_keymap<InputAction>() -- the conventional key bindings for
// the UI's own actions, matched to the caller's enum by name.
//
// The failure this exists to catch: a DefaultAction value that no binding
// reaches. Such an action is silently dead -- the widget systems query it every
// frame and it never fires, with nothing in the build or at runtime to say so.
// That exact bug shipped in the consuming app, where a menu input layer defined
// WidgetNext but not WidgetMod, so Shift+Tab could not move focus backward.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/utilities.h>

using namespace afterhours;
using namespace afterhours::ui;

using KeyChord = input::KeyChord;

// Bound key codes for `action`, ignoring gamepad entries. Empty = unbound.
template <typename E>
static std::vector<int> keys_for(const input::ProvidesInputMapping::GameMapping &m,
                                 E action) {
  std::vector<int> out;
  auto it = m.find(static_cast<int>(action));
  if (it == m.end())
    return out;
  for (const auto &any : it->second)
    if (const auto *chord = std::get_if<KeyChord>(&any))
      out.push_back(chord->key);
  return out;
}

static bool has_key(const std::vector<int> &keys, int key) {
  return std::find(keys.begin(), keys.end(), key) != keys.end();
}

// Modifier mask required for `key` under `action` (-1 if that key isn't bound).
template <typename E>
static int mods_for(const input::ProvidesInputMapping::GameMapping &m, E action,
                    int key) {
  auto it = m.find(static_cast<int>(action));
  if (it == m.end())
    return -1;
  for (const auto &any : it->second)
    if (const auto *chord = std::get_if<KeyChord>(&any); chord && chord->key == key)
      return static_cast<int>(chord->required_modifiers);
  return -1;
}

// The guard that matters: no DefaultAction may go unbound. Anything added to
// the enum without a binding here is an action the UI can never trigger.
TEST(every_default_action_is_bound) {
  auto m = ui::default_keymap<DefaultAction>();
  for (auto action : magic_enum::enum_values<DefaultAction>()) {
    if (action == DefaultAction::None)
      continue; // None is the "unmapped" sentinel and must stay unbound.
    bool bound = m.contains(static_cast<int>(action));
    if (!bound)
      fprintf(stderr, "        unbound: %s\n",
              std::string(magic_enum::enum_name(action)).c_str());
    CHECK(bound);
  }
  CHECK(!m.contains(static_cast<int>(DefaultAction::None)));
}

TEST(focus_navigation_bindings) {
  auto m = ui::default_keymap<DefaultAction>();
  CHECK(has_key(keys_for(m, DefaultAction::WidgetNext), keys::TAB));
  // WidgetMod is what turns Tab into Shift+Tab. Binding WidgetNext without it
  // is the real-world bug named at the top of this file.
  CHECK(has_key(keys_for(m, DefaultAction::WidgetMod), keys::LEFT_SHIFT));
  CHECK(has_key(keys_for(m, DefaultAction::WidgetMod), keys::RIGHT_SHIFT));
  CHECK(has_key(keys_for(m, DefaultAction::WidgetPress), keys::ENTER));
  CHECK(has_key(keys_for(m, DefaultAction::WidgetPress), keys::SPACE));
  CHECK(has_key(keys_for(m, DefaultAction::WidgetUp), keys::UP));
  CHECK(has_key(keys_for(m, DefaultAction::WidgetDown), keys::DOWN));
  CHECK(has_key(keys_for(m, DefaultAction::MenuBack), keys::ESCAPE));
}

TEST(editing_chords_cover_both_cmd_and_ctrl) {
  auto m = ui::default_keymap<DefaultAction>();
  CHECK(mods_for(m, DefaultAction::TextCopy, keys::C) != -1);
  // Two entries for one key: cmd (macOS) and ctrl (elsewhere).
  auto copy = keys_for(m, DefaultAction::TextCopy);
  CHECK(copy.size() == 2);
  int mask = 0;
  for (const auto &any : m.at(static_cast<int>(DefaultAction::TextCopy)))
    mask |= std::get<KeyChord>(any).required_modifiers;
  CHECK(mask == (KeyChord::MOD_SUPER | KeyChord::MOD_CTRL));

  // Plain backspace edits text; the same key is also WidgetBack, which the text
  // systems only shadow while a field holds focus.
  CHECK(mods_for(m, DefaultAction::TextBackspace, keys::BACKSPACE) == 0);
  CHECK(has_key(keys_for(m, DefaultAction::WidgetBack), keys::BACKSPACE));

  // Redo is the shifted form of undo, so the two must not collide.
  CHECK(mods_for(m, DefaultAction::TextUndo, keys::Z) == KeyChord::MOD_SUPER);
  CHECK(mods_for(m, DefaultAction::TextRedo, keys::Z) ==
        (KeyChord::MOD_SUPER | KeyChord::MOD_SHIFT));
}

// An app enum that shares only some names with DefaultAction, plus one of its
// own. Bindings are matched by name, so it should get exactly the overlap.
enum struct AppAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  Jump, // not a UI action -- must stay unbound
};

TEST(matches_a_custom_enum_by_name) {
  auto m = ui::default_keymap<AppAction>();
  CHECK(has_key(keys_for(m, AppAction::WidgetNext), keys::TAB));
  CHECK(has_key(keys_for(m, AppAction::WidgetPress), keys::ENTER));

  // Names this enum lacks are skipped rather than landing on whatever integer
  // they would have had in DefaultAction -- the failure mode of index-based
  // matching would be binding Jump to a text-editing chord.
  CHECK(!m.contains(static_cast<int>(AppAction::Jump)));
  CHECK(m.size() == 4);
}

int main() { return ui_test::run_registered_tests("keymap tests"); }
