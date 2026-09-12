#include <afterhours/src/plugins/native_dialogs.h>
#import <Cocoa/Cocoa.h>
#include <cassert>
#include <thread>

int main() {
    using namespace afterhours::native_dialogs;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        std::thread worker([] {
            assert(std::holds_alternative<Error>(show_native({})));
        });
        worker.join();
        for (auto kind : {Kind::OpenFile, Kind::SaveFile, Kind::Directory}) {
            Queue queue;
            auto id = queue.submit({.kind = kind, .title = "Afterhours dialog test",
                                    .default_name = "é space.txt", .extensions = {".txt"}});
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC),
                           dispatch_get_main_queue(), ^{ [NSApp abortModal]; });
            queue.process_pending();
            auto result = queue.take_result(id);
            assert(result && std::holds_alternative<Cancelled>(*result));
        }
    }
}
