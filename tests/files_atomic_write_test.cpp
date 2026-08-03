// files_atomic_write_test.cpp
// files::write_string_atomic / read_string. The interesting cases are the
// failure ones: a failed write must leave the original intact and drop its
// temp.

#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ".tmp")
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

  fs::remove_all(root, ec);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
