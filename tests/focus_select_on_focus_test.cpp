#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/component.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
Entity *find_field_entity() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "text_input_field")
      return e.get();
  }
  return nullptr;
}

void two_frames(ImmTestHarness &h, const std::function<void()> &emit) {
  for (int i = 0; i < 2; i++) {
    h.begin_frame();
    emit();
    h.layout_only();
  }
}
} // namespace

TEST(select_on_focus_fires_for_keyboard_but_not_pointer_focus) {
  ImmTestHarness h;
  auto &ctx = h.context();
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(
      h.context_entity());
  int clicks_a = 0, clicks_b = 0;
  h.begin_frame();
  auto a = div(ctx, mk(h.root(), 0),
               ComponentConfig{}
                   .with_size({pixels(100), pixels(40)})
                   .with_select_on_focus(true));
  a.ent().addComponentIfMissing<HasClickListener>(
      [&clicks_a](Entity &) { clicks_a++; });
  auto b = div(ctx, mk(h.root(), 1),
               ComponentConfig{}
                   .with_size({pixels(100), pixels(40)})
                   .with_select_on_focus(true));
  b.ent().addComponentIfMissing<HasClickListener>(
      [&clicks_b](Entity &) { clicks_b++; });
  h.layout_only();
  CHECK(a.ent().has<SelectOnFocus>());
  CHECK(b.ent().has<SelectOnFocus>());

  HandleSelectOnFocus<ui_test::TestInputAction> sys;
  sys.once(0.f);
  ctx.set_focus(a.ent().id, FocusSource::Explicit);
  sys.for_each_with(a.ent(), a.ent().get<UIComponent>(),
                    a.ent().get<SelectOnFocus>(),
                    a.ent().get<HasClickListener>(), 0.f);
  CHECK(clicks_a == 1);

  ctx.set_focus(b.ent().id, FocusSource::Pointer);
  sys.for_each_with(b.ent(), b.ent().get<UIComponent>(),
                    b.ent().get<SelectOnFocus>(),
                    b.ent().get<HasClickListener>(), 0.f);
  CHECK(clicks_b == 0);
}

TEST(text_input_selects_all_only_for_non_pointer_focus) {
  ImmTestHarness h;
  std::string text = "hello";
  Entity *outer = nullptr;
  auto emit = [&] {
    outer = &imm::text_input(h.context(), mk(h.root(), 0), text,
                             ComponentConfig{}
                                 .with_size({pixels(220), pixels(40)})
                                 .with_select_on_focus(true))
                 .ent();
  };
  two_frames(h, emit);
  Entity *field = find_field_entity();
  CHECK(field != nullptr);
  if (!field || !outer)
    return;
  auto &state = outer->get<text_input::HasTextInputState>();
  CHECK(!field->has<SelectOnFocus>());

  h.context().set_focus(field->id, FocusSource::Pointer);
  two_frames(h, emit);
  CHECK(!state.has_selection());

  h.context().set_focus(h.context().ROOT, FocusSource::Explicit);
  two_frames(h, emit);
  h.context().set_focus(field->id, FocusSource::Explicit);
  two_frames(h, emit);
  CHECK(state.has_selection());
  if (!state.has_selection())
    return;
  CHECK(state.selection_start() == 0);
  CHECK(state.selection_end() == state.text_size());
  CHECK(text_input::delete_selection(state));
  CHECK(text_input::insert_char(state, 'X'));
  CHECK(state.text() == std::string("X"));
}

int main() {
  return ui_test::run_registered_tests("focus select-on-focus");
}
