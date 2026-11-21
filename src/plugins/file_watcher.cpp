#include "file_watcher.h"

#include <cmath>
#include <mutex>
#include <utility>

#ifdef __APPLE__
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#endif

namespace afterhours::file_watcher {

struct Watcher::Impl {
    std::mutex mutex;
    std::vector<Event> events;
    std::size_t limit = 4096;
    bool overflowed = false;

    void push_event(std::filesystem::path path, bool must_rescan = false) {
        std::lock_guard lock(mutex);
        if (overflowed) return;
        if (events.size() >= limit) {
            events.clear();
            events.push_back({{}, true});
            overflowed = true;
            return;
        }
        events.push_back({std::move(path), must_rescan});
    }

#ifdef __APPLE__
    // kqueue, not FSEvents: FSEvents delivery depends on fseventsd, which
    // on a wedged daemon took 13-15s per change and dropped some outright.
    struct Signature {
        std::uintmax_t size = 0;
        std::int64_t mtime_ns = 0;
        bool operator==(const Signature &) const = default;
    };
    struct Watched {
        std::filesystem::path path;
        bool is_dir = false;
    };

    std::atomic<bool> running{false};
    std::thread thread;
    int kq = -1;
    std::unordered_map<int, Watched> by_fd;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, Signature>>
        snapshots;

    static Signature signature(const std::filesystem::path &path) {
        std::error_code ec;
        Signature sig;
        sig.size = std::filesystem::file_size(path, ec);
        if (ec) sig.size = 0;
        auto t = std::filesystem::last_write_time(path, ec);
        sig.mtime_ns =
            ec ? 0
               : std::chrono::duration_cast<std::chrono::nanoseconds>(
                     t.time_since_epoch())
                     .count();
        return sig;
    }

    bool is_watched(const std::filesystem::path &path) const {
        for (const auto &[fd, w] : by_fd)
            if (w.path == path) return true;
        return false;
    }

    bool watch_path(const std::filesystem::path &path, bool is_dir) {
        int fd = ::open(path.c_str(), O_EVTONLY | O_CLOEXEC);
        if (fd < 0) return false;
        struct kevent change {};
        const std::uint32_t flags =
            NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME |
            NOTE_DELETE | NOTE_REVOKE;
        EV_SET(&change, static_cast<std::uintptr_t>(fd), EVFILT_VNODE,
               EV_ADD | EV_CLEAR, flags, 0, nullptr);
        if (::kevent(kq, &change, 1, nullptr, 0, nullptr) < 0) {
            ::close(fd);
            return false;
        }
        by_fd[fd] = {path, is_dir};
        return true;
    }

    // Directory vnodes only say "something changed"; the snapshot diff names what.
    void scan_dir(const std::filesystem::path &dir, bool emit) {
        std::unordered_map<std::string, Signature> now;
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            std::error_code sec;
            if (entry.is_directory(sec)) {
                if (!is_watched(entry.path())) {
                    if (emit) push_event(entry.path());
                    watch_tree(entry.path());
                }
                continue;
            }
            if (!entry.is_regular_file(sec)) continue;
            now[entry.path().filename().string()] = signature(entry.path());
        }
        auto key = dir.string();
        auto old_it = snapshots.find(key);
        if (emit && old_it != snapshots.end()) {
            const auto &old = old_it->second;
            for (const auto &[name, sig] : now) {
                auto it = old.find(name);
                if (it == old.end() || !(it->second == sig))
                    push_event(dir / name);
            }
            for (const auto &[name, sig] : old)
                if (!now.count(name)) push_event(dir / name);
        }
        // Files are watched too: a directory vnode does not fire on content changes.
        for (const auto &[name, sig] : now) {
            auto path = dir / name;
            if (!is_watched(path)) watch_path(path, false);
        }
        snapshots[key] = std::move(now);
    }

    void watch_tree(const std::filesystem::path &dir) {
        if (!is_watched(dir)) watch_path(dir, true);
        if (!snapshots.count(dir.string())) snapshots[dir.string()] = {};
        scan_dir(dir, false);
    }

    void drop_fd(int fd) {
        auto it = by_fd.find(fd);
        if (it == by_fd.end()) return;
        if (it->second.is_dir) snapshots.erase(it->second.path.string());
        by_fd.erase(it);
        ::close(fd);
    }

    void run() {
        struct kevent out[64];
        while (running.load()) {
            struct timespec timeout {0, 200 * 1000 * 1000};
            int n = ::kevent(kq, nullptr, 0, out, 64, &timeout);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            for (int i = 0; i < n; ++i) {
                int fd = static_cast<int>(out[i].ident);
                auto it = by_fd.find(fd);
                if (it == by_fd.end()) continue;
                auto watched = it->second;
                std::uint32_t flags = out[i].fflags;
                if (flags & (NOTE_DELETE | NOTE_RENAME | NOTE_REVOKE)) {
                    push_event(watched.path);
                    drop_fd(fd);
                    continue;
                }
                if (watched.is_dir) {
                    scan_dir(watched.path, true);
                } else {
                    push_event(watched.path);
                    // Keep the parent snapshot in sync, or its scan re-reports this.
                    auto parent = watched.path.parent_path();
                    auto sit = snapshots.find(parent.string());
                    if (sit != snapshots.end())
                        sit->second[watched.path.filename().string()] =
                            signature(watched.path);
                }
            }
        }
    }

    void start_native(const std::vector<std::filesystem::path> &roots) {
        kq = ::kqueue();
        if (kq < 0) return;
        for (const auto &root : roots) watch_tree(root);
        running.store(true);
        thread = std::thread([this] { run(); });
    }

    void stop_native() {
        running.store(false);
        if (thread.joinable()) thread.join();
        for (auto &[fd, w] : by_fd) ::close(fd);
        by_fd.clear();
        snapshots.clear();
        if (kq >= 0) {
            ::close(kq);
            kq = -1;
        }
    }
#endif
};

Watcher::Watcher() : impl_(std::make_unique<Impl>()) {}
Watcher::~Watcher() { stop(); }

StartResult Watcher::start(const std::vector<std::filesystem::path> &roots,
                           Options options) {
    stop();
#ifndef __APPLE__
    (void)roots;
    (void)options;
    return Unsupported{};
#else
    if (roots.empty()) return Error{"At least one watch root is required"};
    if (!std::isfinite(options.latency_seconds) || options.latency_seconds < 0.0 ||
        options.max_pending_events == 0) return Error{"Invalid watcher options"};
    std::vector<std::filesystem::path> canonical;
    for (const auto &root : roots) {
        std::error_code error;
        auto path = std::filesystem::canonical(root, error);
        if (error) return Error{error.message()};
        canonical.push_back(std::move(path));
    }
    impl_->limit = options.max_pending_events;
    impl_->start_native(canonical);
    if (impl_->kq < 0) return Error{"Unable to create kqueue"};
    return Started{};
#endif
}

void Watcher::stop() {
#ifdef __APPLE__
    impl_->stop_native();
#endif
    std::lock_guard lock(impl_->mutex);
    impl_->events.clear();
    impl_->overflowed = false;
}

std::vector<Event> Watcher::drain() {
    std::lock_guard lock(impl_->mutex);
    impl_->overflowed = false;
    return std::exchange(impl_->events, {});
}

}  // namespace afterhours::file_watcher
