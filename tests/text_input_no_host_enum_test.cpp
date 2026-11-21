// text_input_no_host_enum_test.cpp
// A host InputAction with NO Text* enumerators still compiles and edits:
// text_action_pressed falls back to the library default chords. Before, the
// widget referenced InputAction::TextBackspace & friends directly, so this
// enum was a compile error and the host had to pre-declare every name.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/ui/text_input/component.h>

#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;

// Widget actions only -- exactly what a host writing its own enum from the
// widget names ends up with. No TextBackspace, TextCopy, TextPaste, ...
enum struct BareAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetUp,
  WidgetDown,
  WidgetLeft,
  WidgetRight,
  MenuBack,
};

namespace {

struct BareHarness {
  EntityCollection &coll = UICollectionHolder::get().collection;
  UIContext<BareAction> *ctx = nullptr;
  Entity *root_entity = nullptr;

  BareHarness() {
    imm::clear_existing_ui_elements();
    if (!EntityHelper::has_singleton<TextMeasureCache>()) {
      Entity &tm = EntityHelper::createPermanentEntity();
      auto &cache = tm.addComponent<TextMeasureCache>();
      cache.set_measure_function([](std::string_view text, std::string_view,
                                    float font_size, float) {
        return Vector2Type{static_cast<float>(text.size()) * font_size * 0.5f,
                           font_size};
      });
      EntityHelper::registerSingleton<TextMeasureCache>(tm);
    }
    if (!EntityHelper::has_singleton<
            window_manager::ProvidesCurrentResolution>()) {
      Entity &res = EntityHelper::createPermanentEntity();
      res.addComponent<window_manager::ProvidesCurrentResolution>(
          window_manager::Resolution{800, 600});
      EntityHelper::registerSingleton<
          window_manager::ProvidesCurrentResolution>(res);
    }
    Entity *ctx_entity = &coll.createEntity();
    ctx = &ctx_entity->addComponent<UIContext<BareAction>>();
    ctx->screen_width = 800;
    ctx->screen_height = 600;
    root_entity = &coll.createEntity();
    root_entity->addComponent<UIComponent>(root_entity->id);
    root_entity->addComponent<AutoLayoutRoot>();
    auto &root_cmp = root_entity->get<UIComponent>();
    root_cmp.set_desired_width(pixels(800));
    root_cmp.set_desired_height(pixels(600));
  }
  ~BareHarness() {
    for (const auto &e : coll.get_entities())
      if (e)
        e->cleanup = true;
    coll.cleanup();
    imm::clear_existing_ui_elements();
  }
  void begin_frame() {
    coll.merge_entity_arrays();
    for (const auto &e : coll.get_entities())
      if (e && e->has<UIComponent>())
        e->get<UIComponent>().children.clear();
  }
  void layout_only() {
    coll.merge_entity_arrays();
    auto &entities = coll.get_entities();
    EntityID max_id = 0;
    for (const auto &e : entities)
      if (e)
        max_id = std::max(max_id, e->id);
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (const auto &e : entities)
      if (e)
        mapping[static_cast<size_t>(e->id)] = e.get();
    AutoLayout::autolayout(root_entity->get<UIComponent>(), {800, 600},
                           mapping, false, 1.0f);
  }
};

void hold_mods(uint8_t mods, bool down) {
  using KC = input::KeyChord;
  const std::pair<uint8_t, int> mod_keys[] = {
      {KC::MOD_SHIFT, keys::LEFT_SHIFT},
      {KC::MOD_CTRL, keys::LEFT_CONTROL},
      {KC::MOD_ALT, keys::LEFT_ALT},
      {KC::MOD_SUPER, keys::LEFT_SUPER},
  };
  for (auto [bit, key] : mod_keys) {
    if (!(mods & bit)) continue;
    if (down)
      input_injector::set_key_held(key);
    else
      input_injector::set_key_up(key);
  }
}

// Press a chord for one frame: hold its modifiers, tap its key.
void press_chord(input::KeyChord chord) {
  input_injector::reset_all();
  hold_mods(chord.required_modifiers, true);
  input_injector::set_key_down(chord.key);
  input_injector::reset_frame();
}

} // namespace

TEST(bare_enum_text_input_edits_and_copies_via_defaults) {
  clipboard::MemoryProvider memory;
  clipboard::ScopedProvider scope(memory);
  BareHarness h;
  std::string value = "hello";
  auto emit = [&]() -> Entity & {
    h.begin_frame();
    auto result = imm::text_input(
        *h.ctx, mk(*h.root_entity, 0), value,
        ComponentConfig{}.with_size({pixels(300), pixels(100)}));
    h.layout_only();
    return result.ent();
  };
  auto &entity = emit();
  h.ctx->set_focus(entity.id);
  emit();

  // Gaining focus selects all; End (default chord) collapses to the caret
  // at the end, then Backspace deletes the last character.
  press_chord({keys::END, 0});
  emit();
  input_injector::reset_all();
  press_chord({keys::BACKSPACE, 0});
  emit();
  input_injector::reset_all();
  CHECK(value == "hell");

  // Select all + copy via the primary-modifier defaults.
#if defined(__APPLE__)
  constexpr uint8_t PRIMARY = input::KeyChord::MOD_SUPER;
#else
  constexpr uint8_t PRIMARY = input::KeyChord::MOD_CTRL;
#endif
  press_chord({keys::A, PRIMARY});
  emit();
  input_injector::reset_all();
  press_chord({keys::C, PRIMARY});
  emit();
  input_injector::reset_all();
  CHECK(clipboard::get_text() == "hell");

  // Paste via the default chord replaces the still-selected text.
  clipboard::set_text("xy");
  press_chord({keys::V, PRIMARY});
  emit();
  input_injector::reset_all();
  CHECK(value == "xy");
}

int main() { return ui_test::run_registered_tests("text input no host enum"); }
