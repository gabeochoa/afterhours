#include "file_watcher.h"
#include <cmath>
#include <mutex>
#include <utility>

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#endif

namespace afterhours::file_watcher {

struct Watcher::Impl {
    std::mutex mutex;
    std::vector<Event> events;
    std::size_t limit = 4096;
    bool overflowed = false;
#ifdef __APPLE__
    FSEventStreamRef stream = nullptr;
    dispatch_queue_t queue = nullptr;

    static void changed(ConstFSEventStreamRef, void *context, std::size_t count,
                        void *raw_paths, const FSEventStreamEventFlags *flags,
                        const FSEventStreamEventId *) {
        auto &self = *static_cast<Impl *>(context);
        auto paths = static_cast<char **>(raw_paths);
        std::lock_guard lock(self.mutex);
        if (self.overflowed) return;
        for (std::size_t i = 0; i < count; ++i) {
            if (self.events.size() >= self.limit) {
                self.events.clear();
                self.events.push_back({{}, true});
                self.overflowed = true;
                return;
            }
            const auto rescan_flags = kFSEventStreamEventFlagMustScanSubDirs |
                kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped |
                kFSEventStreamEventFlagRootChanged;
            self.events.push_back({paths[i], (flags[i] & rescan_flags) != 0});
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
    auto paths = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);
    if (!paths) return Error{"Unable to allocate watch roots"};
    for (const auto &root : canonical) {
        auto path = CFStringCreateWithCString(nullptr, root.c_str(), kCFStringEncodingUTF8);
        if (!path) {
            CFRelease(paths);
            return Error{"Watch roots must be valid UTF-8"};
        }
        CFArrayAppendValue(paths, path);
        CFRelease(path);
    }
    FSEventStreamContext context{};
    context.info = impl_.get();
    impl_->limit = options.max_pending_events;
    impl_->stream = FSEventStreamCreate(nullptr, &Impl::changed, &context, paths,
        kFSEventStreamEventIdSinceNow, options.latency_seconds,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagWatchRoot);
    CFRelease(paths);
    if (!impl_->stream) return Error{"Unable to create file event stream"};
    impl_->queue = dispatch_queue_create("afterhours.file-watcher", DISPATCH_QUEUE_SERIAL);
    if (!impl_->queue) {
        stop();
        return Error{"Unable to create file event queue"};
    }
    FSEventStreamSetDispatchQueue(impl_->stream, impl_->queue);
    if (!FSEventStreamStart(impl_->stream)) {
        stop();
        return Error{"Unable to start file event stream"};
    }
    return Started{};
#endif
}

void Watcher::stop() {
#ifdef __APPLE__
    if (impl_->stream) {
        FSEventStreamStop(impl_->stream);
        FSEventStreamInvalidate(impl_->stream);
    }
    if (impl_->queue) dispatch_sync_f(impl_->queue, nullptr, [](void *) {});
    if (impl_->stream) FSEventStreamRelease(std::exchange(impl_->stream, nullptr));
    if (impl_->queue) dispatch_release(std::exchange(impl_->queue, nullptr));
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

}
