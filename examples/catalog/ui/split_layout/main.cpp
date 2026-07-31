// split_layout — imm::vsplit / imm::hsplit
//
// Divide a region into N parts and get all N back at once, so an app skeleton
// is one statement per axis instead of N nested divs. N is deduced from the
// size list.
//
//   +--------------------------------------------------+
//   | title bar                        (36px)           |
//   +----------------+---------------------------------+
//   | nav (200px)    | content (expand)                 |
//   +----------------+---------------------------------+
//   | status bar                       (28px)           |
//   +--------------------------------------------------+
//
// Also the "how short can setup get" showcase: no InputAction enum, no
// singleton wiring, no system-ordering ritual.
//
// Renders to a hidden window and writes split_layout.png, then prints the
// resolved rects. Build: make (from this directory).

#include <cstdio>
#include <filesystem>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/autolayout.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;

// The build pass runs before layout, so stash ids and read rects afterwards.
struct {
  EntityID title = -1, nav = -1, content = -1, status = -1;
} g_regions;

// DefaultUIContext is UIContext<DefaultAction> — the library ships the input
// vocabulary its widget systems need, so this app declares no enum.
struct BuildShell : System<DefaultUIContext> {
  virtual void for_each_with(Entity &entity, DefaultUIContext &ctx,
                             float) override {
    ctx.theme.roundness = 0.f;

    // expand(1) soaks up whatever the fixed bands leave behind.
    auto [title, body, status] =
        vsplit(ctx, mk(entity), {pixels(36), expand(1.f), pixels(28)});

    // Nested splits divide only their own region.
    auto [nav, content] =
        hsplit(ctx, mk(body.ent()), {pixels(200), expand(1.f)});

    title.ent().get<HasColor>().set(Color{38, 42, 58, 255});
    nav.ent().get<HasColor>().set(Color{28, 31, 44, 255});
    content.ent().get<HasColor>().set(Color{22, 24, 34, 255});
    status.ent().get<HasColor>().set(Color{38, 42, 58, 255});

    // Regions are ordinary containers; a bare string is a label-only config.
    const auto label = [](const char *text, float size) {
      return ComponentConfig{text}
          .with_font(UIComponent::DEFAULT_FONT, pixels(size))
          .with_size(ComponentSize{percent(1.f), percent(1.f)})
          .with_padding(Padding{.left = pixels(12)})
          .with_align_items(AlignItems::Center);
    };

    div(ctx, mk(title.ent()), label("afterhours", 20.f));
    button(ctx, mk(nav.ent(), 0),
           ComponentConfig{"Open"}
               .with_font(UIComponent::DEFAULT_FONT, pixels(18))
               .with_size(ComponentSize{percent(1.f), pixels(40)}));
    button(ctx, mk(nav.ent(), 1),
           ComponentConfig{"Save"}
               .with_font(UIComponent::DEFAULT_FONT, pixels(18))
               .with_size(ComponentSize{percent(1.f), pixels(40)}));
    div(ctx, mk(content.ent()), label("content goes here", 22.f));
    div(ctx, mk(status.ent()), label("ready", 16.f));

    g_regions = {title.id(), nav.id(), content.id(), status.id()};
  }
};

static Rectangle rect_of(EntityID id) {
  return UICollectionHolder::getEntityForIDEnforce(id)
      .get<UIComponent>()
      .rect();
}

static void print_rects() {
  auto show = [](const char *name, EntityID id) {
    Rectangle r = rect_of(id);
    printf("  %-8s x=%6.1f y=%6.1f w=%6.1f h=%6.1f\n", name, r.x, r.y, r.width,
           r.height);
  };
  printf("\nresolved rects @ %dx%d\n\n", SCREEN_W, SCREEN_H);
  show("title", g_regions.title);
  show("nav", g_regions.nav);
  show("content", g_regions.content);
  show("status", g_regions.status);

  printf("\n  bands sum to %.1f (screen height %d)\n",
         rect_of(g_regions.title).height + rect_of(g_regions.content).height +
             rect_of(g_regions.status).height,
         SCREEN_H);
  printf("  nav + content = %.1f (screen width %d)\n",
         rect_of(g_regions.nav).width + rect_of(g_regions.content).width,
         SCREEN_W);
}

int main(int, char **) {
  graphics::Config cfg{};
  cfg.display = graphics::DisplayMode::Windowed;
  cfg.width = SCREEN_W;
  cfg.height = SCREEN_H;
  cfg.target_fps = 60;
  cfg.config_flags = raylib::FLAG_WINDOW_HIDDEN;
  cfg.enable_msaa = false;
  if (!graphics::init(cfg)) {
    fprintf(stderr, "FATAL: windowed backend init failed\n");
    return 1;
  }

  // The bundled debug-overlay render system reads inputs; give it an empty
  // collector so it does not hit an empty optional.
  {
    auto &in = EntityHelper::createEntity();
    in.addComponent<input::InputCollector>();
    EntityHelper::registerSingleton<input::InputCollector>(in);
  }

  SystemManager systems;
  // Creates the UI root and every singleton, then registers the pre-bridge,
  // BuildShell, and the post-bridge in the only order that works. Must run
  // after graphics::init — loading fonts needs a GL context.
  ui::setup_with_resolution<>(systems, {SCREEN_W, SCREEN_H},
                              std::make_unique<BuildShell>());
  ui::register_render_systems<DefaultAction>(systems);

  graphics::begin_frame();
  raylib::ClearBackground(raylib::Color{18, 19, 26, 255});
  systems.run(1.f / 60.f);
  graphics::end_frame();

  std::filesystem::path out = "split_layout.png";
  bool ok = graphics::capture_frame(out);
  printf("screenshot: %s -> %s\n", ok ? "OK" : "FAILED", out.c_str());

  print_rects();
  graphics::shutdown();
  return ok ? 0 : 1;
}
