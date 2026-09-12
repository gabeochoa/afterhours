# Native dialogs

Include `src/plugins/native_dialogs.h`. Compile `native_dialogs_macos.mm` and
link Cocoa on macOS; compile `native_dialogs.cpp` elsewhere. The public types
and calls are identical. Other platforms currently return `Unsupported`.

Own a `native_dialogs::Queue` on the main thread. Submit a `Request` with
`Kind::OpenFile`, `Kind::SaveFile` or `Kind::Directory`. Requests own their
title, initial directory, default filename and extension filters.

Call `process_pending()` from the application loop after ECS execution has
returned, never inside a system, query iteration or widget callback. Submission
does not open a dialog. Processing may enter the native modal event loop;
recursive processing is ignored, and results stay unavailable until processing
returns. Work submitted during processing waits for the next drain.

Retrieve a result once with `take_result(id)`. Its alternatives are `Selected`
with an owned path, `Cancelled`, `Unsupported`, and `Error` with a message.
Cancellation does not modify application state. Selected paths are not read or
written by the plugin. Keep the queue alive throughout processing.

For tests, include `src/plugins/e2e_testing/native_dialog_responses.h`, create
`testing::NativeDialogResponses`, push results, and construct the queue with
`std::ref(responses)`. The responses object must outlive the queue. Each request
consumes one response; exhaustion reports an error without opening an OS dialog.
Tests can run with this provider on any platform.
