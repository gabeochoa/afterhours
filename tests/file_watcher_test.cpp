#include <afterhours/src/plugins/file_watcher.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <thread>

int main() {
    using namespace afterhours::file_watcher;
    namespace fs = std::filesystem;
    Watcher watcher;
#ifndef __APPLE__
    assert(std::holds_alternative<Unsupported>(watcher.start({"."})));
#else
    auto root = fs::temp_directory_path() / ("afh-watch-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "other");
    root = fs::canonical(root);
    assert(std::holds_alternative<Error>(watcher.start({root / "missing"})));
    for (int i = 0; i < 20; ++i) {
        Watcher short_lived;
        assert(std::holds_alternative<Started>(short_lived.start({root})));
    }
    auto await_event = [&](const fs::path &path) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < until) {
            for (const auto &event : watcher.drain())
                if (event.must_rescan || event.path == path) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    };
    assert(std::holds_alternative<Started>(watcher.start({root, root / "other"})));
    const auto file = root / "é space.txt";
    { std::ofstream out(file); out << "created"; }
    assert(await_event(file));
    { std::ofstream out(file); out << "modified"; }
    assert(await_event(file));
    auto renamed = root / "other" / "renamed.txt";
    fs::rename(file, renamed);
    assert(await_event(renamed));
    fs::remove(renamed);
    assert(await_event(renamed));
    watcher.stop();
    watcher.stop();
    assert(watcher.drain().empty());
    assert(std::holds_alternative<Started>(watcher.start({root}, {.max_pending_events = 1})));
    for (int i = 0; i < 20; ++i) { std::ofstream out(root / std::to_string(i)); out << i; }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto events = watcher.drain();
    assert(events.size() == 1 && events[0].must_rescan);
    watcher.stop();
    fs::remove_all(root);
#endif
}
