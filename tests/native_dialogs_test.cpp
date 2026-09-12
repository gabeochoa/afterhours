#include <afterhours/src/plugins/native_dialogs.h>
#include <afterhours/src/plugins/e2e_testing/native_dialog_responses.h>
#include <algorithm>
#include <cassert>
#include <stdexcept>

int main() {
    using namespace afterhours::native_dialogs;
    afterhours::testing::NativeDialogResponses responses;
    responses.push(Selected{std::filesystem::path("/tmp/é space.txt")});
    responses.push(Cancelled{});
    responses.push(Unsupported{});
    responses.push(Error{"test failure"});
    Queue queue(std::ref(responses));
    for (std::size_t i = 0; i < 5; ++i) {
        auto id = queue.submit({});
        assert(!queue.take_result(id));
        queue.process_pending();
        auto result = queue.take_result(id);
        assert(result && result->index() == std::min(i, std::size_t{3}));
        if (auto *selected = std::get_if<Selected>(&*result))
            assert(selected->path == "/tmp/é space.txt");
        assert(!queue.take_result(id));
    }
    int calls = 0;
    Queue *active = nullptr;
    RequestId nested = 0;
    Queue reentrant([&](const Request &request) -> Result {
        ++calls;
        assert(request.kind == Kind::SaveFile);
        if (calls == 1) nested = active->submit(request);
        active->process_pending();
        assert(!active->take_result(1));
        return Cancelled{};
    });
    active = &reentrant;
    auto first = reentrant.submit({.kind = Kind::SaveFile});
    reentrant.process_pending();
    assert(calls == 1 && reentrant.take_result(first));
    reentrant.process_pending();
    assert(calls == 2 && reentrant.take_result(nested));
    Queue throws([](const Request &) -> Result { throw std::runtime_error("failure"); });
    auto failed = throws.submit({});
    throws.process_pending();
    assert(std::holds_alternative<Error>(*throws.take_result(failed)));
    return 0;
}
