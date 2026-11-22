
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
