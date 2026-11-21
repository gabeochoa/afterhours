// event_phases_test.cpp — ordered event phases (R-130 / AH-04).
#include <afterhours/src/core/event_queue.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

static int tests_run = 0, tests_passed = 0;
static void check(bool c, const char *e, const char *f, int l) {
  tests_run++;
  if (c) tests_passed++;
  else fprintf(stderr, "  FAIL: %s (%s:%d)\n", e, f, l);
}
#define CHECK(e) check((e), #e, __FILE__, __LINE__)

enum class Phase { Death = 0, Effects = 1, Completion = 2 };

static std::vector<std::string> run_once() {
  EventQueue<std::string, Phase> q;
  q.emit(Phase::Completion, "complete");
  q.emit(Phase::Effects, "summon");
  q.emit(Phase::Death, "death-a");
  q.emit(Phase::Effects, "heal");
  q.emit(Phase::Death, "death-b");
  std::vector<std::string> out;
  q.drain([&](Phase, const std::string &e) { out.push_back(e); });
  return out;
}

int main() {
  // Phase order regardless of emission order; same phase stable.
  CHECK(run_once() == (std::vector<std::string>{"death-a", "death-b", "summon",
                                                 "heal", "complete"}));
  // Deterministic across runs.
  CHECK(run_once() == run_once());

  // Monotonic sequence returned by emit.
  {
    EventQueue<int> q;
    CHECK(q.emit(2, 1) == 0);
    CHECK(q.emit(1, 2) == 1);
    CHECK(q.next_sequence() == 2);
    CHECK(q.pending() == 2);
    std::vector<int> out;
    q.drain([&](int, int e) { out.push_back(e); });
    CHECK(out == (std::vector<int>{2, 1}));  // phase 1 before phase 2
    CHECK(q.empty());
  }

  // Cascaded events wait for the next batch, then phase-order within it.
  {
    EventQueue<std::string, Phase> q;
    q.emit(Phase::Death, "death");
    std::vector<std::string> out;
    auto r = q.drain([&](Phase, const std::string &e) {
      out.push_back(e);
      if (e == "death") {
        q.emit(Phase::Completion, "complete");
        q.emit(Phase::Effects, "on-death");
      }
    });
    CHECK(!r.limit_exceeded && r.delivered == 3);
    CHECK(out == (std::vector<std::string>{"death", "on-death", "complete"}));
  }

  // Work limit: stops, reports, remainder (incl. cascades) stays pending.
  {
    EventQueue<int> q;
    for (int i = 0; i < 5; i++) q.emit(0, i);
    std::vector<int> out;
    auto r = q.drain([&](int, int e) { out.push_back(e); }, 3);
    CHECK(r.limit_exceeded && r.delivered == 3 && q.pending() == 2);
    auto r2 = q.drain([&](int, int e) { out.push_back(e); });
    CHECK(!r2.limit_exceeded && out == (std::vector<int>{0, 1, 2, 3, 4}));
  }

  // clear() drops pending without delivering.
  {
    EventQueue<int> q;
    q.emit(0, 1);
    q.clear();
    CHECK(q.empty());
  }

  printf("event_phases: %d/%d passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
