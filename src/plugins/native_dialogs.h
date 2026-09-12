#pragma once

#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
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

Result show_native(const Request &request);

class Queue {
 public:
    explicit Queue(Backend backend = show_native) : backend_(std::move(backend)) {}
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
        auto batch = std::exchange(pending_, {});
        for (const auto &[id, request] : batch) {
            try {
                auto result = backend_ ? backend_(request)
                                       : Result{Error{"No dialog backend"}};
                if (const auto *selected = std::get_if<Selected>(&result);
                    selected && selected->path.empty())
                    result = Error{"Dialog returned an empty selected path"};
                completed_.emplace(id, std::move(result));
            } catch (const std::exception &e) {
                completed_.emplace(id, Error{e.what()});
            }
        }
    }

    std::optional<Result> take_result(RequestId id) {
        if (processing_) return std::nullopt;
        auto node = completed_.extract(id);
        if (node.empty()) return std::nullopt;
        return std::move(node.mapped());
    }

 private:
    Backend backend_;
    RequestId next_id_ = 1;
    bool processing_ = false;
    std::deque<std::pair<RequestId, Request>> pending_;
    std::map<RequestId, Result> completed_;
};

}
