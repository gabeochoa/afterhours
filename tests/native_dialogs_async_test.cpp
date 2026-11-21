#include <afterhours/src/plugins/native_dialogs.h>
#include <cassert>
#include <optional>
#include <utility>

int main() {
    using namespace afterhours::native_dialogs;

    std::optional<std::pair<Request, Completion>> captured;
    bool cancel_called = false;
    Queue queue(AsyncBackend{[&](const Request &req, Completion done) {
        captured.emplace(req, std::move(done));
        return CancelFn{[&] { cancel_called = true; }};
    }});

    auto id = queue.submit({.kind = Kind::OpenFile, .title = "Pick"});
    queue.process_pending();
    assert(captured && captured->first.title == "Pick");
    assert(!queue.take_result(id));

    captured->second(Selected{std::filesystem::path("/tmp/chosen.txt")});
    assert(!queue.take_result(id));
    queue.process_pending();
    auto result = queue.take_result(id);
    assert(result && std::holds_alternative<Selected>(*result));
    assert(std::get<Selected>(*result).path == "/tmp/chosen.txt");

    auto id2 = queue.submit({});
    queue.process_pending();
    captured->second(Cancelled{});
    queue.poll();
    assert(std::holds_alternative<Cancelled>(*queue.take_result(id2)));

    captured.reset();
    auto id3 = queue.submit({});
    assert(queue.cancel(id3));
    queue.process_pending();
    assert(!captured);
    assert(std::holds_alternative<Cancelled>(*queue.take_result(id3)));

    auto id4 = queue.submit({});
    queue.process_pending();
    assert(captured);
    auto late = captured->second;
    assert(queue.cancel(id4));
    assert(cancel_called);
    queue.process_pending();
    assert(std::holds_alternative<Cancelled>(*queue.take_result(id4)));
    late(Selected{std::filesystem::path("/tmp/late.txt")});
    queue.process_pending();
    assert(!queue.take_result(id4));
    assert(!queue.cancel(9999));

#ifndef __APPLE__
    Queue native(native_async_backend());
    auto nid = native.submit({});
    native.process_pending();
    assert(std::holds_alternative<Unsupported>(*native.take_result(nid)));
#endif
    return 0;
}
