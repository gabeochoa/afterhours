#define AFTER_HOURS_REPLACE_LOGGING
#include <cstdio>
#include <cstdlib>

#define log_trace(...) do {} while (0)
#define log_info(...) do {} while (0)
#define log_warn(...) do {} while (0)
#define log_error(...) std::abort()
#define log_once_per(...) do {} while (0)

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

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

void abort_in_sink(const char *, const char *) { std::abort(); }

struct Outcome {
  int status = 0;
  std::string stderr_text;
};

Outcome crash_in_child(bool sink_aborts) {
  const auto path =
      std::filesystem::temp_directory_path() / "afh_crash_reentry.txt";
  std::filesystem::remove(path);
  std::fflush(stdout);
  const pid_t pid = fork();
  if (pid == 0) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, 2);
    alarm(5);
    afterhours::crash::install_handler();
    if (sink_aborts)
      log_sink_fn = abort_in_sink;
    volatile int *bad = nullptr;
    *bad = 1;
    _exit(0);
  }
  Outcome out;
  waitpid(pid, &out.status, 0);
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  out.stderr_text = ss.str();
  std::filesystem::remove(path);
  return out;
}

} // namespace

int main() {
  {
    const Outcome out = crash_in_child(false);
    check(WIFSIGNALED(out.status) && WTERMSIG(out.status) == SIGSEGV,
          "a host whose log_error aborts still dies of the original signal");
    check(out.stderr_text.find("afterhours crash: SIGSEGV") != std::string::npos,
          "and the report reaches stderr");
  }
  {
    const Outcome out = crash_in_child(true);
    check(WIFSIGNALED(out.status) && WTERMSIG(out.status) != SIGALRM,
          "a sink that aborts mid-report ends the process instead of looping");
    const auto first = out.stderr_text.find("afterhours crash");
    check(first != std::string::npos, "the first report line was still written");
    check(out.stderr_text.find("afterhours crash", first + 1) == std::string::npos,
          "and the abort inside the report does not start a second one");
  }

  std::printf("%d/%d checks passed\n", checks_run - checks_failed, checks_run);
  return checks_failed == 0 ? 0 : 1;
}
