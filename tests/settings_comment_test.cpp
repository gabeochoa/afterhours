// R-142: settings load->modify one value->save keeps comments/formatting
// elsewhere byte-stable; comments + trailing commas parse; invalid errors.
#include <afterhours/src/plugins/settings.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

struct Data {
  int volume = 50;
  std::string name = "hi";
  bool muted = false;
  static Data from_json(const json &j) {
    Data d; d.volume = j.value("volume", 50); d.name = j.value("name", std::string("hi"));
    d.muted = j.value("muted", false); return d;
  }
  json to_json() const { return {{"volume", volume}, {"name", name}, {"muted", muted}}; }
  std::string to_string() const { return name; }
};

static std::string read_all(const std::filesystem::path &p) {
  std::ifstream f(p); return {(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
}
static void write_all(const std::filesystem::path &p, const std::string &s) {
  std::ofstream f(p); f << s;
}

int main() {
  using namespace afterhours;
  int failures = 0;
  auto check = [&](bool ok, const char *w) { if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", w); } };
  auto dir = std::filesystem::temp_directory_path() / "afterhours-settings-comment-test";
  std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
  auto path = dir / "settings.json";

  const std::string original =
      "// user settings -- do not lose me\n"
      "{\n"
      "  // loudness\n"
      "  \"volume\": 50,\n"
      "  \"name\": \"hi\",   /* inline */\n"
      "  \"muted\": false,\n" // trailing comma tolerated
      "}\n";
  write_all(path, original);

  settings::ProvidesSettings<Data> provider{Data{}, "settings.json"};
  check(settings::load_from_json(&provider, path), "load with comments/trailing comma");
  check(provider.data.volume == 50 && provider.data.name == "hi", "values parsed");

  provider.data.volume = 80;
  check(settings::save_to_json(&provider, path), "save succeeds");
  std::string saved = read_all(path);
  check(saved.find("// user settings -- do not lose me") != std::string::npos, "header comment kept");
  check(saved.find("// loudness") != std::string::npos, "inner comment kept");
  check(saved.find("/* inline */") != std::string::npos, "inline comment kept");
  check(saved.find("\"volume\": 80") != std::string::npos, "changed value written");
  check(saved.find("\"name\": \"hi\",") != std::string::npos, "unchanged line byte-stable");
  check(saved.find("\"muted\": false,") != std::string::npos, "trailing-comma line byte-stable");

  // Invalid file still fails (returns false, data untouched).
  write_all(path, "{ \"volume\": oops,, }\n");
  provider.data.volume = 7;
  check(!settings::load_from_json(&provider, path), "invalid file returns failure");
  check(provider.data.volume == 7, "invalid load leaves data untouched");

  // Fresh file (no prior text) still saves plain JSON.
  settings::ProvidesSettings<Data> fresh{Data{}, "settings.json"};
  fresh.data.volume = 3;
  check(settings::save_to_json(&fresh, path), "fresh save succeeds");
  check(read_all(path).find("\"volume\": 3") != std::string::npos, "fresh content written");

  std::filesystem::remove_all(dir);
  if (!failures) std::printf("settings_comment_test PASS\n");
  return failures ? 1 : 0;
}
