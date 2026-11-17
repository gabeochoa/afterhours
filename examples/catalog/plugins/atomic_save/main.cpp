#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "../../../../ah.h"
#include "../../../../src/plugins/files.h"

using afterhours::files;
namespace fs = std::filesystem;

int main() {
    const fs::path dir = fs::temp_directory_path() /
                         ("afh_atomic_save_" + std::to_string(
                             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    const fs::path save = dir / "save.json";
    bool ok = true;

    ok &= files::write_string_atomic(save, "{\"level\":1}");
    fs::permissions(save, fs::perms::owner_read | fs::perms::owner_write);
    ok &= files::write_string_atomic(save, "{\"level\":2}");
    const bool private_kept = (fs::status(save).permissions() & fs::perms::all) ==
                              (fs::perms::owner_read | fs::perms::owner_write);
    std::cout << "0600 kept after save: " << (private_kept ? "yes" : "no") << "\n";
    ok &= private_kept;

    const std::string autosave(100000, 'a');
    const std::string manual(100000, 'm');
    auto saver = [&](const std::string &payload, bool &result) {
        result = true;
        for (int i = 0; i < 20; i++)
            result &= files::write_string_atomic(save, payload);
    };
    bool autosave_ok = false;
    bool manual_ok = false;
    std::thread a(saver, std::cref(autosave), std::ref(autosave_ok));
    std::thread b(saver, std::cref(manual), std::ref(manual_ok));
    a.join();
    b.join();
    ok &= autosave_ok && manual_ok;

    const auto got = files::read_string(save);
    const bool whole = got && (*got == autosave || *got == manual);
    std::cout << "two threads saved; file is "
              << (whole ? (*got == autosave ? "the whole autosave" : "the whole manual save")
                        : "torn")
              << "\n";
    ok &= whole;

    fs::remove_all(dir);
    std::cout << (ok ? "atomic save example passed" : "atomic save example FAILED") << "\n";
    return ok ? 0 : 1;
}
