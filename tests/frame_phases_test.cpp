#ifndef AFTER_HOURS_USE_NONE
#define AFTER_HOURS_USE_NONE 1
#endif
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>
#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(e) check((e), #e, __LINE__)

static EntityID g_button_id = -1;
static EntityID g_panel_id = -1;

struct BuildUI : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    auto [left, right] = hsplit(ctx, mk(entity), {pixels(200), expand(1.f)});
    auto b = button(ctx, mk(left.ent(), 0), "Click Me");
    g_button_id = b.id();
    g_panel_id = right.id();
  }
};

static float g_width_after_layout = 0.f;
static size_t g_draws_after_layout = 999;
struct BetweenLayoutAndInput : System<> {
  void once(float) override {
    if (g_button_id == -1) return;
    g_width_after_layout = UICollectionHolder::getEntityForIDEnforce(g_button_id)
                               .get<UIComponent>().rect().width;
    g_draws_after_layout = draw_calls().size();
  }
};

static Rectangle rect_of(EntityID id) {
  return UICollectionHolder::getEntityForIDEnforce(id).get<UIComponent>().rect();
}

static std::string summarize() {
  Rectangle b = rect_of(g_button_id), p = rect_of(g_panel_id);
  int texts = 0;
  for (const auto &c : draw_calls()) if (c.op == "text") texts++;
  return fmt::format("b={:.2f},{:.2f},{:.2f},{:.2f} p={:.2f},{:.2f} draws={} texts={}",
                     b.x, b.y, b.width, b.height, p.width, p.height,
                     draw_calls().size(), texts);
}

static void ensure_input() {
  if (!EntityHelper::has_singleton<input::InputCollector>())
    input::add_singleton_components(EntityHelper::createEntity(),
                                    ui::default_keymap<DefaultAction>());
}

static std::string run_monolithic() {
  ensure_input();
  SystemManager systems;
  ui::setup_with_resolution<>(systems, {800, 600}, std::make_unique<BuildUI>());
  ui::register_render_systems<DefaultAction>(systems);
  systems.run(1.f / 60.f);
  return summarize();
}

static std::string run_phased() {
  ensure_input();
  SystemManager systems;
  Entity &root = init_ui_plugin<DefaultAction>();
  if (!EntityHelper::has_singleton<window_manager::ProvidesCurrentResolution>())
    window_manager::add_singleton_components(root, {800, 600}, 60);
  ui::register_before_ui_updates<DefaultAction>(systems);
  systems.register_update_system(std::make_unique<BuildUI>());
  ui::register_layout_systems<DefaultAction>(systems);
  systems.register_update_system(std::make_unique<BetweenLayoutAndInput>());
  ui::register_post_layout_systems<DefaultAction>(systems);
  ui::register_render_systems<DefaultAction>(systems);

  auto &entities = EntityHelper::get_entities_for_mod();
  systems.fixed_tick_all(entities, 1.f / 60.f);
  systems.tick_all(entities, 1.f / 60.f);
  EntityHelper::cleanup();
  CHECK(g_width_after_layout > 0.f);
  CHECK(g_draws_after_layout == 0);
  systems.render_all(1.f / 60.f);
  return summarize();
}

int main() {
  printf("=== frame phases ===\n\n");
  int fds[2];
  if (pipe(fds) != 0) return 1;
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    std::string s = run_monolithic();
    ssize_t ignored = write(fds[1], s.c_str(), s.size());
    (void) ignored;
    _exit(0);
  }
  close(fds[1]);
  std::string mono;
  char buf[512];
  ssize_t n;
  while ((n = read(fds[0], buf, sizeof(buf))) > 0) mono.append(buf, (size_t) n);
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  CHECK(!mono.empty());

  std::string phased = run_phased();
  printf("  mono:   %s\n  phased: %s\n", mono.c_str(), phased.c_str());
  CHECK(phased == mono);
  CHECK(g_button_id != -1 && rect_of(g_button_id).width > 0.f);
  CHECK(!draw_calls().empty());

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) { printf("FAILURES: %d\n", checks_run - checks_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
