#include "native_dialogs.h"

#import <Cocoa/Cocoa.h>

namespace afterhours::native_dialogs {
namespace {

NSString *to_ns(const std::string &value) {
    if (value.find('\0') != std::string::npos) return nil;
    return [NSString stringWithUTF8String:value.c_str()];
}

NSSavePanel *make_panel(const Request &request, std::string &error) {
    NSSavePanel *panel;
    if (request.kind == Kind::SaveFile) {
        panel = [NSSavePanel savePanel];
        [panel setCanCreateDirectories:YES];
    } else {
        NSOpenPanel *open = [NSOpenPanel openPanel];
        [open setCanChooseFiles:request.kind == Kind::OpenFile];
        [open setCanChooseDirectories:request.kind == Kind::Directory];
        [open setAllowsMultipleSelection:NO];
        panel = open;
    }
    NSString *title = to_ns(request.title);
    NSString *name = to_ns(request.default_name);
    NSString *directory = to_ns(request.directory.string());
    if (!title || !name || !directory) { error = "Dialog options must be valid UTF-8"; return nil; }
    if (!request.title.empty()) [panel setTitle:title];
    if (!request.default_name.empty()) [panel setNameFieldStringValue:name];
    if (!request.directory.empty())
        [panel setDirectoryURL:[NSURL fileURLWithPath:directory isDirectory:YES]];
    if (request.kind != Kind::Directory && !request.extensions.empty()) {
        NSMutableArray<NSString *> *types = [NSMutableArray array];
        for (auto extension : request.extensions) {
            if (!extension.empty() && extension.front() == '.') extension.erase(0, 1);
            if (extension.empty()) { error = "Empty dialog extension"; return nil; }
            NSString *type = to_ns(extension);
            if (!type) { error = "Dialog extensions must be valid UTF-8"; return nil; }
            [types addObject:type];
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [panel setAllowedFileTypes:types];
#pragma clang diagnostic pop
    }
    return panel;
}

Result result_from(NSSavePanel *panel, NSModalResponse response) {
    if (response == NSModalResponseCancel || response == NSModalResponseAbort)
        return Cancelled{};
    if (response != NSModalResponseOK) return Error{"Native dialog failed"};
    const char *path = [[[panel URL] path] UTF8String];
    if (!path || !*path) return Error{"Native dialog returned no path"};
    return Selected{std::filesystem::path(path)};
}

}  // namespace

Result show_native(const Request &request) {
    if (![NSThread isMainThread]) return Error{"Native dialogs require the main thread"};
    @autoreleasepool {
        @try {
            std::string error;
            NSSavePanel *panel = make_panel(request, error);
            if (!panel) return Error{error};
            return result_from(panel, [panel runModal]);
        } @catch (NSException *exception) {
            const char *reason = [[exception reason] UTF8String];
            return Error{reason ? reason : "Native dialog exception"};
        }
    }
}

CancelFn show_native_async(const Request &request, Completion completion) {
    auto done = [completion](Result r) { if (completion) completion(std::move(r)); };
    if (![NSThread isMainThread]) { done(Error{"Native dialogs require the main thread"}); return {}; }
    @autoreleasepool {
        @try {
            std::string error;
            NSSavePanel *panel = make_panel(request, error);
            if (!panel) { done(Error{error}); return {}; }
            // beginWithCompletionHandler returns immediately; panel stays up without runModal.
            [panel beginWithCompletionHandler:^(NSModalResponse response) {
              @try { done(result_from(panel, response)); } @catch (NSException *e) {
                  const char *r = [[e reason] UTF8String];
                  done(Error{r ? r : "Native dialog exception"});
              }
            }];
            return [panel]() { [panel cancel:nil]; };
        } @catch (NSException *exception) {
            const char *reason = [[exception reason] UTF8String];
            done(Error{reason ? reason : "Native dialog exception"});
            return {};
        }
    }
}

AsyncBackend native_async_backend() {
    return [](const Request &request, Completion completion) {
        return show_native_async(request, std::move(completion));
    };
}

}
