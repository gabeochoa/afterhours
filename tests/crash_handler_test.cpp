// crash_handler_test.cpp
// The handler is only worth having if it survives a real crash, so this forks
// a child and actually kills it rather than calling the reporting function
// directly. A handler that works when invoked by hand and not when the process
// is dying is the failure mode worth catching.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include <afterhours/src/crash_handler.h>
#include <afterhours/src/log_file.h>

namespace {

int checks_run = 0;
int checks_failed = 0;

void check(bool ok, const char *what) {
  checks_run++;
  if (!ok) {
    checks_failed++;
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

std::string read_all(const std::filesystem::path &p) {
  std::ifstream in(p);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Runs `crash` in a child with the handler installed and the log pointed at
// `path`. Returns the child's wait status.
int crash_in_child(const std::filesystem::path &path, void (*crash)()) {
  std::filesystem::remove(path);
  // The child inherits our stdio buffer, and abort() flushes it, so anything
  // unflushed here gets printed twice.
  std::fflush(stdout);
  const pid_t pid = fork();
  if (pid == 0) {
    afterhours::crash::install_handler();
    afterhours::log_file::open(path.string().c_str());
    afterhours::crash::set_running_system("PretendRenderSystem");
    crash();
    // The handler re-raises, so this is unreachable. If it is reached the
    // handler swallowed the crash, which is its own bug.
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return status;
}

void raise_segv() {
  volatile int *bad = nullptr;
  *bad = 1;
}

void call_abort() { std::abort(); }

} // namespace

int main() {
  std::printf("=== crash handler tests ===\n\n");

  const auto dir = std::filesystem::temp_directory_path();

  {
    const auto path = dir / "afh_crash_segv.txt";
    const int status = crash_in_child(path, raise_segv);
    const std::string report = read_all(path);
    std::printf("  segv: %zu bytes of report\n", report.size());

    check(WIFSIGNALED(status), "the child still dies of the signal");
    check(report.find("afterhours crash") != std::string::npos,
          "a segfault leaves a report");
    check(report.find("SIGSEGV") != std::string::npos,
          "and names the signal");
    // The thing a stack trace usually cannot tell you.
    check(report.find("PretendRenderSystem") != std::string::npos,
          "and names the system that was running");
    std::filesystem::remove(path);
  }

  {
    // abort() is the interesting one: it is how the library's own invariants
    // fail, via VALIDATE, gen_first_enforce and get_singleton_cmp_enforce.
    const auto path = dir / "afh_crash_abort.txt";
    const int status = crash_in_child(path, call_abort);
    const std::string report = read_all(path);
    std::printf("  abort: %zu bytes of report\n", report.size());

    check(WIFSIGNALED(status), "an abort still terminates the child");
    check(report.find("afterhours crash") != std::string::npos,
          "an abort leaves a report too");
    check(report.find("SIGABRT") != std::string::npos, "and names it");
    std::filesystem::remove(path);
  }

  std::printf("\n%d/%d checks passed\n", checks_run - checks_failed,
              checks_run);
  if (checks_failed == 0)
    std::printf("All checks passed!\n");
  return checks_failed == 0 ? 0 : 1;
}
