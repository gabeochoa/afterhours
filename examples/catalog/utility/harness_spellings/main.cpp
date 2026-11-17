#include "../../../../src/plugins/e2e_testing/harness.h"

#include <cstdio>
#include <vector>

int main() {
  std::vector<std::string> words = {"app",
                                    "--test-script=scripts/login.e2e",
                                    "--e2e-timeout=12.5",
                                    "--screenshot-dir=shots",
                                    "--headless"};
  std::vector<char *> argv;
  for (auto &w : words) argv.push_back(w.data());

  const int argc = static_cast<int>(argv.size());
  if (!afterhours::testing::should_run_e2e(argc, argv.data())) {
    printf("no e2e script requested\n");
    return 1;
  }

  const afterhours::testing::E2EArgs args =
      afterhours::testing::parse_e2e_args(argc, argv.data());
  printf("script: %s\n", args.script_path.c_str());
  printf("timeout: %.1f\n", static_cast<double>(args.timeout_seconds));
  printf("screenshot: %s\n",
         afterhours::testing::screenshot_path(args, "login", "fallback")
             .c_str());
  printf("headless: %s\n", args.headless ? "yes" : "no");
  return args.enabled && args.script_path == "scripts/login.e2e" ? 0 : 1;
}
