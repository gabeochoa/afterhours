#include <afterhours/src/incremental_loader.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using afterhours::IncrementalLoader;

static int checks_run = 0, checks_passed = 0;
static void check(bool c, const char *e, int l) {
  checks_run++;
  if (c) checks_passed++;
  else fprintf(stderr, "  FAIL: %s (line %d)\n", e, l);
}
#define CHECK(x) check((x), #x, __LINE__)

static IncrementalLoader loader_with(int n, std::vector<int> &order) {
  IncrementalLoader l;
  for (int i = 0; i < n; i++) l.add([&order, i] { order.push_back(i); });
  return l;
}

int main() {
  {  // empty is done/sane
    IncrementalLoader l;
    CHECK(l.done()); CHECK(l.progress() == 1.f);
    CHECK(l.pump_items(3) == 0); CHECK(l.pump_time(5, [] { return 0.0; }) == 0);
  }
  {  // item budget exact + ceil(N/budget) calls + order
    std::vector<int> order;
    auto l = loader_with(7, order);
    CHECK(l.pump_items(3) == 3); CHECK(!l.done());
    CHECK(l.completed() == 3 && l.remaining() == 4 && l.total() == 7);
    CHECK(std::fabs(l.progress() - 3.f / 7.f) < 1e-6f);
    CHECK(l.pump_items(3) == 3); CHECK(l.pump_items(3) == 1); CHECK(l.done());
    CHECK((order == std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
    CHECK(l.pump_items(3) == 0);
  }
  {  // zero budget runs nothing for items
    std::vector<int> order; auto l = loader_with(2, order);
    CHECK(l.pump_items(0) == 0); CHECK(order.empty());
  }
  {  // time budget with injected deterministic clock: each now() call +10ms
    std::vector<int> order; auto l = loader_with(10, order);
    double t = 0; auto clock = [&] { return (t += 10); };
    // start=10; after step1 t=20 (10<25 run), step2 t=30 (20<25), step3 t=40 stop
    CHECK(l.pump_time(25, clock) == 3);
    CHECK((order == std::vector<int>{0, 1, 2}));
    // zero time budget still runs one step (no stall)
    auto l2 = loader_with(2, order);
    double t2 = 0; auto c2 = [&] { return (t2 += 100); };
    CHECK(l2.pump_time(0, c2) == 1);
  }
  {  // cancel
    std::vector<int> order; auto l = loader_with(5, order);
    CHECK(l.pump_items(2) == 2); l.cancel();
    CHECK(l.cancelled()); CHECK(l.done()); CHECK(l.remaining() == 0);
    CHECK(l.pump_items(2) == 0); CHECK(order.size() == 2);
    CHECK(l.progress() == 1.f);  // total() now equals completed after cancel
    l.clear(); CHECK(!l.cancelled()); CHECK(l.total() == 0);
  }
  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
