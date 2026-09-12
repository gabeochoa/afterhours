#include "native_dialogs.h"

#import <Cocoa/Cocoa.h>

namespace afterhours::native_dialogs {

Result show_native(const Request &request) {
    if (![NSThread isMainThread]) return Error{"Native dialogs require the main thread"};
    @autoreleasepool {
        @try {
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
            auto string = [](const std::string &value) {
                if (value.find('\0') != std::string::npos) return (NSString *)nil;
                return [NSString stringWithUTF8String:value.c_str()];
            };
            NSString *title = string(request.title);
            NSString *name = string(request.default_name);
            NSString *directory = string(request.directory.string());
            if (!title || !name || !directory) return Error{"Dialog options must be valid UTF-8"};
            if (!request.title.empty()) [panel setTitle:title];
            if (!request.default_name.empty()) [panel setNameFieldStringValue:name];
            if (!request.directory.empty())
                [panel setDirectoryURL:[NSURL fileURLWithPath:directory isDirectory:YES]];
            if (request.kind != Kind::Directory && !request.extensions.empty()) {
                NSMutableArray<NSString *> *types = [NSMutableArray array];
                for (auto extension : request.extensions) {
                    if (!extension.empty() && extension.front() == '.') extension.erase(0, 1);
                    if (extension.empty()) return Error{"Empty dialog extension"};
                    NSString *type = string(extension);
                    if (!type) return Error{"Dialog extensions must be valid UTF-8"};
                    [types addObject:type];
                }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                [panel setAllowedFileTypes:types];
#pragma clang diagnostic pop
            }
            const auto response = [panel runModal];
            if (response == NSModalResponseCancel || response == NSModalResponseAbort)
                return Cancelled{};
            if (response != NSModalResponseOK) return Error{"Native dialog failed"};
            const char *path = [[[panel URL] path] UTF8String];
            if (!path || !*path) return Error{"Native dialog returned no path"};
            return Selected{std::filesystem::path(path)};
        } @catch (NSException *exception) {
            const char *reason = [[exception reason] UTF8String];
            return Error{reason ? reason : "Native dialog exception"};
        }
    }
}

}
