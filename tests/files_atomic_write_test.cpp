// files_atomic_write_test.cpp
// files::write_string_atomic / read_string. The interesting cases are the
// failure ones: a failed write must leave the original intact and drop its
// temp.

#include <afterhours/src/plugins/files.h>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using afterhours::files;

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

static bool has_temp_siblings(const fs::path &dir) {
  const std::string suffix = files::TEMP_SUFFIX;
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(dir, ec)) {
    const std::string name = e.path().filename().string();
    if (name.size() > suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
      return true;
  }
  return false;
}

int main() {
  printf("=== files atomic write tests ===\n\n");

  std::error_code ec;
  const fs::path root = fs::temp_directory_path(ec) /
                       ("afh_atomic_write_test_" + std::to_string(getpid()));
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);

  // 1. Round trip.
  {
    const fs::path p = root / "settings.json";
    check(files::write_string_atomic(p, "{\"a\":1}"), "write succeeds");
    auto got = files::read_string(p);
    check(got.has_value() && *got == "{\"a\":1}", "read returns what was written");
    check(!has_temp_siblings(root), "no .tmp is left behind on success");
  }

  // 2. Overwrite replaces rather than appends.
  {
    const fs::path p = root / "settings.json";
    check(files::write_string_atomic(p, "second"), "overwrite succeeds");
    auto got = files::read_string(p);
    check(got.has_value() && *got == "second", "overwrite replaces contents");
    check(!has_temp_siblings(root), "no .tmp after overwrite");
  }

  // 3. Missing file reads as nullopt so callers can fall back to defaults.
  {
    auto got = files::read_string(root / "does_not_exist.json");
    check(!got.has_value(), "reading a missing file returns nullopt");
  }

  // 4. Parent directories are created.
  {
    const fs::path p = root / "nested" / "deeper" / "f.txt";
    check(files::write_string_atomic(p, "x"), "write creates parent dirs");
    check(fs::exists(p, ec), "nested file exists");
  }

  // 5. Clearing a file is a real operation, not a no-op.
  {
    const fs::path p = root / "empty.txt";
    check(files::write_string_atomic(p, ""), "empty write succeeds");
    auto got = files::read_string(p);
    check(got.has_value() && got->empty(), "empty file reads back as empty");
  }

  // 6. A c_str()-based implementation would truncate at the first NUL.
  {
    const fs::path p = root / "binary.bin";
    const std::string payload("a\0b\0c", 5);
    check(files::write_string_atomic(p, payload), "binary write succeeds");
    auto got = files::read_string(p);
    check(got.has_value() && *got == payload && got->size() == 5,
          "embedded NULs survive the round trip");
  }

  // 7. Target is a directory, so the rename fails after the temp is written --
  //    the window where a plain ofstream would already have truncated the file.
  {
    const fs::path good = root / "keepme.json";
    check(files::write_string_atomic(good, "original"), "seed write succeeds");

    const fs::path blocked = root / "blocked";
    fs::create_directories(blocked, ec);
    check(!files::write_string_atomic(blocked, "nope"),
          "writing over a directory fails");
    check(fs::is_directory(blocked, ec), "the directory is left alone");
    check(!has_temp_siblings(root), "the temp is cleaned up after a failure");

    auto got = files::read_string(good);
    check(got.has_value() && *got == "original",
          "an unrelated file is untouched by the failed write");
  }

  {
    const fs::path p = root / "recover.json";
    check(files::write_string_atomic(p, "good"), "seed write succeeds");
    const std::string stale = p.string() + files::TEMP_SUFFIX;
    {
      std::ofstream out(stale, std::ios::binary);
      out << "half-written gar";
    }
    check(files::write_string_atomic(p, "newer"), "next write succeeds");
    auto after = files::read_string(p);
    check(after.has_value() && *after == "newer", "the new contents landed");
    check(fs::exists(stale, ec), "the stale temp is not someone else's write");
    check(files::sweep_temp_files(root) == 1, "sweep reclaims the stale temp");
  }

  // 9. Write-once paths never get a second write, so their temps would sit
  //    there forever -- sweep_temp_files is what actually reclaims them. It
  //    must not touch a caller's own .tmp, or the target files.
  {
    const fs::path dir = root / "cache";
    fs::create_directories(dir / "nested", ec);
    check(files::write_string_atomic(dir / "tx_1.json", "one"), "seed tx_1");
    {
      std::ofstream a(dir / ("tx_2.json" + std::string(files::TEMP_SUFFIX)));
      a << "orphan";
      std::ofstream b(dir / "nested" /
                      ("tx_3.json" + std::string(files::TEMP_SUFFIX)));
      b << "orphan";
      std::ofstream c(dir / "user_owned.tmp");
      c << "not ours";
    }

    const int removed = files::sweep_temp_files(dir);
    check(removed == 2, "sweep removes both orphaned temps, including nested");
    check(fs::exists(dir / "user_owned.tmp", ec),
          "sweep leaves a caller's own .tmp alone");
    auto kept = files::read_string(dir / "tx_1.json");
    check(kept.has_value() && *kept == "one", "sweep leaves real files alone");
    check(files::sweep_temp_files(root / "no_such_dir") == 0,
          "sweeping a missing dir is a no-op");
  }

  {
    const fs::path dir = root / "race";
    const fs::path p = dir / "shared.json";
    constexpr int writers = 8;
    constexpr int rounds = 4;
    std::vector<std::string> payloads;
    for (int i = 0; i < writers; i++)
      payloads.emplace_back(256 * 1024, static_cast<char>('a' + i));
    auto is_payload = [&](const std::string &s) {
      for (const auto &want : payloads)
        if (s == want)
          return true;
      return false;
    };

    std::atomic<int> failed_writes{0};
    std::atomic<int> torn_reads{0};
    std::atomic<bool> writing{true};
    std::thread reader([&] {
      while (writing) {
        auto got = files::read_string(p);
        if (got.has_value() && !is_payload(*got))
          torn_reads++;
      }
    });
    std::vector<std::thread> threads;
    for (int i = 0; i < writers; i++)
      threads.emplace_back([&, i] {
        for (int r = 0; r < rounds; r++)
          if (!files::write_string_atomic(p, payloads[i]))
            failed_writes++;
      });
    for (auto &t : threads)
      t.join();
    writing = false;
    reader.join();

    check(failed_writes == 0, "every competing write succeeds");
    check(torn_reads == 0, "a reader never sees a mixed or partial file");
    auto got = files::read_string(p);
    check(got.has_value() && is_payload(*got),
          "the target holds exactly one writer's payload");
    check(!has_temp_siblings(dir), "competing writers leave no temps");
  }

  {
    const fs::path dir = root / "full";
    const fs::path p = dir / "save.json";
    check(files::write_string_atomic(p, "original"), "seed write succeeds");

    rlimit old{};
    getrlimit(RLIMIT_FSIZE, &old);
    rlimit small = old;
    small.rlim_cur = 4096;
    std::signal(SIGXFSZ, SIG_IGN);
    setrlimit(RLIMIT_FSIZE, &small);
    const bool ok = files::write_string_atomic(p, std::string(64 * 1024, 'x'));
    setrlimit(RLIMIT_FSIZE, &old);
    std::signal(SIGXFSZ, SIG_DFL);

    check(!ok, "a write that runs out of space reports failure");
    auto got = files::read_string(p);
    check(got.has_value() && *got == "original",
          "the original survives a failed write");
    check(!has_temp_siblings(dir), "the failed write removes its temp");
  }

  {
    const fs::path p = root / "private.json";
    check(files::write_string_atomic(p, "secret"), "seed private file");
    fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write, ec);
    check(files::write_string_atomic(p, "secret2"), "rewrite private file");
    check((fs::status(p, ec).permissions() & fs::perms::all) ==
              (fs::perms::owner_read | fs::perms::owner_write),
          "a 0600 file stays 0600 after a replace");
  }

  {
    const fs::path dir = root / "sweep_live";
    fs::create_directories(dir, ec);
    const pid_t child = fork();
    if (child == 0)
      _exit(0);
    waitpid(child, nullptr, 0);
    const std::string suffix = files::TEMP_SUFFIX;
    const fs::path live =
        dir / ("a.json." + std::to_string(getpid()) + ".0" + suffix);
    const fs::path dead =
        dir / ("b.json." + std::to_string(child) + ".0" + suffix);
    const fs::path legacy = dir / ("c.json" + suffix);
    for (const auto &f : {live, dead, legacy})
      std::ofstream(f) << "temp";

    check(files::sweep_temp_files(dir) == 2,
          "sweep removes the dead writer's and the legacy temp");
    check(fs::exists(live, ec), "a live writer's temp is left alone");
    check(!fs::exists(dead, ec), "a dead writer's temp is swept");
    check(!fs::exists(legacy, ec), "the legacy fixed-name temp is swept");
  }

  fs::remove_all(root, ec);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
