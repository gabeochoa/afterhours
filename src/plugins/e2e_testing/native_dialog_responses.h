#pragma once

#include "../native_dialogs.h"

namespace afterhours::testing {

class NativeDialogResponses {
 public:
    void push(native_dialogs::Result result) {
        results_.push_back(std::move(result));
    }
    native_dialogs::Result operator()(const native_dialogs::Request &) {
        if (results_.empty())
            return native_dialogs::Error{"No queued test dialog response"};
        auto result = std::move(results_.front());
        results_.pop_front();
        return result;
    }

 private:
    std::deque<native_dialogs::Result> results_;
};

}
