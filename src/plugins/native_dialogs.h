#pragma once

#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace afterhours::native_dialogs {

enum class Kind { OpenFile, SaveFile, Directory };

struct Request {
    Kind kind = Kind::OpenFile;
    std::string title;
    std::filesystem::path directory;
    std::string default_name;
    std::vector<std::string> extensions;
};

struct Selected { std::filesystem::path path; };
struct Cancelled {};
struct Unsupported {};
struct Error { std::string message; };
using Result = std::variant<Selected, Cancelled, Unsupported, Error>;
using RequestId = std::uint64_t;
using Backend = std::function<Result(const Request &)>;
using Completion = std::function<void(Result)>;
using CancelFn = std::function<void()>;
using AsyncBackend = std::function<CancelFn(const Request &, Completion)>;

Result show_native(const Request &request);
CancelFn show_native_async(const Request &request, Completion completion);
AsyncBackend native_async_backend();

class Queue {
 public:
    explicit Queue(Backend backend = show_native) : backend_(std::move(backend)) {}
    explicit Queue(AsyncBackend backend) : async_backend_(std::move(backend)) {}
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;

    RequestId submit(Request request) {
        const auto id = next_id_++;
        pending_.emplace_back(id, std::move(request));
        return id;
    }

    void process_pending() {
        if (processing_) return;
        struct Guard {
            bool &flag;
            ~Guard() { flag = false; }
        } guard{processing_};
        processing_ = true;
        drain_inbox();
        auto batch = std::exchange(pending_, {});
        for (const auto &[id, request] : batch) {
            if (async_backend_) {
                start_async(id, request);
            } else {
                try {
                    auto result = backend_ ? backend_(request)
                                           : Result{Error{"No dialog backend"}};
                    store_completed(id, std::move(result));
                } catch (const std::exception &e) {
                    store_completed(id, Error{e.what()});
                }
            }
        }
        drain_inbox();
    }

    void poll() { process_pending(); }

    bool cancel(RequestId id) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->first != id) continue;
            pending_.erase(it);
            store_completed(id, Cancelled{});
            return true;
        }
        CancelFn cancel_fn;
        {
            std::lock_guard lock(mutex_);
            auto it = cancels_.find(id);
            if (it == cancels_.end()) return false;
            cancel_fn = std::move(it->second);
            cancels_.erase(it);
            ignored_.insert(id);
        }
        store_completed(id, Cancelled{});
        if (cancel_fn) cancel_fn();
        return true;
    }

    std::optional<Result> take_result(RequestId id) {
        if (processing_) return std::nullopt;
        auto node = completed_.extract(id);
        if (node.empty()) return std::nullopt;
        return std::move(node.mapped());
    }

 private:
    void start_async(RequestId id, const Request &request) {
        {
            std::lock_guard lock(mutex_);
            cancels_.emplace(id, CancelFn{});
        }
        try {
            auto cancel_fn = async_backend_(request, [this, id](Result result) {
                std::lock_guard lock(mutex_);
                if (ignored_.erase(id)) return;
                inbox_.emplace_back(id, std::move(result));
            });
            std::lock_guard lock(mutex_);
            if (auto it = cancels_.find(id); it != cancels_.end())
                it->second = std::move(cancel_fn);
        } catch (const std::exception &e) {
            std::lock_guard lock(mutex_);
            cancels_.erase(id);
            inbox_.emplace_back(id, Error{e.what()});
        }
    }

    void drain_inbox() {
        std::deque<std::pair<RequestId, Result>> ready;
        {
            std::lock_guard lock(mutex_);
            ready = std::exchange(inbox_, {});
        }
        for (auto &[id, result] : ready) store_completed(id, std::move(result));
    }

    void store_completed(RequestId id, Result result) {
        if (const auto *selected = std::get_if<Selected>(&result);
            selected && selected->path.empty())
            result = Error{"Dialog returned an empty selected path"};
        completed_.emplace(id, std::move(result));
    }

    Backend backend_;
    AsyncBackend async_backend_;
    RequestId next_id_ = 1;
    bool processing_ = false;
    std::deque<std::pair<RequestId, Request>> pending_;
    std::map<RequestId, Result> completed_;
    std::mutex mutex_;
    std::deque<std::pair<RequestId, Result>> inbox_;
    std::map<RequestId, CancelFn> cancels_;
    std::set<RequestId> ignored_;
};

}
