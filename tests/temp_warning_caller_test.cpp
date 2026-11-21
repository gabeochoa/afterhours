#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                    \
  } register_##name##_instance;                                                \
  static void test_##name()

struct TestEntry {
  const char *name;
  void (*fn)();
};
static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}
static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static std::vector<std::string> &captured() {
  static std::vector<std::string> v;
  return v;
}

static std::vector<std::string> miss_messages() {
  std::vector<std::string> out;
  for (const auto &m : captured()) {
    if (m.find("query will miss") != std::string::npos) out.push_back(m);
  }
  return out;
}

static bool captured_contains(const std::string &needle) {
  for (const auto &m : captured()) {
    if (m.find(needle) != std::string::npos) return true;
  }
  return false;
}

TEST(warning_names_calling_file_line_and_function) {
  EntityCollection ec;
  ec.createEntity();
  CHECK(!ec.get_temp().empty());
  captured().clear();
  const int line = __LINE__ + 2;
  (void)line;
  EntityQuery q(ec);
  (void)q;
  const auto miss = miss_messages();
  CHECK(miss.size() == 1);
  if (!miss.empty()) {
    CHECK(miss[0].find("query will miss 1 ents in temp") !=
          std::string::npos);
#if defined(AFTER_HOURS_DEBUG)
    CHECK(miss[0].find("temp_warning_caller_test.cpp") != std::string::npos);
    CHECK(miss[0].find(":" + std::to_string(line) + " in ") !=
          std::string::npos);
    CHECK(miss[0].find("warning_names_calling_file_line_and_function") !=
          std::string::npos);
#else
    CHECK(miss[0].find("query at") == std::string::npos);
#endif
  }
  CHECK(captured_contains("temp entity 0"));
}

TEST(each_call_site_reports_its_own_line) {
  EntityCollection ec;
  ec.createEntity();
  captured().clear();
  const int first_line = __LINE__ + 1;
  EntityQuery a(ec);
  const int second_line = __LINE__ + 1;
  EntityQuery b(ec);
  (void)a;
  (void)b;
  (void)first_line; (void)second_line;
  CHECK(first_line != second_line);
  const auto miss = miss_messages();
  CHECK(miss.size() == 2);
#if defined(AFTER_HOURS_DEBUG)
  if (miss.size() == 2) {
    CHECK(miss[0].find(":" + std::to_string(first_line) + " in ") !=
          std::string::npos);
    CHECK(miss[1].find(":" + std::to_string(second_line) + " in ") !=
          std::string::npos);
  }
#else
  for (const auto &m : miss) CHECK(m.find("query at") == std::string::npos);
#endif
}

TEST(default_collection_query_names_its_caller_too) {
  EntityCollection ec;
  ec.createEntity();
  EntityHelper::set_default_collection(&ec);
  captured().clear();
  const int line = __LINE__ + 2;
  (void)line;
  EntityQuery q;
  (void)q;
  EntityHelper::set_default_collection(nullptr);
  const auto miss = miss_messages();
  CHECK(miss.size() == 1);
#if defined(AFTER_HOURS_DEBUG)
  if (!miss.empty()) {
    CHECK(miss[0].find("temp_warning_caller_test.cpp") != std::string::npos);
    CHECK(miss[0].find(":" + std::to_string(line) + " in ") !=
          std::string::npos);
  }
#else
  if (!miss.empty()) CHECK(miss[0].find("query at") == std::string::npos);
#endif
}

TEST(no_warning_once_temp_is_merged) {
  EntityCollection ec;
  ec.createEntity();
  ec.merge_entity_arrays();
  CHECK(ec.get_temp().empty());
  captured().clear();
  EntityQuery q(ec);
  (void)q;
  CHECK(miss_messages().empty());
}

TEST(ignore_temp_warning_stays_quiet) {
  EntityCollection ec;
  ec.createEntity();
  captured().clear();
  EntityQuery q(ec, {.ignore_temp_warning = true});
  (void)q;
  CHECK(miss_messages().empty());
}

TEST(force_merge_stays_quiet_and_merges) {
  EntityCollection ec;
  ec.createEntity();
  captured().clear();
  EntityQuery q(ec, {.force_merge = true});
  (void)q;
  CHECK(miss_messages().empty());
  CHECK(ec.get_temp().empty());
}

int main() {
  log_sink_fn = [](const char *level, const char *message) {
    captured().push_back(std::string(level) + " " + message);
  };
  printf("=== temp warning caller tests ===\n\n");
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }
  log_sink_fn = nullptr;
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
