// process_relaunch_test.cpp
// process::current_executable_path, build_argv, and a real forked
// relaunch_self that must come back with the child exit code.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <afterhours/src/plugins/process.h>

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace afterhours;

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
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--relaunched")
      return 42;

  const auto exe = process::current_executable_path();
  check(exe.has_value(), "current_executable_path returns a path");
  if (exe)
    check(std::filesystem::exists(*exe),
          "the executable path exists on disk");

  {
    const std::string exe_arg = "/bin/app";
    const std::vector<std::string> args = {"--relaunched", "value"};
    const auto built = process::build_argv(exe_arg, args);
    check(built.size() == 4, "build_argv has exe, args, and terminator");
    check(built.size() == 4 && std::string(built[0]) == "/bin/app",
          "argv[0] is the executable");
    check(built.size() == 4 && std::string(built[1]) == "--relaunched" &&
              std::string(built[2]) == "value",
          "arguments follow in order");
    check(built.size() == 4 && built[3] == nullptr,
          "argv is null terminated");
  }

#if !defined(_WIN32)
  {
    const pid_t pid = fork();
    check(pid >= 0, "fork succeeds");
    if (pid == 0) {
      const std::vector<std::string> args = {"--relaunched"};
      process::relaunch_self(args);
      _exit(3);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 42,
          "the relaunched child exits with code 42");
  }
#endif

  printf("process_relaunch_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
