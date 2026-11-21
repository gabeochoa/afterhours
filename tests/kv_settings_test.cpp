// kv_settings_test.cpp — kv_settings round-trip, defaults, corrupt lines,
// atomic save. Small games' settings without JSON.

#include <afterhours/src/plugins/kv_settings.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using afterhours::files;
using afterhours::kv_settings;

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

int main() {
  printf("=== kv settings tests ===\n\n");

  const fs::path dir = fs::temp_directory_path() /
      ("afterhours-kv-" + std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(dir);
  const fs::path path = dir / "settings.kv";

  // Round-trip: '=' and '#' inside values, interior spaces, empty value.
  kv_settings s;
  s.set("name", "pumpkin beachball");
  s.set("expr", "a = b # c");
  s.set("tag", "#notacomment");
  s.set("empty", "");
  s.set("level", "42");
  s.set("volume", "0.25");
  s.set("muted", "on");
  check(s.save(path), "save succeeds");
  const kv_settings loaded = kv_settings::load(path);
  check(loaded.entries == s.entries, "round-trip preserves all entries");
  check(loaded.get("expr") == "a = b # c", "value with '=' and '#' survives");
  check(loaded.get("tag") == "#notacomment", "leading '#' value survives");
  check(loaded.get("empty") == "", "empty value survives");
  check(loaded.get_int("level", 7) == 42, "get_int parses");
  check(loaded.get_double("volume", 9.0) == 0.25, "get_double parses");
  check(loaded.get_bool("muted", false), "get_bool parses 'on'");

  // Defaults: missing keys fall back, garbage falls back, missing file is empty.
  check(s.get("nope", "dflt") == "dflt", "missing key returns fallback");
  check(s.get_int("nope", 7) == 7, "missing int returns fallback");
  kv_settings bad;
  bad.set("i", "twelvex");
  bad.set("d", "1.2.3");
  bad.set("b", "maybe");
  check(bad.get_int("i", 7) == 7, "garbage int returns fallback");
  check(bad.get_double("d", 9.0) == 9.0, "garbage double returns fallback");
  check(bad.get_bool("b", true), "garbage bool returns fallback");
  const kv_settings missing = kv_settings::load(dir / "absent.kv");
  check(missing.entries.empty(), "missing file loads empty");
  check(missing.get_int("level", 3) == 3, "missing file gives defaults");

  // Parse: comments, blank lines, corrupt lines, trimming, duplicates.
  std::map<std::string, std::string> m;
  kv_settings::parse_into(
      "# comment\n  # indented comment\n\nk1=v1\n naked line\n spaced =  hi "
      "there \na=first\na=second\n= no key\nc=3", m);
  check(m.size() == 4, "comments/blank/corrupt lines skipped");
  check(m["k1"] == "v1", "bare key=value parses");
  check(m["spaced"] == "hi there", "key and value are trimmed");
  check(m["a"] == "second", "duplicate key: last wins");
  check(!m.contains("naked line"), "line without '=' skipped");

  // Atomic save: overwriting a valid file leaves a valid file.
  kv_settings s2 = loaded;
  s2.set("level", "99");
  check(s2.save(path), "second save succeeds");
  const kv_settings reloaded = kv_settings::load(path);
  check(reloaded.get("level") == "99", "reloaded file has new value");
  check(reloaded.get("name") == "pumpkin beachball", "reloaded file keeps old values");
  bool temp_left = false;
  for (const auto &e : fs::directory_iterator(dir)) {
    const std::string name = e.path().filename().string();
    if (name.find(files::TEMP_SUFFIX) != std::string::npos)
      temp_left = true;
  }
  check(!temp_left, "no temp siblings left after save");

  // erase
  kv_settings s3 = loaded;
  check(s3.erase("level") && !s3.has("level"), "erase removes key");
  check(!s3.erase("level"), "erase of absent key is false");

  fs::remove_all(dir);
  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
