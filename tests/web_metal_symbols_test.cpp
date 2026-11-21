// web_metal_symbols_test.cpp
// R-144: web (Emscripten/GLES3) builds pulled in Metal symbols that do not
// exist there (metal_create_system_device / metal_capture_render_texture),
// because the sokol backend keyed its capture/device code on
// AFTER_HOURS_USE_VULKAN-vs-everything-else instead of the compiled API.
//
// The real assertion is a build-time symbol audit in tests/Makefile: the
// simulated-web TU web_sim/web_probe.cpp is compiled with web defines and
// the resulting object must have no undefined metal_*/vulkan_* symbols,
// while a SOKOL_METAL control compile of the same TU must still reference
// the Metal symbols (guards against over-excluding the native path). The
// Makefile fails the suite if either audit fails; this binary re-reads the
// audit proof file the rule left behind and reports it.

#include <cstdio>

#ifndef WEB_METAL_AUDIT_PATH
#define WEB_METAL_AUDIT_PATH "output/web_metal_symbols.audit"
#endif
#include <cstdlib>
#include <fstream>
#include <string>

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main(int argc, char **argv) {
  // Path to the audit proof file, passed by the Makefile rule.
  const char *proof =
      argc > 1 ? argv[1] : WEB_METAL_AUDIT_PATH;
  std::ifstream in(proof);
  std::string web_line, native_line;
  std::getline(in, web_line);
  std::getline(in, native_line);
  check(web_line == "web: no undefined metal_*/vulkan_* symbols",
        "web-sim object audited clean (no Metal symbols)");
  check(native_line == "native: metal_* symbols referenced",
        "native Metal control still references metal_* symbols");
  printf("web_metal_symbols_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
