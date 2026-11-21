// Simulates a web (Emscripten) build of crash_handler on a native box: no
// emcc here, so the Makefile builds this target with -D__EMSCRIPTEN__ and
// -Iweb_sim (a shim <emscripten.h> standing in for the Emscripten sysroot).
//
// What it proves: crash_handler.h neither includes execinfo.h nor calls
// backtrace under __EMSCRIPTEN__ (the web build failure wordproc reported),
// and the crash report still names the crash, the running system, and the
// Emscripten callstack. The degradation branch (no emscripten.h at all) is
// covered by the preprocessor audit in the matching gap PR, not a binary,
// because Emscripten always ships the header.

#include <cstdio>
#include <string>

#include <afterhours/src/crash_handler.h>

#ifdef AFTER_HOURS_HAS_EXECINFO
#error "web sim build must not take the execinfo path"
#endif

#ifndef AFTER_HOURS_HAS_EMSCRIPTEN_STACK
#error "web sim build must take the emscripten callstack path"
#endif

extern "C" char *emscripten_get_callstack(void) {
  static char stack[] = "wasm callstack (simulated)";
  return stack;
}

namespace {

std::string captured;

void capture(const char *, const char *message) { captured += message; }

int checks_run = 0;
int checks_failed = 0;

void check(bool ok, const char *what) {
  checks_run++;
  if (!ok) {
    checks_failed++;
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

} // namespace

int main() {
  using namespace afterhours::crash;

  install_handler();
  set_running_system("WebSimSystem");
  log_sink_fn = capture;
  detail::report("web sim crash");

  check(captured.find("afterhours crash: web sim crash") != std::string::npos,
        "the report names the crash");
  check(captured.find("WebSimSystem") != std::string::npos,
        "and names the system that was running");
  check(captured.find("wasm callstack (simulated)") != std::string::npos,
        "and reports the emscripten callstack");

  std::printf("%d/%d checks passed\n", checks_run - checks_failed, checks_run);
  return checks_failed == 0 ? 0 : 1;
}
