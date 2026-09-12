#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace afterhours::file_watcher {

struct Event {
    std::filesystem::path path;
    bool must_rescan = false;
};

struct Options {
    double latency_seconds = 0.1;
    std::size_t max_pending_events = 4096;
};

struct Started {};
struct Unsupported {};
struct Error { std::string message; };
using StartResult = std::variant<Started, Unsupported, Error>;

class Watcher {
 public:
    Watcher();
    ~Watcher();
    Watcher(const Watcher &) = delete;
    Watcher &operator=(const Watcher &) = delete;
    StartResult start(const std::vector<std::filesystem::path> &roots,
                      Options options = {});
    void stop();
    std::vector<Event> drain();

 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
