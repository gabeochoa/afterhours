# Optional plugin APIs

All paths below are relative to afterhours. Include only the APIs you use.

## Terminal

Include `src/plugins/terminal/terminal.h`. Own a `terminal::Console` and register synchronous callbacks:

```cpp
terminal::Console console;
console.add_command({"greet", "Say hello", [](terminal::Arguments args) {
  if (args.size() != 1) return terminal::Result{"Usage: greet <name>", false};
  return terminal::Result{"Hello, " + args[0]};
}, {"world", "player"}});
console.execute("greet world");
```

Or register a struct with `console.add_command(std::make_unique<MyCommand>())`. Inherit `terminal::CommandBase` and override `name()` and `help()` returning `std::string_view`, plus `run(Arguments)` returning `Result`. Optionally override `completions()`. The console owns the command; removal releases it after any active call returns. Metadata is copied at registration. WM's `TerminalDemo::CountCommand` demonstrates this alongside callback commands.

`args.get<int>(0)` and `args.get<double>(0)` return an expected value or `ArgumentError::Missing` / `Invalid`. `args.get<int>(0, 1)` defaults only when missing. Numeric conversion consumes the entire argument, rejects overflow and nonfinite floats, and follows `std::from_chars` syntax: decimal integers, dot-decimal floats, no leading `+` or whitespace. Use `if (!value)` to check errors, then `*value`. Raw indexing and iteration still provide strings.

`help` and `clear` are built in. Registration returns false for duplicate/reserved names or missing callbacks. Arguments support single/double quotes, empty strings and escaped quotes/backslashes; other backslashes remain literal. Argument views last only through the callback. Capture app state with a lifetime at least as long as the registered command; use `remove_command` when retiring it. Output and history default to 200 and 100 entries, configurable in the constructor.

Set `console.execution = terminal::Execution::Queued` to defer panel submissions. `enqueue(line)` also queues directly, returning false for blank input or a printed parse error. Input is echoed and saved to history on submission; owned arguments wait for `drain()`, which runs the current batch in order and prints results. Commands are looked up at drain time, so removed commands report an error. `help` and `clear` follow the same queue order. Recursive drains do nothing; commands enqueued by callbacks wait for the next drain. `pending_count()` reports waiting commands. `execute()` always runs immediately.

Use the owning thread and keep the console alive throughout a drain. Choose a drain point safe for your callbacks: WM drains before constructing its terminal UI because its commands only change local screen state; world-changing commands should drain after ECS execution returns. No background thread is created.

Set `Command::usage` to syntax such as `save [path]`; callbacks still validate arguments. Optional `Command::unavailable_reason` returns `std::optional<std::string>`: no value allows execution; a reason blocks it. Structs override `usage()` and `unavailable_reason() const`. The panel and `help <command>` show both. Unavailable suggestions remain selectable; Enter/Run report the reason. Empty reasons display "Command unavailable".

Keep availability checks cheap and read-only: visible hints refresh each frame, and `execute()` / each queued command in `drain()` recheck before running. Custom UIs can call `command_usage(name)` and `command_unavailable_reason(name)`. WM demonstrates this with `reset`, available after `count` changes the counter.

Call `terminal::panel(ctx, mk(parent), console, config)` during UI construction. Suggestions appear above the input while typing. Up/Down select; Tab, Enter, or clicking accepts without executing and immediately shows choices for the next argument. Set `console.enter_accepts_first_suggestion = false` so Enter submits the typed command unless Up/Down explicitly selected a suggestion. Editing or accepting a completion resets that selection. Tab and clicking still accept suggestions. The default is `true`; Enter runs when no suggestions are open. Escape dismisses suggestions first, then blurs. Up/Down recall history when the list is closed; Shift+Tab navigates. Command names, `help` targets, and registered argument choices complete, including unfinished quotes. Supply an ordinary `ComponentConfig` for dimensions, font and background. Use the optional overlay below for focus and input-layer management. For use without a renderer, include only `src/plugins/terminal/console.h`.

For live choices, set `Command::complete` or override `CommandBase::complete(const CompletionRequest&) const`. Return candidate argument values; the console filters by prefix, sorts, deduplicates, and quotes only empty values or values containing whitespace, quotes, or backslashes. Static `completions` remain a first-argument shortcut; a callback takes precedence.

`CompletionRequest` provides the zero-based `argument_index`, decoded `prefix`, original `line`, and parsed `arguments` excluding the command name. Arguments include the partial value when present; after a separating space, the new argument is absent. Views last only through the callback. Completion currently targets the final argument. Providers run when input changes, the command registry changes, or completion is explicitly requested. Call `console.invalidate_completions()` when app data changes to refresh an unchanged input; nothing is polled every frame. WM's `palette add amber` demonstrates live choices, and `palette remove <color>` demonstrates a later argument depending on earlier input.

Pass an optional fifth `AutocompleteStyle` argument to style one terminal. Its `list`, `row`, `selected_row`, and `description` fields accept ordinary `ComponentConfig` overrides. Selection overrides apply after row overrides. Defaults use flat rows with command/help columns and the current theme; no global theme changes are needed.

For a toggleable terminal, construct `terminal::Overlay controls(mapping, terminal_layer)` using your `ProvidesLayeredInputMapping<Layer>` and register `layered_input<Layer>` plus the modal plugin. The terminal layer should contain UI/text-editing actions and your toggle action, with gameplay actions omitted. Call `controls.toggle()` from your app-chosen action, then `terminal::overlay(ctx, mk(parent), console, controls)` every frame, including while closed. `open()`, `close()`, and `is_open()` also support buttons or code. `OverlayStyle` exposes `window` (modal configuration), `panel`, and `autocomplete` styling. A panel debug name prefixes its child names, allowing multiple terminals to be targeted separately.

Opening focuses the input and blocks background UI. Escape dismisses suggestions first, then closes and restores focus. Closing restores the previous input layer unless the app has already switched to another layer. Layer transitions clear collected actions; handle toggling before gameplay consumers run. Raw keyboard polling bypasses input layers and remains the app's responsibility. The mapping must outlive the controls; destruction restores its layer. WM uses F2 and an Open overlay button; Space increments a jump counter outside the overlay and only types spaces inside it.

## Command picker

Include `src/plugins/command_picker/command_picker.h`. Own a `command_picker::Picker` with entries `{command_line, label, category, shortcut_label}` and call `command_picker::panel(ctx, mk(parent), console, picker, config, style)`. Entries reference commands in an existing `terminal::Console`; command lines can include preset arguments. No terminal panel is required. Shortcut labels are display metadata; applications bind their own shortcuts.

Search matches command lines, labels and categories using case-insensitive ASCII subsequences. Up/Down select, Enter or clicking executes, and Escape clears search. The virtual list reveals keyboard selection. Unavailable commands remain discoverable and show their reason. Execution respects `console.execution`; queued results remain in `console.output()` after the app calls `drain()`. The panel returns true when execution succeeds or is queued. It preserves the terminal input draft.

`picker.query`, `set_entries()`, and `result` support app control. `Style` provides search, row, selected-row and detail overrides. For custom rendering, include only `picker.h` and use `refresh()`, `count()`, `entry()`, `select()` / `move()` and `activate(console)`. WM's `command_picker` screen demonstrates counter, color, grid and zoom commands.

## Timing charts

Include `src/plugins/charts.h` for backend-independent point bounds and nearest-sample lookup. Include `src/plugins/ui/line_chart.h` for `ui::imm::line_chart`.

Pass owned `ChartSeries` values (name, points, color), optional unit and selected sample index, and an ordinary `ComponentConfig`. The widget retains its data through the draw callback. Mouse hover selects the nearest X sample in each series. Applications can expose keyboard sample controls through `selected_index`.

The plot shares axes across series, displays bounds and a legend, handles empty and constant series, and leaves gaps at nonfinite samples. X values should be ordered for connected lines. Use short series names for the compact legend. The widget uses theme text/grid colors and explicit series colors. The minimum drawing area is 120 by 90 pixels.

`wm --screen=chart_lab` demonstrates empty, single, constant, negative, multiple and live series, plus keyboard-accessible sample controls. The first release covers timing line plots; bars, histograms, pie charts and broader chart interactions remain future work.

`LineChartOptions::label_font_size` sets the legend, axis and hover-label text size. It defaults to 12px. The plot reserves more space for larger labels.

## File watching

Include `src/plugins/file_watcher.h` and compile `file_watcher.cpp`. Link CoreServices on macOS. Other platforms expose the same API and return `Unsupported` from `start`.

Own a `Watcher`, call `start` with existing roots, and check the `Started`, `Unsupported` or `Error` result. Starting again stops the previous watch first. Use `drain` on the app thread to receive owned paths and rescan hints. Events are notifications to inspect current state, not a complete filesystem journal. Native coalescing can combine changes. Rename may report old and new paths.

The macOS backend uses FSEvents on a private serial dispatch queue. It never calls application code from that queue. The event buffer is bounded; overflow produces one event with an empty path and `must_rescan = true`, meaning rescan all watched roots. Native dropped-event and root-change flags also request rescans. Choose the limit and latency through `Options`.

Call start, stop and destruction from the owning app thread. `stop` waits for in-flight native callbacks and clears pending events; destruction calls stop. No callbacks can retain the watcher after stop. Filtering, debounce, Git state and refresh policy remain application-owned.

## Input binding persistence

Include `src/plugins/input_binding_codec.h`. `encode` and `decode` handle one binding; `encode_bindings` and `decode_bindings` handle an ordered binding list. Decoding returns `std::nullopt` for malformed data, unsupported versions/tags, unknown key/button/axis codes, invalid modifiers or invalid directions.

The format uses versioned strings, suitable for a JSON array or another application-owned container. For example, `v1:key:65:3:1` records A with Shift+Ctrl and explicit modifiers. `v1:key:65:0:0` is a permissive A binding; `v1:key:65:0:1` requires no modifiers. All four current binding alternatives are supported. Numeric codes follow the shared afterhours key/gamepad codes.

Decoding builds a separate complete list. Apply it to the active mapping only after successful decoding, so invalid input does not partially replace live bindings. Action names, layers, filenames, file containers and migrations from old application-specific formats remain caller-owned. This header adds no JSON dependency and is independent of binding display.

`v1` identifies the library's binding wire format, independently of the app's settings schema version. It is not configurable: a decoder version must agree with the bytes it accepts. Apps may keep their own schema version around these strings or serialize the public `AnyInput` alternatives using a different codec. Using afterhours input does not require using this persistence format.

## Input prompts

Include `src/plugins/input_prompts.h`. Device activity belongs to the normal `InputCollector`: both the standard and layered collection systems update it while checking bindings, before alternatives are reduced to one action. Prompts read `collector.device_activity.preferred()`; they do not poll inputs or maintain a second input history. Activity uses the same deadzone-filtered axis samples as gameplay. Axis identity is the physical axis and controller, so two bindings for one action cannot hide a held axis from device tracking.

Mapped key/button presses, new stick excursions and mouse clicks select the device. Incidental mouse motion and a held stick do not repeatedly switch prompts. Keyboard or mouse clicks win simultaneous activity. Unmapped keyboard or controller inputs do not affect prompts.

Pass `prompt_for(mapping, action, collector, preference)` to read the current layer and bindings. `DevicePreference::pinned` optionally overrides presentation; reset it to resume automatic selection. It never disables device input. An unbound device returns `std::nullopt`. The result retains the typed binding for artwork or glyphs.

English text is the default. `format_binding(binding, key_name)` accepts an app-owned key-name lookup, for example to show Return instead of Enter. `prompt_for` accepts an optional `BindingFormatter` for the entire binding. Use this for localization so modifier names, ordering and separators can follow the locale. The library does not impose a translation catalog or global mutable label map. Default gamepad labels describe button positions without assuming a controller brand. Persistence remains in a separate optional header.

## Native dialogs

Include `src/plugins/native_dialogs.h`. Compile `native_dialogs_macos.mm` and link Cocoa on macOS; compile `native_dialogs.cpp` elsewhere. The public types and calls are identical. Other platforms currently return `Unsupported`.

Own a `native_dialogs::Queue` on the main thread. Submit a `Request` with `Kind::OpenFile`, `Kind::SaveFile` or `Kind::Directory`. Requests own their title, initial directory, default filename and extension filters.

Call `process_pending()` from the application loop after ECS execution has returned, never inside a system, query iteration or widget callback. Submission does not open a dialog. Processing may enter the native modal event loop; recursive processing is ignored, and results stay unavailable until processing returns. Work submitted during processing waits for the next drain.

Retrieve a result once with `take_result(id)`. Its alternatives are `Selected` with an owned path, `Cancelled`, `Unsupported`, and `Error` with a message. Cancellation does not modify application state. Selected paths are not read or written by the plugin. Keep the queue alive throughout processing.

For tests, include `src/plugins/e2e_testing/native_dialog_responses.h`, create `testing::NativeDialogResponses`, push results, and construct the queue with `std::ref(responses)`. The responses object must outlive the queue. Each request consumes one response; exhaustion reports an error without opening an OS dialog. Tests can run with this provider on any platform.

## Render capture formats

Use `capture_render_texture_rgba` for owned RGBA8 pixels and physical pixel dimensions. Rows run from top to bottom; each row contains width times four bytes in red, green, blue, alpha order, without padding. Values are the stored render-target channels. The operation does not change their alpha convention.

Use `capture_render_texture_png` for owned encoded PNG bytes. Both return `std::optional` and report failure with `std::nullopt`. The backend with no graphics support returns failure. Invalid or unloaded targets fail rather than returning an empty successful image.

The older `capture_render_texture_to_memory` now consistently returns encoded PNG on both raylib and Metal, or an empty buffer on failure. Metal callers that previously indexed its raw bytes must move to `capture_render_texture_rgba`. The library's Metal tests have been migrated. The reviewed external caller in endless-dance-chaos supplies PNG screenshots to MCP and retains that behavior.

Backend tests capture the same nonsquare target with a translucent red top and opaque blue bottom. They verify dimensions, channel order, orientation, alpha, legacy PNG signatures, byte-for-byte PNG decode versus RGBA, and failed readback. No CPU image editing or mutable texture upload API is added here.

## E2E key chords

`key Cmd+A` holds the actual Super modifier (Command on macOS, Windows/Super elsewhere). `Cmd`, `Super`, `Win` and `Meta` are case-insensitive aliases; `Ctrl` remains Control and `Option` means Alt. Combine modifiers with `+`. Scripts that intended Control must use `Ctrl+`, not `Cmd+`.

The action key emits one delayed press; modifiers are held without press events. The handler releases the chord after two frames. Reset, skip and timeout clear injected input.

## E2E arguments

Quote arguments containing spaces: `assert_ui header "text=My Project" hidden=false` or `assert_ui header text="My Project"`. Inside double quotes, `\"` means a literal quote and `\\` means a backslash. Other backslash sequences stay literal; `""` is an empty argument. Unclosed quotes fail the script with its line number.

Text commands such as `type`, `expect_text` and the value in `expect_input_text` still accept the unquoted rest of the line verbatim. To use escapes there, quote the entire text. Custom handlers receive decoded arguments and should not tokenize them again.

## Image tint

Use `ComponentConfig{}.with_image_tint({255, 120, 60, 200})` with `image`, `sprite`, `image_button`, or `with_texture`. RGB multiplies the source pixels; tint alpha multiplies the widget's opacity and ancestor opacity. Omitting the option restores white tint on a reused widget. Backgrounds and labels keep their own colors.

## Text visibility assertions

`expect_text "Label"` accepts a label whose bounds overlap the window and all ancestor clips by at least one pixel. `expect_text_fully_visible "Label"` requires its entire label bounds inside that area. Both use substring matching and work with immediate and batched rendering, including composed styled labels. These are bounds checks, not tests for glyph occlusion by another widget.

Use `assert_ui row_name text="Label"` to check the label's value even when it is clipped or hidden. Custom draw code using `register_text` asserts visibility itself; use `register_text_in_clip` when it has clipping bounds.

## Font tiers and zoom

`with_font_size(FontSize::Medium)` and `with_font("Inter", FontSize::Medium)` use the theme's tier size. Adaptive mode multiplies it by `ui_scale`; Proportional mode scales it from the 720px reference height. Component mode overrides screen mode, which overrides the app default. Explicit `pixels(...)` and `h720(...)` sizes retain their units. Config copies preserve both interpretations until measurement or rendering.

## Minimum touch targets

Set `enforce_min_touch_target = true` in `UIStylingDefaults::get().get_validation_config_mut()`, with `mode = ValidationMode::Warn`. `min_touch_target_size` defaults to 44 UI pixels on each axis, measured after layout and zoom. `highlight_violations` enables outlines.

The check uses click/drag listener bounds after transforms, scrolling, ancestor clipping and window clipping. Hidden, disabled, pointer-ignored and fully clipped controls are excluded. Partially clipped controls use their remaining clickable area. Register validation updates after UI layout and the overlay after UI rendering, before ending the frame. Both collection modes are supported.

## Checkbox marks

`ui::imm::checkbox` draws a font-independent checkmark when checked and leaves the unchecked indicator empty. Marks use the existing text color, alignment and inset settings, scale with the control, and respect disabled styling and opacity. `with_checkbox_indicators("yes", "no")` supplies custom text; `("", "")` hides both indicators. The supplied boolean is authoritative on every call; pending clicks toggle that value. External changes update the mark without reporting user input. See WM’s `checkboxes` screen.

Uniform UI borders draw inside the component bounds at their configured width, with widths resolved for the component’s scaling mode. Rounded and mixed corners preserve the outer radius; oversized widths fill the available interior. Focus rings use separate outward outlines.

## Tab navigation

`with_skip_tabbing(true)` excludes a control from keyboard traversal without changing pointer input. Passing false or omitting it on a later frame restores traversal. A manually added `SkipWhenTabbing` tag remains independent and must be removed by its owner. Config state is available as `UIComponent::skip_when_tabbing`.
