#define AFTER_HOURS_REPLACE_LOGGING
#include <cstdio>
#include <cstdlib>

#define log_trace(...) do {} while (0)
#define log_info(...) do {} while (0)
#define log_warn(...) do {} while (0)
#define log_error(...) std::abort()
#define log_once_per(...) do {} while (0)

#include <csignal>

#include <sys/wait.h>
#include <unistd.h>

#include "../../../../src/crash_handler.h"

int main() {
  std::fflush(stdout);
  const pid_t child = fork();
  if (child == 0) {
    afterhours::crash::install_handler();
    afterhours::crash::set_running_system("PhysicsSystem");
    std::raise(SIGSEGV);
    _exit(0);
  }
  int status = 0;
  waitpid(child, &status, 0);
  const bool died_of_segv = WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV;
  std::printf("child with an aborting log_error died of %s\n",
              died_of_segv ? "SIGSEGV, after one report" : "something else");
  return died_of_segv ? 0 : 1;
}
