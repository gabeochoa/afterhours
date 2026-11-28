#include <cstdio>
#include <string>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/text_input/text_input.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Notes : System<DefaultUIContext> {
  std::string editable_text = "edit me";
  std::string readonly_text = "read only";
  std::string disabled_text = "cannot touch";
  bool readonly_flag = false;
  bool disabled_flag = false;
  bool editable_flag = true;
  bool disabled_focused = true;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto editable = text_area(
        ctx, mk(root, 0), editable_text,
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(60)})
            .with_line_height(pixels(20)));
    auto readonly_area = text_area(
        ctx, mk(root, 1), readonly_text,
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(60)})
            .with_line_height(pixels(20))
            .with_readonly());
    auto disabled_area = text_area(
        ctx, mk(root, 2), disabled_text,
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(60)})
            .with_line_height(pixels(20))
            .with_disabled(true));
    namespace ti = afterhours::text_input;
    if (readonly_area.ent().has<ti::HasTextAreaState>()) {
      auto &s = readonly_area.ent().get<ti::HasTextAreaState>();
      readonly_flag = s.readonly && !s.disabled;
    }
    if (disabled_area.ent().has<ti::HasTextAreaState>()) {
      auto &s = disabled_area.ent().get<ti::HasTextAreaState>();
      disabled_flag = s.disabled;
      disabled_focused = s.is_focused;
    }
    if (editable.ent().has<ti::HasTextAreaState>()) {
      auto &s = editable.ent().get<ti::HasTextAreaState>();
      editable_flag = !s.readonly && !s.disabled;
    }
  }
};

int main() {
  Checks checks;
  HeadlessUI<Notes> app(400, 260, std::make_unique<Notes>());
  if (!app.ok())
    return 1;
  app.run(3);
  auto &screen = app.screen();
  std::printf("editable ok=%d, readonly ok=%d, disabled ok=%d focused=%d\n",
              screen.editable_flag, screen.readonly_flag, screen.disabled_flag,
              screen.disabled_focused);
  std::printf("readonly text: %s, disabled text: %s\n",
              screen.readonly_text.c_str(), screen.disabled_text.c_str());
  checks.check(screen.editable_flag, "editable area stays editable");
  checks.check(screen.readonly_flag, "readonly area is readonly");
  checks.check(screen.disabled_flag, "disabled area is disabled");
  checks.check(!screen.disabled_focused, "disabled area never takes focus");
  checks.check(screen.readonly_text == "read only", "readonly text unchanged");
  checks.check(screen.disabled_text == "cannot touch",
               "disabled text unchanged");
  return checks.finish("text_area_readonly");
}
