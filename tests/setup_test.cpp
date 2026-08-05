// setup_test.cpp
// Covers ui::setup() and ui::DefaultAction.
//
// setup() collapses init_ui_plugin + the resolution singleton +
// register_before_ui_updates + user systems + register_after_ui_updates into
// one call. The failure it exists to prevent is silent: get the system order
// wrong and the UI still "runs", it just never lays out. So the test asserts
// that a widget built through setup() has a real resolved rect after one frame.
//
// setup() creates permanent singletons, so it can only be called once per
// process — everything here shares the single call in main().

#define FMT_HEADER_ONLY
#include <fmt/format.h>

// ui.h FIRST, deliberately: it is what apps include, and a header beneath it
// that forgets an include only fails when nothing else pulled that include in
// first. Ordering autolayout.h ahead of it hid exactly that bug once.
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cstdio>
#include <cmath>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(e) check((e), #e, __LINE__)
#define CHECK_APPROX(a, b)                                                     \
  check(std::fabs((a) - (b)) < 1.5f, #a " ~= " #b, __LINE__)

// Every action name the UI plugin looks up. If a system starts referencing a
// new one, or someone trims DefaultAction, this list is the tripwire — apps
// relying on DefaultAction would otherwise fail to compile with no hint that
// the default set was the thing that regressed.
static constexpr const char *REQUIRED_ACTIONS[] = {
    "None",          "WidgetMod",      "WidgetNext",
    "WidgetBack",    "WidgetPress",    "WidgetLeft",
    "WidgetRight",   "WidgetUp",       "WidgetDown",
    "MenuBack",      "TextBackspace",  "TextCopy",
    "TextCut",       "TextDelete",     "TextDeleteWordBack",
    "TextDeleteWordForward", "TextEnd", "TextHome",
    "TextPaste",     "TextRedo",       "TextSelectAll",
    "TextSelectLeft", "TextSelectRight", "TextUndo",
    "TextWordLeft",  "TextWordRight",
};

static EntityID g_button_id = -1;
static EntityID g_panel_id = -1;
static int g_build_calls = 0;

struct BuildUI : System<DefaultUIContext> {
  virtual void for_each_with(Entity &entity, DefaultUIContext &ctx,
                             float) override {
    g_build_calls++;
    auto [left, right] =
        hsplit(ctx, mk(entity), {pixels(200), expand(1.f)});
    auto b = button(ctx, mk(left.ent(), 0), "Click Me");
    g_button_id = b.id();
    g_panel_id = right.id();
  }
};

static Rectangle rect_of(EntityID id) {
  return UICollectionHolder::getEntityForIDEnforce(id)
      .get<UIComponent>()
      .rect();
}

int main() {
  printf("=== ui::setup / DefaultAction ===\n\n");

  // DefaultAction carries the full vocabulary the plugin references.
  for (const char *name : REQUIRED_ACTIONS) {
    bool present = magic_enum::enum_cast<DefaultAction>(name).has_value();
    if (!present)
      fprintf(stderr, "  FAIL: DefaultAction missing '%s'\n", name);
    check(present, "DefaultAction has required action", __LINE__);
  }
  CHECK(magic_enum::enum_count<DefaultAction>() ==
        std::size(REQUIRED_ACTIONS));

  // One call: root, singletons, and the update systems in working order.
  SystemManager systems;
  ui::setup_with_resolution<>(systems, {800, 600},
                              std::make_unique<BuildUI>());

  // setup() supplies the resolution singleton that RunAutoLayout needs and
  // that init_ui_plugin does not create.
  CHECK(
      EntityHelper::has_singleton<window_manager::ProvidesCurrentResolution>());

  systems.run(1.f);

  CHECK(g_build_calls == 1);
  CHECK(g_button_id != -1);

  // The real assertion: layout actually ran. A mis-ordered registration leaves
  // every rect at zero while everything else still looks fine.
  if (g_button_id != -1) {
    Rectangle r = rect_of(g_button_id);
    CHECK(r.width > 0.f);
    CHECK(r.height > 0.f);
  }

  // And it ran against the resolution setup() was given, not a default.
  if (g_panel_id != -1) {
    Rectangle panel = rect_of(g_panel_id);
    CHECK_APPROX(panel.width, 600.f); // 800 - 200 sidebar
    CHECK_APPROX(panel.height, 600.f);
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
