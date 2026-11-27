// log_file_test.cpp
// logging.h printed to stdout and nowhere else, which is nothing at all to a
// double-clicked .app: a tester who hits a bug has nothing to send back.
//
// The awkward part is that the log path is not knowable at static-init time --
// it lives under the save directory, which does not exist until files::init
// runs -- so the startup banner and any early failure happen before a file can
// be opened. Lines are buffered until then, and that buffering is what these
// check, since a sink that only starts working after init drops exactly the
// lines you need when the failure is at startup.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

std::filesystem::path temp_log(const char *name) {
  return std::filesystem::temp_directory_path() / name;
}

} // namespace

int main() {
  std::printf("=== log file tests ===\n\n");

  // Logged before any file exists. These are the ones that matter most and the
  // ones a naive sink loses.
  log_info("early line one");
  log_warn("early line two");

  const auto path = temp_log("afh_log_file_test.txt");
  std::filesystem::remove(path);
  afterhours::log_file::open(path.string().c_str());

  log_info("after open");

  const std::string contents = read_all(path);
  check(contents.find("early line one") != std::string::npos,
        "a line logged before the file opened is still in it");
  check(contents.find("early line two") != std::string::npos,
        "the second buffered line too");
  check(contents.find("after open") != std::string::npos,
        "and lines logged afterwards");
  check(contents.find("[INFO]") != std::string::npos,
        "the level is written");

  // Order matters: a report read top to bottom should be chronological.
  const auto first = contents.find("early line one");
  const auto second = contents.find("early line two");
  const auto third = contents.find("after open");
  check(first < second && second < third, "in the order they were logged");

  // Per line, not per buffer. The log is most useful when the process dies,
  // which is exactly when an unflushed tail is lost -- so the file has to be
  // complete without anyone closing it.
  log_error("written but not closed");
  check(read_all(path).find("written but not closed") != std::string::npos,
        "each line is on disk without a close");

  afterhours::log_file::close();

  // Reopening truncates rather than appending, so a run's log is that run's.
  afterhours::log_file::open(path.string().c_str());
  log_info("second run");
  const std::string reopened = read_all(path);
  check(reopened.find("second run") != std::string::npos,
        "the reopened file has the new line");
  check(reopened.find("early line one") == std::string::npos,
        "and not the previous run's");
  afterhours::log_file::close();

  // The pre-init buffer is capped and keeps the recent lines: the startup
  // failure is at the end of the buffer, so the oldest lines drop first.
  for (int i = 0; i < 600; i++)
    log_info("cap line {}", i);
  const auto cap_path = temp_log("afh_log_file_cap_test.txt");
  std::filesystem::remove(cap_path);
  afterhours::log_file::open(cap_path.string().c_str());
  const std::string capped = read_all(cap_path);
  check(capped.find("cap line 599") != std::string::npos,
        "the most recent buffered line survives the cap");
  check(capped.find("cap line 0") == std::string::npos,
        "the oldest buffered line dropped once the cap filled");
  int kept = 0;
  for (std::size_t at = capped.find("cap line "); at != std::string::npos;
       at = capped.find("cap line ", at + 1))
    kept++;
  check(kept == 512, "exactly the capped number of lines replay");
  check(capped.find("cap line 88") < capped.find("cap line 599") &&
            capped.find("cap line 88") != std::string::npos,
        "the kept lines replay in order");
  afterhours::log_file::close();
  std::filesystem::remove(cap_path);

  // A host's own sink still wins: logging.h calls whatever log_sink_fn
  // points at now, so assigning a different one takes over.
  static std::string captured;
  captured.clear();
  log_sink_fn = [](const char *, const char *m) {
    captured += m;
    captured += "\n";
  };
  log_info("for the replacement sink");
  check(captured.find("for the replacement sink") != std::string::npos,
        "a replacement sink receives the line");
  log_sink_fn = afterhours::log_file::detail::write_line;

  // A path that cannot be opened must not take the process with it.
  afterhours::log_file::open("/definitely/not/a/directory/log.txt");
  log_info("still alive");
  check(true, "an unopenable path is survivable");

  afterhours::log_file::close();

  // Concurrent writers, before and after open: every line lands intact.
  const auto conc_path = temp_log("afh_log_file_conc_test.txt");
  std::filesystem::remove(conc_path);
  constexpr int threads = 8;
  constexpr int per_thread = 50;
  auto run_threads = [](const char *tag) {
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
      pool.emplace_back([tag, t] {
        for (int i = 0; i < per_thread; i++)
          log_info("{}-{}-{}", tag, t, i);
      });
    for (auto &th : pool)
      th.join();
  };
  run_threads("pre");
  afterhours::log_file::open(conc_path.string().c_str());
  run_threads("post");
  afterhours::log_file::close();
  {
    std::istringstream in(read_all(conc_path));
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line))
      lines.push_back(line);
    auto expect = [&](const char *tag) {
      for (int t = 0; t < threads; t++)
        for (int i = 0; i < per_thread; i++) {
          const std::string want =
              "[INFO] " + std::string(tag) + "-" + std::to_string(t) + "-" +
              std::to_string(i);
          bool found = false;
          for (const auto &got : lines)
            if (got == want)
              found = true;
          check(found, "every concurrent line present and intact");
        }
    };
    expect("pre");
    expect("post");
  }
  std::filesystem::remove(conc_path);

  std::filesystem::remove(path);

  std::printf("\n%d/%d checks passed\n", checks_run - checks_failed,
              checks_run);
  if (checks_failed == 0)
    std::printf("All checks passed!\n");
  return checks_failed == 0 ? 0 : 1;
}
