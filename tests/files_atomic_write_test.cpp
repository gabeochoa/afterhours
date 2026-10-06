// files_atomic_write_test.cpp
// files::write_string_atomic / read_string. The interesting cases are the
// failure ones: a failed write must leave the original intact and drop its
// temp.

#include <afterhours/src/plugins/files.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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

// Our temps carry TEMP_SUFFIX inside the name (`x.json.afh-tmp.<pid>.<n>`);
// matching on the extension never found them, so these checks always passed.
static bool has_temp_siblings(const fs::path &dir) {
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(dir, ec))
    if (e.path().filename().string().find(files::TEMP_SUFFIX) != std::string::npos)
      return true;
  return false;
}

int main() {
  printf("=== files atomic write tests ===\n\n");

  std::error_code ec;
  const fs::path root = fs::temp_directory_path(ec) / "afh_atomic_write_test";
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

  // 8. A temp left by an earlier crash is consumed by the next write to the
  //    same path.
  {
    const fs::path p = root / "recover.json";
    check(files::write_string_atomic(p, "good"), "seed write succeeds");
    {
      std::ofstream stale(p.string() + files::TEMP_SUFFIX, std::ios::binary);
      stale << "half-written gar";
    }
    check(fs::exists(p.string() + files::TEMP_SUFFIX, ec), "a stale temp exists");
    auto before = files::read_string(p);
    check(before.has_value() && *before == "good",
          "the stale temp does not affect the real file");

    check(files::write_string_atomic(p, "newer"), "next write succeeds");
    check(!fs::exists(p.string() + files::TEMP_SUFFIX, ec),
          "the next write consumes the stale temp");
    auto after = files::read_string(p);
    check(after.has_value() && *after == "newer", "the new contents landed");
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

  // 10. Competing writers: each write has its own temp, so the file always
  //     holds one writer's complete payload and no temp survives.
  {
    const fs::path p = root / "contended.json";
    std::atomic<int> failures{0};
    auto writer = [&](char fill) {
      for (int i = 0; i < 100; i++)
        if (!files::write_string_atomic(p, std::string(64 * 1024, fill)))
          failures++;
    };
    std::thread a(writer, 'a'), b(writer, 'b');
    a.join();
    b.join();
    auto got = files::read_string(p);
    check(failures == 0, "every competing write succeeds");
    check(got && got->size() == 64 * 1024 &&
              (*got == std::string(64 * 1024, 'a') || *got == std::string(64 * 1024, 'b')),
          "the file holds one writer's whole payload, never a mix");
    check(!has_temp_siblings(root), "no temp survives competing writers");
  }

  // 11. Replacing a 0600 file keeps it 0600.
  {
    const fs::path p = root / "private.json";
    check(files::write_string_atomic(p, "secret"), "seed private file");
    fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write, ec);
    check(files::write_string_atomic(p, "secret2"), "rewrite private file");
    check((fs::status(p, ec).permissions() & fs::perms::all) ==
              (fs::perms::owner_read | fs::perms::owner_write),
          "permissions survive the replace");
  }

#ifndef _WIN32
  // 12. Disk full, simulated by a file-size limit: the write fails, the
  //     original is intact and the partial temp is gone.
  {
    const fs::path p = root / "full.json";
    check(files::write_string_atomic(p, "original"), "seed before disk full");
    struct rlimit before {};
    getrlimit(RLIMIT_FSIZE, &before);
    signal(SIGXFSZ, SIG_IGN);
    struct rlimit small = before;
    small.rlim_cur = 1024;
    setrlimit(RLIMIT_FSIZE, &small);
    const bool wrote = files::write_string_atomic(p, std::string(64 * 1024, 'x'));
    setrlimit(RLIMIT_FSIZE, &before);
    check(!wrote, "a write past the size limit fails");
    auto got = files::read_string(p);
    check(got && *got == "original", "the original survives a full disk");
    check(!has_temp_siblings(root), "the partial temp is removed");
  }

  // 13. Interruption: a writer killed mid-write leaves the old file or the
  //     new one whole, and its temp is reclaimed by the sweep.
  {
    const fs::path dir = root / "killed";
    fs::create_directories(dir, ec);
    const fs::path p = dir / "state.json";
    check(files::write_string_atomic(p, "old"), "seed before interruption");
    const std::string big(64 * 1024 * 1024, 'n');
    const pid_t child = fork();
    if (child == 0) {
      files::write_string_atomic(p, big);
      _exit(0);
    }
    usleep(20 * 1000);
    kill(child, SIGKILL);
    int status = 0;
    waitpid(child, &status, 0);
    auto got = files::read_string(p);
    check(got && (*got == "old" || *got == big), "an interrupted write leaves old or whole new contents");
    files::sweep_temp_files(dir);
    check(!has_temp_siblings(dir), "the killed writer's temp is swept");
  }

  // 14. The sweep spares a live writer's temp and takes a dead one's.
  {
    const fs::path dir = root / "sweep_live";
    fs::create_directories(dir, ec);
    const std::string live = "a.json" + std::string(files::TEMP_SUFFIX) + "." +
                             std::to_string(getpid()) + ".0";
    const std::string dead = "b.json" + std::string(files::TEMP_SUFFIX) + ".999999.0";
    { std::ofstream(dir / live) << "writing"; }
    { std::ofstream(dir / dead) << "orphan"; }
    files::sweep_temp_files(dir);
    check(fs::exists(dir / live, ec), "a live writer's temp is left alone");
    check(!fs::exists(dir / dead, ec), "a dead writer's temp is swept");
  }
#endif

  fs::remove_all(root, ec);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
