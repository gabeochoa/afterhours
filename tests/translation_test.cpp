// translation_test.cpp
// Regression tests for translation lookup failure handling: translator typos
// (malformed format strings, missing params) and maps without English used
// to throw out of UI string building.

#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/translation.h>

#include <cstdio>
#include <map>
#include <string>

using namespace afterhours;
using namespace afterhours::translation;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

namespace {
enum class Key { Hello, Items };
enum class Param { Count };
enum class FontID { Default };
struct FontNameGetter {
  std::string operator()(FontID) const { return "default"; }
};
using Plugin = translation_plugin<Key, Param, FontID, FontNameGetter>;
using ParamMap = std::map<Param, std::string>;
using TS = Plugin::TranslatableStringType;

std::string captured_errors;
void capture_error(const char *level, const char *message) {
  if (level && message &&
      std::string_view(level).find("ERROR") != std::string_view::npos) {
    captured_errors += message;
    captured_errors += '\n';
  }
}
} // namespace

int main() {
  const ParamMap param_map{{Param::Count, "count"}};
  const auto previous_sink = log_sink_fn;
  log_sink_fn = capture_error;

  // Happy path: a well-formed string with its param formats normally.
  {
    TS s(std::string("{count} items"));
    s.set_param(Param::Count, std::string("3"), param_map);
    bool threw = false;
    std::string out;
    try {
      out = s.format(param_map);
    } catch (...) {
      threw = true;
    }
    CHECK(!threw);
    CHECK(out == "3 items");
  }

  // Malformed format: returns the raw string, logs, and does not throw.
  {
    captured_errors.clear();
    TS s(std::string("{count} item{s"));
    s.set_param(Param::Count, std::string("3"), param_map);
    bool threw = false;
    std::string out;
    try {
      out = s.format(param_map);
    } catch (...) {
      threw = true;
    }
    CHECK(!threw);
    CHECK(out == "{count} item{s");
    CHECK(captured_errors.find("{count} item{s") != std::string::npos);
  }

  // Missing param (name mismatch): same raw-string fallback.
  {
    captured_errors.clear();
    const ParamMap wrong_names{{Param::Count, "total"}};
    TS s(std::string("{count} items"));
    s.set_param(Param::Count, std::string("3"), wrong_names);
    bool threw = false;
    std::string out;
    try {
      out = s.format(wrong_names);
    } catch (...) {
      threw = true;
    }
    CHECK(!threw);
    CHECK(out == "{count} items");
    CHECK(captured_errors.find("{count} items") != std::string::npos);
  }

  log_sink_fn = previous_sink;

  // A map without English: unknown language falls back to empty, no throw.
  {
    Plugin::ProvidesTranslation p;
    p.translations = {{Language::Korean,
                       {{Key::Hello, TS(std::string("hello"))}}}};
    p.current_language = Language::Japanese;
    bool threw = false;
    std::string out;
    try {
      out = p.get_string(Key::Hello);
    } catch (...) {
      threw = true;
    }
    CHECK(!threw);
    CHECK(out == "MISSING_TRANSLATION");
  }

  // English fallback still applies when English exists.
  {
    Plugin::ProvidesTranslation p;
    p.translations = {{Language::English,
                       {{Key::Hello, TS(std::string("Hello"))}}}};
    p.current_language = Language::Japanese;
    bool threw = false;
    std::string out;
    try {
      out = p.get_string(Key::Hello);
    } catch (...) {
      threw = true;
    }
    CHECK(!threw);
    CHECK(out == "Hello");
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0)
    std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
