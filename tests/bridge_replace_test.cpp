// bridge_replace_test.cpp — ui::replace_*_system swaps a built-in bridge
// system in place (same slot/order), reports failure, and leaves the
// unreplaced default list identical.
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>
#include <afterhours/ah.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s (line %d)\n", expr, line);
}
#define CHECK(e) check((e), #e, __LINE__)

static int g_replacement_runs = 0;
struct ReplacementScroll : System<> {
  void once(float) override { g_replacement_runs++; }
  void for_each_with(Entity &, float) override {}
};
struct NeverRegistered : System<> {
  void for_each_with(Entity &, float) override {}
};

static std::vector<std::string> names_of(
    const std::vector<std::unique_ptr<SystemBase>> &systems) {
  std::vector<std::string> out;
  for (auto &s : systems) out.emplace_back(s->name());
  return out;
}

int main() {
  printf("=== bridge system replace ===\n\n");

  // Unreplaced default, constructed directly.
  UIPluginPostUpdateBridge<DefaultAction> stock;
  const auto default_names = names_of(stock.systems);
  size_t scroll_idx = stock.systems.size();
  for (size_t i = 0; i < stock.systems.size(); i++)
    if (typeid(*stock.systems[i]) == typeid(HandleScrollInput<DefaultAction>))
      scroll_idx = i;
  CHECK(scroll_idx < stock.systems.size());

  SystemManager sm;
  register_after_ui_updates<DefaultAction>(sm);

  UIPluginPostUpdateBridge<DefaultAction> *bridge = nullptr;
  for (auto &s : sm.update_systems_)
    if (auto *b = dynamic_cast<UIPluginPostUpdateBridge<DefaultAction> *>(s.get()))
      bridge = b;
  CHECK(bridge != nullptr);
  if (!bridge) return 1;
  CHECK(names_of(bridge->systems) == default_names);

  // Missing bridge / missing target: explicit false, nothing changes.
  CHECK((!replace_pre_update_system<DefaultAction,
                                   HandleScrollInput<DefaultAction>>(
      sm, std::make_unique<ReplacementScroll>())));
  CHECK((!replace_post_update_system<DefaultAction, NeverRegistered>(
      sm, std::make_unique<ReplacementScroll>())));
  CHECK(names_of(bridge->systems) == default_names);

  // Replace stock scroll: same size, same order, replacement in the slot.
  CHECK((replace_post_update_system<DefaultAction,
                                   HandleScrollInput<DefaultAction>>(
      sm, std::make_unique<ReplacementScroll>())));
  CHECK(bridge->systems.size() == default_names.size());
  auto after = names_of(bridge->systems);
  for (size_t i = 0; i < after.size(); i++) {
    if (i == scroll_idx)
      CHECK(after[i].find("ReplacementScroll") != std::string::npos);
    else
      CHECK(after[i] == default_names[i]);
  }
  bool stock_present = false;
  for (auto &s : bridge->systems)
    if (typeid(*s) == typeid(HandleScrollInput<DefaultAction>))
      stock_present = true;
  CHECK(!stock_present);

  // The slot bridge execution calls once() on now runs the replacement.
  bridge->systems[scroll_idx]->once(0.f);
  CHECK(g_replacement_runs == 1);

  // Second replacement of the now-absent stock system fails visibly.
  CHECK((!replace_post_update_system<DefaultAction,
                                    HandleScrollInput<DefaultAction>>(
      sm, std::make_unique<ReplacementScroll>())));

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
