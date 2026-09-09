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

  // A path that cannot be opened must not take the process with it.
  afterhours::log_file::open("/definitely/not/a/directory/log.txt");
  log_info("still alive");
  check(true, "an unopenable path is survivable");

  std::filesystem::remove(path);

  std::printf("\n%d/%d checks passed\n", checks_run - checks_failed,
              checks_run);
  if (checks_failed == 0)
    std::printf("All checks passed!\n");
  return checks_failed == 0 ? 0 : 1;
}
