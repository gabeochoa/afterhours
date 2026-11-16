#include <afterhours/src/local_time.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
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
  setenv("TZ", "UTC", 1);
  tzset();

  const auto when = std::chrono::system_clock::from_time_t(1761995096);
  const auto t = to_local_time(when);
  CHECK(t.has_value());
  if (t) {
    printf("  %s %s %d %d  %02d:%02d:%02d\n", std::string(t->weekday_name()).c_str(),
           std::string(t->month_name()).c_str(), t->day, t->year, t->hour,
           t->minute, t->second);
    CHECK(t->year == 2025);
    CHECK(t->month == 11);
    CHECK(t->day == 1);
    CHECK(t->weekday == 6);
    CHECK(t->hour == 11);
    CHECK(t->minute == 4);
    CHECK(t->second == 56);
    CHECK(t->weekday_name() == "Sat");
    CHECK(t->month_name() == "Nov");
  }

  setenv("TZ", "America/New_York", 1);
  tzset();
  const auto ny = to_local_time(when);
  CHECK(ny.has_value() && ny->hour == 7);

  CHECK(local_time_now().has_value());

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
