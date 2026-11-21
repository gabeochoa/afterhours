#include <afterhours/src/tunable.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

int main() {
  tunable<float> speed("speed", 1.5f);
  tunable<int> lives("lives", 3);
  tunable<bool> god("god", false);
  tunable<std::string> title("title", "hi");
  CHECK(tunable_registry_instance().size() == 4);

  int speed_fires = 0;
  speed.on_change([&](const float &) { ++speed_fires; });

  // Changed source updates values; comments and blanks are skipped.
  auto r = reload_tunables("# cfg\nspeed = 2.5\nlives=5\ngod = true\ntitle = hello world\n\n// done\n");
  CHECK(r.errors.empty());
  CHECK(r.applied == 4 && r.changed == 4);
  CHECK(speed.get() == 2.5f && lives.get() == 5 && god.get() && title.get() == "hello world");
  CHECK(speed_fires == 1);

  // Re-applying the same source changes nothing and fires nothing.
  r = reload_tunables("speed=2.5\nlives=5\ngod=true\ntitle=hello world\n");
  CHECK(r.applied == 4 && r.changed == 0 && r.errors.empty());
  CHECK(speed_fires == 1);

  // Bad lines are reported and tolerated; good lines still apply.
  r = reload_tunables("speed=9\nnope=1\nlives=abc\nbroken line\nspeed = \n");
  CHECK(r.applied == 1 && r.changed == 1);
  CHECK(r.errors.size() == 4);
  CHECK(speed.get() == 9.f && lives.get() == 5); // failed conversions keep old values
  CHECK(speed_fires == 2);

  // Single-pair console-style set, with errors surfaced.
  std::string err;
  CHECK(set_tunable("lives", "7", &err) && lives.get() == 7);
  CHECK(!set_tunable("ghost", "1", &err) && !err.empty());
  CHECK(!set_tunable("lives", "x", &err) && lives.get() == 7);
  CHECK(set_tunable("god", "off", &err) && !god.get());

  // Leaving scope unregisters.
  {
    tunable<float> temp("temp", 0.f);
    CHECK(tunable_registry_instance().contains("temp"));
  }
  CHECK(!tunable_registry_instance().contains("temp"));
  CHECK(tunable_registry_instance().size() == 4);

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
