#define FMT_HEADER_ONLY
#include <chrono>
#include <istream>
#include <string>

struct TestJson {
    std::string text;
    std::string dump(int) const { return text; }
    friend std::istream &operator>>(std::istream &in, TestJson &value) {
        return in >> value.text;
    }
};

#define JSON_TYPE TestJson
#include <afterhours/src/plugins/settings.h>

#include <cstdio>
#include <filesystem>

struct Data {
    std::string value;
    std::string to_string() const { return value; }
    static Data from_string(const std::string &text) { return {text}; }
};

// The settings root is chosen by the files plugin (or the cwd without one);
// an app must be able to override it from code (cartographer, EXT:settings).
int main() {
    using afterhours::settings;
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() /
               ("afterhours-settings-path-" +
                std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    const std::string file_name = "settings_path_test.json";
    const auto cwd_file = fs::current_path() / file_name;
    fs::remove(cwd_file);
    int failures = 0;
    auto check = [&](bool ok, const char *what) {
        if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
    };

    settings::init<Data>("settings_path_test", file_name);

    settings::get_data<Data>().value = "default root";
    check(settings::save<Data>(), "save without override succeeds");
    check(fs::exists(cwd_file), "default root is the cwd without a files plugin");
    fs::remove(cwd_file);

    settings::set_path_override(dir);
    check(settings::get_save_path() == dir, "get_save_path reports the override");
    settings::get_data<Data>().value = "moved";
    check(settings::save<Data>(), "save with override succeeds");
    check(fs::exists(dir / file_name), "settings file lands in the override dir");
    check(!fs::exists(cwd_file), "nothing is written to the default root");

    settings::get_data<Data>().value = "stale";
    check(settings::load<Data>(), "load with override succeeds");
    check(settings::get_data<Data>().value == "moved",
          "load reads back the override dir's file");

    settings::clear_path_override();
    check(settings::get_save_path() == fs::current_path(),
          "clearing the override restores the default root");

    fs::remove(cwd_file);
    fs::remove_all(dir);
    return failures ? 1 : 0;
}
