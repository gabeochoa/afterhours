#include <afterhours/src/graphics.h>

#include <cstdio>

namespace g = afterhours::graphics;

static_assert(static_cast<int>(g::TraceLogLevel::All) == ::raylib::LOG_ALL);
static_assert(static_cast<int>(g::TraceLogLevel::Trace) ==
              ::raylib::LOG_TRACE);
static_assert(static_cast<int>(g::TraceLogLevel::Debug) ==
              ::raylib::LOG_DEBUG);
static_assert(static_cast<int>(g::TraceLogLevel::Info) ==
              ::raylib::LOG_INFO);
static_assert(static_cast<int>(g::TraceLogLevel::Warning) ==
              ::raylib::LOG_WARNING);
static_assert(static_cast<int>(g::TraceLogLevel::Error) ==
              ::raylib::LOG_ERROR);
static_assert(static_cast<int>(g::TraceLogLevel::Fatal) ==
              ::raylib::LOG_FATAL);
static_assert(static_cast<int>(g::TraceLogLevel::None) ==
              ::raylib::LOG_NONE);
static_assert(g::FLAG_BORDERLESS_WINDOWED_MODE != 0);
static_assert((g::FLAG_BORDERLESS_WINDOWED_MODE & g::FLAG_WINDOW_RESIZABLE) ==
              0);
static_assert((g::FLAG_BORDERLESS_WINDOWED_MODE & g::FLAG_VSYNC_HINT) == 0);

int main() {
  int failures = 0;
  auto check = [&](bool cond, const char *what) {
    if (!cond) {
      failures++;
      std::printf("  FAIL: %s\n", what);
    }
  };

  g::clear_window_state(g::FLAG_WINDOW_RESIZABLE | g::FLAG_VSYNC_HINT);
  check(g::window_state() == 0, "clear leaves no tracked bits");
  g::set_window_state(g::FLAG_WINDOW_RESIZABLE);
  check((g::window_state() & g::FLAG_WINDOW_RESIZABLE) != 0,
        "set records RESIZABLE");
  check((g::window_state() & g::FLAG_VSYNC_HINT) == 0,
        "VSYNC stays clear");
  g::set_window_state(g::FLAG_VSYNC_HINT);
  g::clear_window_state(g::FLAG_WINDOW_RESIZABLE);
  check((g::window_state() & g::FLAG_VSYNC_HINT) != 0,
        "VSYNC survives clearing RESIZABLE");
  check((g::window_state() & g::FLAG_WINDOW_RESIZABLE) == 0,
        "RESIZABLE stays clear");

  g::clear_window_state(g::FLAG_BORDERLESS_WINDOWED_MODE);
  g::set_window_state(g::FLAG_BORDERLESS_WINDOWED_MODE);
  check((g::window_state() & g::FLAG_BORDERLESS_WINDOWED_MODE) != 0,
        "set records BORDERLESS");
  check((g::window_state() & g::FLAG_VSYNC_HINT) != 0,
        "VSYNC survives setting BORDERLESS");
  g::clear_window_state(g::FLAG_BORDERLESS_WINDOWED_MODE);
  check((g::window_state() & g::FLAG_BORDERLESS_WINDOWED_MODE) == 0,
        "BORDERLESS stays clear");

  g::set_trace_log_level(g::TraceLogLevel::Warning);

  std::printf("window_state_test: %s\n", failures ? "FAIL" : "ok");
  return failures ? 1 : 0;
}
