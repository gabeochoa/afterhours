// crash_report_file_test.cpp
// crash::install(report_path) has to leave a usable file behind: signal,
// backtrace, the system SystemManager said was running, and the log tail.
// The direct-writer checks run in-process; the real-signal checks fork, like
// crash_handler_test, so the suite itself never dies.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <afterhours/src/core/system.h>
#include <afterhours/src/crash_handler.h>

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

std::size_t occurrences(const std::string &haystack, const std::string &needle) {
  std::size_t count = 0, at = 0;
  while ((at = haystack.find(needle, at)) != std::string::npos) {
    count++;
    at += needle.size();
  }
  return count;
}

void aborting_sink(const char *, const char *) { std::abort(); }

} // namespace

int main() {
  std::printf("=== crash report file tests ===\n\n");

  const auto dir = std::filesystem::temp_directory_path();
  const auto path = dir / "afh_crash_report.txt";
  std::filesystem::remove(path);

  // --- the writer, directly ---
  auto *sink_before = log_sink_fn;
  afterhours::crash::install(path.string().c_str());
  afterhours::crash::install(path.string().c_str()); // again: no double chain
  afterhours::crash::set_running_system("DirectWriterSystem");
  if (log_sink_fn)
    log_sink_fn("[INFO]", "tail marker one");
  check(afterhours::crash::detail::write_report_file("SIGTEST (direct writer)"),
        "the writer writes when a report path is installed");
  const std::string report = read_all(path);
  check(report.find("afterhours crash: SIGTEST (direct writer)") !=
            std::string::npos,
        "the file names what happened");
  check(report.find("DirectWriterSystem") != std::string::npos,
        "the file names the running system");
  check(report.find("backtrace:") != std::string::npos,
        "the file has a backtrace section");
  check(report.find("tail marker one") != std::string::npos,
        "the file carries the log tail");
  check(occurrences(report, "tail marker one") == 1,
        "a second install does not chain the sink twice");

  afterhours::crash::uninstall();
  check(log_sink_fn == sink_before, "uninstall restores the previous log sink");
  check(!afterhours::crash::detail::write_report_file("SIGTEST (after)"),
        "uninstall clears the report path, so the writer stops writing");
  afterhours::crash::set_running_system(nullptr);
  std::filesystem::remove(path);

  // --- a real abort inside a system the manager is running ---
  {
    const auto sys_path = dir / "afh_crash_report_system.txt";
    std::filesystem::remove(sys_path);
    std::fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
      afterhours::crash::install(sys_path.string().c_str());
      afterhours::SystemManager sm;
      sm.register_update_system([](float) {
        if (log_sink_fn)
          log_sink_fn("[INFO]", "last words from inside the system");
        std::abort();
      });
      afterhours::Entities entities;
      sm.tick(entities, 0.016f);
      _exit(0); // the handler re-raises, so this is unreachable
    }
    int status = 0;
    waitpid(pid, &status, 0);
    const std::string file = read_all(sys_path);
    check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT,
          "the child still dies of the abort");
    check(file.find("SIGABRT") != std::string::npos,
          "the file names the signal");
    // The name comes from SystemManager's profile scope (the demangled type),
    // not from anything the test set by hand: (a) feeding (b).
    check(file.find("CallbackSystem") != std::string::npos,
          "the file names the system SystemManager was running");
    check(file.find("last words from inside the system") != std::string::npos,
          "the file's log tail has the system's last log line");
    std::filesystem::remove(sys_path);
  }

  // --- a sink that aborts mid-report cannot lose the file (#126) ---
  {
    const auto sink_path = dir / "afh_crash_report_sink.txt";
    std::filesystem::remove(sink_path);
    std::fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
      alarm(5);
      log_sink_fn = aborting_sink; // installed before, so it gets chained
      afterhours::crash::install(sink_path.string().c_str());
      volatile int *bad = nullptr;
      *bad = 1;
      _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    const std::string file = read_all(sink_path);
    check(WIFSIGNALED(status) && WTERMSIG(status) != SIGALRM,
          "an aborting sink ends the child instead of looping the handler");
    check(file.find("afterhours crash: SIGSEGV") != std::string::npos,
          "the report file was complete before the sink ever ran");
    std::filesystem::remove(sink_path);
  }

  std::printf("\n%d/%d checks passed\n", checks_run - checks_failed, checks_run);
  if (checks_failed == 0)
    std::printf("All checks passed!\n");
  return checks_failed == 0 ? 0 : 1;
}
