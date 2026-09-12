# File watching

Include `src/plugins/file_watcher.h` and compile `file_watcher.cpp`. Link
CoreServices on macOS. Other platforms expose the same API and return
`Unsupported` from `start`.

Own a `Watcher`, call `start` with existing roots, and check the `Started`,
`Unsupported` or `Error` result. Starting again stops the previous watch first.
Use `drain` on the app thread to receive owned paths and rescan hints. Events
are notifications to inspect current state, not a complete filesystem journal.
Native coalescing can combine changes. Rename may report old and new paths.

The macOS backend uses FSEvents on a private serial dispatch queue. It never
calls application code from that queue. The event buffer is bounded; overflow
produces one event with an empty path and `must_rescan = true`, meaning rescan
all watched roots. Native dropped-event and root-change flags also request
rescans. Choose the limit and latency through `Options`.

Call start, stop and destruction from the owning app thread. `stop` waits for
in-flight native callbacks and clears pending events; destruction calls stop.
No callbacks can retain the watcher after stop. Filtering, debounce, Git state
and refresh policy remain application-owned.
