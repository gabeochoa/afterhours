#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <memory>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

struct Gated : System<> {
  bool *open;
  int *activates;
  int *deactivates;
  int *runs;
  std::string *order;
  Gated(bool &o, int &a, int &d, int &r, std::string &ord)
      : open(&o), activates(&a), deactivates(&d), runs(&r), order(&ord) {}
  bool should_run(const float) const override { return *open; }
  void on_activate(const float) override {
    ++*activates;
    *order += "A";
  }
  void on_deactivate(const float) override {
    ++*deactivates;
    *order += "D";
  }
  void once(const float) override {
    ++*runs;
    *order += "R";
  }
};

struct ConstHooks : System<> {
  bool *open;
  int *activates;
  int *deactivates;
  ConstHooks(bool &o, int &a, int &d) : open(&o), activates(&a), deactivates(&d) {}
  bool should_run(const float) const override { return *open; }
  void on_activate(const float) const override { ++*activates; }
  void on_deactivate(const float) const override { ++*deactivates; }
};

int main() {
  {
    bool open = false;
    int activates = 0, deactivates = 0, runs = 0;
    std::string order;
    SystemManager sm;
    auto sys = std::make_unique<Gated>(open, activates, deactivates, runs, order);
    Gated *raw = sys.get();
    sm.register_update_system(std::move(sys));

    sm.run(1.f / 60.f);
    check(activates == 0 && deactivates == 0 && runs == 0,
          "starting closed fires neither hook and does not run");
    check(!raw->is_active(), "starting closed is not active");

    open = true;
    sm.run(1.f / 60.f);
    check(activates == 1 && deactivates == 0 && runs == 1,
          "opening fires on_activate once and runs");
    check(raw->is_active(), "open system is active");
    check(order == "AR", "on_activate fires before the first run");

    sm.run(1.f / 60.f);
    check(activates == 1 && runs == 2, "staying open does not refire on_activate");

    open = false;
    sm.run(1.f / 60.f);
    check(deactivates == 1 && runs == 2,
          "closing fires on_deactivate once and does not run");
    check(!raw->is_active(), "closed system is not active");
    check(order == "ARRD", "on_deactivate fires instead of a run on close");

    sm.run(1.f / 60.f);
    check(deactivates == 1, "staying closed does not refire on_deactivate");

    open = true;
    sm.run(1.f / 60.f);
    check(activates == 2 && runs == 3, "reopening fires on_activate again");
  }
  {
    bool open = false;
    int activates = 0, deactivates = 0;
    SystemManager sm;
    sm.register_update_system(
        std::make_unique<ConstHooks>(open, activates, deactivates));
    sm.run(1.f / 60.f);
    open = true;
    sm.run(1.f / 60.f);
    open = false;
    sm.run(1.f / 60.f);
    check(activates == 1, "const-only on_activate is honoured");
    check(deactivates == 1, "const-only on_deactivate is honoured");
  }
  {
    bool open = true;
    int activates = 0, deactivates = 0, runs = 0;
    std::string order;
    SystemManager sm;
    sm.register_fixed_update_system(
        std::make_unique<Gated>(open, activates, deactivates, runs, order));
    sm.run(1.f / 60.f);
    check(activates == 1 && runs >= 1, "fixed system activates");
    open = false;
    sm.run(1.f / 60.f);
    check(deactivates == 1, "fixed system deactivates");
  }
  {
    bool open = true;
    int activates = 0, deactivates = 0, runs = 0;
    std::string order;
    SystemManager sm;
    sm.register_render_system(
        std::make_unique<Gated>(open, activates, deactivates, runs, order));
    sm.run(1.f / 60.f);
    check(activates == 1, "render system activates once despite two passes");
    open = false;
    sm.run(1.f / 60.f);
    check(deactivates == 1, "render system deactivates once");
  }

  std::printf("system_deactivate_hook_test: %d/%d checks passed\n",
              checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
