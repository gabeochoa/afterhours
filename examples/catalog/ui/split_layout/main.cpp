// split_layout — imm::vsplit / imm::hsplit
//
// Divide a region into N parts and get all N back at once, so an app skeleton
// is one statement per axis instead of N nested divs. N is deduced from the
// size list, so there is no count to keep in sync.
//
// This builds the classic three-band shell:
//
//   +--------------------------------------------------+
//   | title bar                        (30px)           |
//   +----------------+---------------------------------+
//   | nav (200px)    | content (expand)                 |
//   |                |                                  |
//   +----------------+---------------------------------+
//   | status bar                       (24px)           |
//   +--------------------------------------------------+
//
// Runs headless for one frame and prints the resolved rects, so the layout is
// verifiable without a window. Build: make (from this directory).

#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../shared/vector.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Boilerplate, not content: the nav/text/debug systems in the post-update
// bridge reference every one of these by name, so the enum has to spell them
// all out before anything compiles. See todo.md item 5 ("ui::DefaultAction") —
// shipping this vocabulary with the library deletes this whole block.
enum struct InputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetLeft,
  WidgetRight,
  WidgetUp,
  WidgetDown,
  MenuBack,
  TextBackspace,
  TextCopy,
  TextCut,
  TextDelete,
  TextDeleteWordBack,
  TextDeleteWordForward,
  TextEnd,
  TextHome,
  TextPaste,
  TextRedo,
  TextSelectAll,
  TextSelectLeft,
  TextSelectRight,
  TextUndo,
  TextWordLeft,
  TextWordRight,
};

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;

// The build pass runs before layout, so stash the ids and read the resolved
// rects afterwards (see PrintRects below).
struct {
  EntityID title = -1, nav = -1, content = -1, status = -1;
} g_regions;

struct BuildShell : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &entity, UIContext<InputAction> &ctx,
                             float) override {
    // Three horizontal bands. expand(1) soaks up whatever the fixed bands
    // leave behind, so the body always fills the gap.
    auto [title, body, status] =
        vsplit(ctx, mk(entity), {pixels(30), expand(1.f), pixels(24)});

    // Split the middle band again, this time along the other axis. Nested
    // splits divide only their own region.
    auto [nav, content] =
        hsplit(ctx, mk(body.ent()), {pixels(200), expand(1.f)});

    // Regions are ordinary containers: widgets parent into them normally, and
    // a bare string is a label-only config.
    div(ctx, mk(title.ent()), "afterhours");
    button(ctx, mk(nav.ent(), 0), "Open");
    button(ctx, mk(nav.ent(), 1), "Save");
    div(ctx, mk(content.ent()), "content goes here");
    div(ctx, mk(status.ent()), "ready");

    g_regions = {title.id(), nav.id(), content.id(), status.id()};
  }
};

// Runs after the post-update bridge, which is where RunAutoLayout lives.
struct PrintRects : System<> {
  bool should_iterate() const override { return false; }

  static Rectangle rect_of(EntityID id) {
    return UICollectionHolder::getEntityForIDEnforce(id)
        .get<UIComponent>()
        .rect();
  }

  static void show(const char *name, EntityID id) {
    Rectangle r = rect_of(id);
    printf("  %-8s x=%6.1f y=%6.1f w=%6.1f h=%6.1f\n", name, r.x, r.y, r.width,
           r.height);
  }

  virtual void once(float) override {
    printf("split_layout — resolved rects @ %dx%d\n\n", SCREEN_W, SCREEN_H);
    show("title", g_regions.title);
    show("nav", g_regions.nav);
    show("content", g_regions.content);
    show("status", g_regions.status);

    // The three bands tile the screen exactly: 30 + 666 + 24 = 720.
    float banded = rect_of(g_regions.title).height +
                   rect_of(g_regions.content).height +
                   rect_of(g_regions.status).height;
    printf("\n  bands sum to %.1f (screen height %d)\n", banded, SCREEN_H);

    // nav and content tile the middle band across: 200 + 1080 = 1280.
    float across =
        rect_of(g_regions.nav).width + rect_of(g_regions.content).width;
    printf("  nav + content = %.1f (screen width %d)\n", across, SCREEN_W);
  }
};

int main(int, char **) {
  // Creates the UI root plus every singleton the plugin needs (UIContext,
  // FontManager, TextMeasureCache, UIEntityMappingCache, AutoLayoutRoot, ...).
  Entity &ui_root = ui::init_ui_plugin<InputAction>();

  // RunAutoLayout reads the resolution from this singleton; the app owns it.
  ui_root.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{SCREEN_W, SCREEN_H});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      ui_root);

  SystemManager systems;
  ui::register_before_ui_updates<InputAction>(systems);
  systems.register_update_system(std::make_unique<BuildShell>());
  ui::register_after_ui_updates<InputAction>(systems); // RunAutoLayout is here
  systems.register_update_system(std::make_unique<PrintRects>());

  systems.run(1.f);
  return 0;
}
