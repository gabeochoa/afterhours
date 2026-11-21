// e2e_render_checkpoint_test.cpp
// Regression for R-005: a host that batches logic ticks (several updates
// per render) left UI lookups reading the previously drawn view — an
// assert_ui for the new geometry passed before that view was ever
// presented, and one for the old geometry passed on a view already gone.
// Lookups now wait for a render newer than dispatch (the runner stamps
// seen_render; a render bumps VisibleTextRegistry::generation via clear()).
#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::testing;

static PendingE2ECommand armed(std::string name, std::vector<std::string> args) {
  PendingE2ECommand cmd;
  cmd.name = std::move(name);
  cmd.args = std::move(args);
  // What E2ERunner::dispatch_command stamps at dispatch time.
  cmd.seen_render =
      static_cast<long long>(VisibleTextRegistry::instance().generation());
  return cmd;
}

static void simulate_render(const char *text) {
  auto &registry = VisibleTextRegistry::instance();
  registry.clear(); // a render pass clears, bumping the generation
  registry.register_text(text);
}

// Build the panel at a new x and lay it out WITHOUT rendering: the batched
// logic ticks of the bug report.
static void logic_tick_to(ui_test::ImmTestHarness &h, float x,
                          const char *label) {
  h.begin_frame();
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(100), ui::pixels(40)})
                   .with_absolute_position(x, 0)
                   .with_label(label)
                   .with_debug_name("panel"));
  h.layout_only();
}

TEST(batched_ticks_lookup_waits_for_render_then_reads_current) {
  ui_test::ImmTestHarness h;
  auto &registry = VisibleTextRegistry::instance();
  registry.clear();

  logic_tick_to(h, 0, "old");
  simulate_render("old"); // presented frame: panel at x=0

  // Several logic ticks, no render: the panel moves to x=300.
  logic_tick_to(h, 300, "new");
  logic_tick_to(h, 300, "new");

  // New geometry: before the checkpoint this consumed immediately on a
  // view that had never been presented.
  auto current = armed("assert_ui", {"panel", "x=300"});
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), current, 0.f);
  CHECK(current.is_retry() && !current.is_consumed());

  // Old (previously presented) geometry: the stale read that used to pass.
  auto stale = armed("assert_ui", {"panel", "x=0"});
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), stale, 0.f);
  CHECK(stale.is_retry() && !stale.is_consumed());

  simulate_render("new");
  current.reset_retry();
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), current, 0.f);
  CHECK(current.is_consumed() && current.error_message.empty());

  stale.reset_retry();
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), stale, 0.f);
  CHECK(stale.is_retry());
  CHECK(stale.last_mismatch.find("x=0 but got 300") != std::string::npos);
  registry.clear();
}

TEST(batched_ticks_dump_ui_waits_for_render) {
  ui_test::ImmTestHarness h;
  auto &registry = VisibleTextRegistry::instance();
  registry.clear();

  logic_tick_to(h, 0, "old");
  simulate_render("old");
  logic_tick_to(h, 300, "new");

  std::string dumped;
  ui_commands::HandleDumpUICommand dump_handler(
      [&](const std::string &, const std::string &xml) { dumped = xml; });
  auto cmd = armed("dump_ui", {"tree"});
  dump_handler.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_retry() && dumped.empty());

  simulate_render("new");
  cmd.reset_retry();
  dump_handler.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.error_message.empty());
  CHECK(dumped.find("x=\"300\"") != std::string::npos);
  registry.clear();
}

TEST(unarmed_lookup_still_evaluates_immediately) {
  // Handlers driven outside the runner (unit tests, custom hosts) keep the
  // old one-call behaviour: seen_render stays -1 and no render is awaited.
  ui_test::ImmTestHarness h;
  logic_tick_to(h, 10, "here");
  PendingE2ECommand cmd;
  cmd.name = "assert_ui";
  cmd.args = {"panel", "x=10"};
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.error_message.empty());
}

int main() { return ui_test::run_registered_tests("E2E render checkpoint"); }
