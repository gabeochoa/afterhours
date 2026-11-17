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
#include <stdexcept>

struct Data {
    std::string value;
    bool fail = false;
    json to_json() const {
        if (fail) throw std::runtime_error("serialization failure");
        return {value};
    }
    std::string to_string() const {
        if (fail) throw std::runtime_error("serialization failure");
        return value;
    }
};

int main() {
    using afterhours::files;
    using afterhours::settings;
    auto dir = std::filesystem::temp_directory_path() /
               ("afterhours-settings-" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    int failures = 0;
    auto check = [&](bool ok, const char *what) {
        if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
    };
    auto has_temp = [&] {
        const std::string suffix = files::TEMP_SUFFIX;
        for (const auto &e : std::filesystem::directory_iterator(dir)) {
            const std::string name = e.path().filename().string();
            if (name.size() > suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                return true;
        }
        return false;
    };
    settings::ProvidesSettings<Data> provider;
    for (auto save : {&settings::save_to_json<Data>,
                      &settings::save_to_raw_string<Data>}) {
        auto path = dir / "settings";
        check(files::write_string_atomic(path, "last good"), "seed file");
        provider.data = {"new", true};
        check(!save(&provider, path), "throwing serializer returns failure");
        check(files::read_string(path) == "last good", "serialization preserves file");
        provider.data.fail = false;
        using std::filesystem::perms;
        std::filesystem::permissions(dir, perms::owner_read | perms::owner_exec);
        const bool saved_read_only = save(&provider, path);
        std::filesystem::permissions(dir, perms::owner_all);
        check(!saved_read_only, "read-only directory returns failure");
        check(files::read_string(path) == "last good", "failed open preserves file");
        check(!has_temp(), "failed open leaves no temporary file");
        auto blocked = dir / "directory";
        std::filesystem::create_directory(blocked);
        check(!save(&provider, blocked), "rename over directory returns failure");
        check(std::filesystem::is_directory(blocked), "directory survives");
        check(!has_temp(), "failed rename removes temporary file");
        check(save(&provider, path), "save succeeds");
        const auto expected = save == &settings::save_to_json<Data>
            ? provider.data.to_json().dump(2) : provider.data.to_string();
        check(files::read_string(path) == expected, "saved content matches");
    }
    std::filesystem::remove_all(dir);
    return failures ? 1 : 0;
}
