# Changelog

For the person bumping the submodule. Twenty projects vendor this, so anything
that moves pixels or changes an API says what you will see and what to do.
Started partway through the project's life, so it does not go all the way back.

## Unreleased

### Breaking

**`HasScrollView::viewport_size` is now `std::optional`.** A plain `{0,0}` could not be told apart from a view genuinely measured as empty, so a consumer windowing its content read zero on frame one and silently built everything — which, before widget retirement, was a permanent plateau.
*What to do:* every read site is a compile error. `viewport_or_zero()` restores the old reading where you do not care; check `has_value()` where "not measured yet" is a real case, which it is for anything that windows.

**Labels lost their free 5px inset.** `Theme::text_inset` replaces a hardcoded 5 and defaults to **0**: edge-aligned text shifts, wrapped text re-breaks, auto-fit grows. Centred text does not move, so most screens are untouched — 97 of wm's 99 baselines were unaffected.
*Old pixels back:* build `-DAFTERHOURS_DEFAULT_TEXT_INSET=5.f`, verified pixel-identical. Per widget: `with_text_inset()`. Assigning a whole theme replaces it, so a screen swapping themes must re-state it.
*Also fixed by consolidating it:* the inset scales with `ui_scale` now (it was device pixels, so labels crept leading-ward on zoom), and `Dim::Text` charges it, so a box sized to its own text finally fits it.

**`Margin` field order now matches `Padding`** (`{top, left, bottom, right}`). Designated initializers and `Margin::Left(...)` are unaffected; a positional `Margin{a,b,c,d}` needs its middle two swapped and **will not error**, so grep for it.

### Visual — re-baseline

- **A child overflowing the cross axis is aligned, not dumped at the start.** `AlignItems::Center`/`FlexEnd` used to silently fall back to `FlexStart`; centring now overflows both edges evenly, per CSS. Moved 10 of 98 screens in wm.
- **Scroll-view children are laid out by autolayout** instead of repositioned afterwards by two drifted copies that double-counted margins. Margined direct children move by one margin, into the right place, and keep the `justify`/`align`/`gap` the old pass discarded.
- **Cross-axis `expand()` stretches** instead of collapsing to 0, so separators and fills that were invisible now have a size.
- **Scroll views draw a draggable scrollbar** on the overflowing edge of each enabled axis. `show_scrollbar = false` opts out; `scrollbar_thickness`/`scrollbar_min_thumb` are h720 Sizes so the bar tracks the window.
- **Menu shortcuts sit one pad in from the panel edge** — they were flush, clipping the last glyph in half.

### New APIs
- **`capture::enable()`** records what the app drew, on any backend, into one buffer (`capture.h`). Only the `none` backend could do this before, which is why an app-level test asserting on rendering meant profiling pixel columns out of a PNG. Off by default; a shipping build pays one branch per draw. The `none` backend's `draw_calls()` is now an alias for the same buffer.

- **`with_corner_radius(px)`** beside `with_roundness(fraction)`, which is a fraction of the *short side* and so means ~8px on a row and 180px on a panel. On `ComponentConfig`, `Theme::Builder`, `HasRoundedCorners`; precedence caller px > caller fraction > theme px > theme fraction, mutually exclusive at theme level. Default unchanged.
  `with_roundness(r > 1.f)` now warns once and names the fix — **floatinghotel hits this immediately** at `diff_renderer.h:614`, `main_content_system.h:815`, `sidebar_system.h:700`, `:706`, `:781`.
- **`imm::mk_keyed(parent, slot, key, &key_changed)`** recycles the slot's entity and says when the slot moved to a different item. A virtualized list can key rows on the slot, so entities stay bounded, without a press that began on row 291 finishing on row 294.
- **`HasScrollView::anchor_scroll`** holds the viewport on whatever child is at the top of it when content is added above, so a "load older" no longer yanks the view to the top. Off by default, since it costs a scan of the children and a list that only grows at the bottom does not need it.
- **`measure_config(cfg, available_w, available_h)`** (`ui/measure_config.h`) sizes a config without minting an entity, returning the box, its margins and a `pitch()` — so windowing a variable-height list stops meaning "restate the box model in app code". Answers Pixels, ScreenPercent, Percent and Text; Children and Expand need siblings and come back 0.
- **Public text measuring** — `ui::measure_text_wrapped` / `ui::wrap_text` (`ui/text_measure.h`) take a font name and size instead of a callable: `ui::measure_text_wrapped(body, width, font, 16.f).height`.
- **`animation::set_instant(bool)`** lands every track on its final value on the first update, still firing `on_complete`. Plus `animation::clear_all()`, and e2e `disable_animations`/`enable_animations`.
- **`expect_text_i`** — `expect_text` ignoring case.
- **Trackpad pinch (macOS)** — `input::get_pinch_delta()` (+0.01 = grow 1%) and `input::is_pinching()`; consume as `zoom *= (1.f + delta)`. Opt in with `-fblocks -framework AppKit -DAFTER_HOURS_ENABLE_MACOS_GESTURES` and one `gestures::install_pinch_monitor()` after the window exists; without it everything reads 0 silently.
  Uses a local `NSEvent` monitor via the ObjC runtime C API, so the library stays header-only and one implementation serves both backends. e2e: `pinch <delta>`.
- **Right-click** — `UIContext::is_right_click(id)` answers "a secondary click finished over this element or inside it". The target needs a click or drag listener (that is what hit-testing resolves against); asking about a plain `div` warns. e2e: `right_click x y`.
- **Synchronized scroll views** — `HasScrollView::sync_group`, same non-zero id on two or more views and scrolling one moves the rest. Only their enabled axes are written and each still clamps to its own content. Purely additive.

### Fixes
- **`virtual_list` culls for real.** Rows are recycled by slot so a 10k list holds ~30 entities instead of one per row ever scrolled past, the un-built rows count toward the scroll range so the bar spans the whole list, the window covers where the view is easing to so a fling does not flash blank, and the first frame sizes itself from the container instead of guessing 60. A press whose row scrolls out from under it is cancelled rather than firing on the row that replaced it. No API change.

- **Clicking a placeholder field and typing aborted the process.** Click-to-position measured against `HasLabel`, which holds the *placeholder* when the field is empty, so the first keystroke inserted past the end of an empty string — `std::out_of_range`, `SIGABRT`. `with_placeholder` is safe to adopt now.
- **A scroll view no longer squashes its content to fit.** `solve_violations` never exempted scroll views, where overflowing is the point, so a list longer than its viewport had every row shrunk toward zero. Latent until `48f808d` made the solver recurse into absolute subtrees. If you pinned strictness to 1 to stop this, you can stop.
- **A horizontally-scrolling column no longer over-reports content width.** `MeasureScrollViews` read the scroll flags rather than `flex_direction`, so a `vstack` scrolled on X — every table — summed its rows' widths instead of taking the max, and scrolled N times too far. Remove any fudge factor on `content_size.x`.
- **Headless runs degrade instead of collapsing** (no effect if your headless mode still makes a GL context): resolution falls back to `window_manager::headless_resolution` instead of (0,0), `measure_text` estimates from font size instead of returning {0,0}, and new `afterhours::shutdown()` deletes entities before the backend — leaving both to static destruction threw `std::bad_variant_access`. Each warns once.
- **`SINGLETON_FWD` still does not work in class scope** — use `SINGLETON_CLASS_FWD`, which has existed since `e9c662b`. Comment-only change.

### e2e

- **Assertions that passed without testing anything.** Injected `scroll_wheel` did nothing (wiped by `reset_frame` before the reader ran; now survives one reset, not drained on read, matching raylib). Injected pinch was delivered twice (now drained on read). `expect_no_text` always passed (missing from the parse chain, and treated a not-yet-rendered registry as absence). The visible-text registry ignored clip rects, so scrolled-away text still counted.
- **`expect_no_text` waits for a render**, by `VisibleTextRegistry::generation()` rather than frames — an app running several `tick()`s per render could burn the whole wait without one. Failures also name the label that matched, which is often a longer string nobody had in mind.
- **Failures are charged to the script that caused them.** A script's own `reset_test_state` doubled as the batch loader's separator and so ended the script early; the separator is now the unspellable `__end_of_script`. And a script's last command was finalized before it ran, so a failing final assertion exited 0 in single-script mode.
- **`dump_ui` is registered.** It was fully implemented and never registered, so scripts got "unknown command" — whose message blamed the consumer's registration order for a command they never had the chance to register. `register_ui_commands(sm, dump_fn)` takes the sink; without one the XML is logged.
- **A timeout names what it was about.** `Command 'assert_ui' ('no_such_widget') timed out` instead of four identical lines for four assertions, and the `expect_text` family prints the visible-text registry untruncated, one label per line, instead of the same 200 chars for every failure on a screen.
- **Injected right-clicks release.** `reset_frame` gated press-expiry on the left button, so an injected secondary press stuck down for the rest of the run and broke unrelated scripts files later.
- **`is_mouse_button_down(1)` returns injector state in test mode** instead of always `false`.

*Re-run your suite and read the new failures before silencing them.* Two of ours were real: a script that resized to 1080p and never resized back, and an assertion on a row that had never been on screen.

### Two silent fallbacks now warn

Same behaviour, they just say so; expect a burst on first run. `with_font_weight` needs a `"<font>@bold"` variant and falls back to the base font without one (this is why it gets filed as "no font weight support"), and `TextOverflow::Wrap` does nothing unless a font size is pinned.

### Already works, despite the gap docs

Don't rebuild: styled spans wrap (hanabi #22), `\n` is a hard break (#24), ellipsis + `expand()` doesn't hang, `with_styled_label` gives multi-colour text, `with_font_tier` is deprecated for `with_font_size(FontSize::Small)`.

### Docs

`FlexWrap`'s comment claimed `Wrap` was the default. It is not — `UIComponent` and `ComponentConfig` both start at `NoWrap`, as do 3860 of 3863 nodes in a downstream sweep.
