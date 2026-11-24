#include "native_dialogs.h"

#ifndef __APPLE__
namespace afterhours::native_dialogs {
Result show_native(const Request &) { return Unsupported{}; }
CancelFn show_native_async(const Request &, Completion completion) {
    if (completion) completion(Unsupported{});
    return {};
}

AsyncBackend native_async_backend() {
    return [](const Request &, Completion completion) {
        if (completion) completion(Unsupported{});
        return CancelFn{};
    };
}
}
#endif
