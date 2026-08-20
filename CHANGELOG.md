# Changelog

Written for the person bumping the submodule. Twenty projects vendor this, so
anything that moves pixels or changes an API is listed here with **what you
will see** and **what to do about it** — a bump should never be a mystery.

Started partway through the project's life, so it does not go all the way back.

## Unreleased

### A scroll view no longer squashes its content to fit

`solve_violations` shrinks children that overflow the main axis. It never
exempted scroll views, where overflowing is the entire point — so a list longer
than its viewport had every row shrunk toward zero instead of scrolling.

This was latent until `48f808d`, which made `solve_violations` recurse into
absolutely-positioned subtrees. An app whose panels are absolute (floatinghotel
builds all of its chrome that way) had never run the solver on them at all;
after that commit it did, and a 55-row commit list collapsed into a 4px-tall
smear. Strict `pixels()` children were unaffected, which is why it did not
show up in wm.

- *You will see:* rows inside a scroll view keep the height you asked for.
- *What to do:* nothing. If you compensated by pinning strictness to 1 to stop
  the squashing, you can stop.
- Only the axis the view actually scrolls is exempt, and only shrinking —
  under-filled content still expands as before.

### `expect_no_text` waits for a render, by generation not by frames

It concluded from whatever the registry held when the handler ran, which is the
*previous* render. An app that runs several `tick()`s per rendered frame — one
draining `wait_frames` in a loop, say — could burn the whole wait without a
single render, so the assertion tested the frame the preceding click was made
on. It now waits for `VisibleTextRegistry::generation()` to change, which only
`clear()` (once per render pass) does.

- Failures also name the label that matched now. Matching is a substring test,
  so the hit is often a longer string nobody had in mind.
- *What to do:* nothing. If a script only passed because the assertion read a
  stale frame, it will now fail — read it before silencing it.
- Still worth knowing: a wait that renders nothing gives assertions nothing to
  read. If your loop batches ticks, render between them.

### Visual changes — expect screenshot baselines to move

**A child bigger than its parent's cross axis is now aligned instead of
being dumped at the start.** `align_items: Center` (and `FlexEnd`) used to be
skipped entirely when a child overflowed, silently falling back to `FlexStart`.
Centring now overflows both edges evenly, matching CSS.

- *You will see:* any row whose content is taller than the row, or column
  wider than the column, shifts by half its overflow. In wm_afterhours this
  moved 10 of 98 screens.
- *What to do:* re-baseline. If a screen looked deliberately top-aligned, it
  was relying on the bug — set `AlignItems::FlexStart` to say so.

**Scroll-view children are laid out by autolayout, not repositioned
afterwards.** The plugin used to un-clamp overflowing children from outside,
in two drifted copies, and double-counted their margins on the way.

- *You will see:* a margined direct child of a scroll view moves by one
  margin, into the right place. Children also keep the `justify`/`align`/`gap`
  the old pass discarded.
- *What to do:* re-baseline. Nothing to change in your code.

**Cross-axis `expand()` stretches instead of collapsing to 0.** It used to
take a share of the leftover space, which on the cross axis is whatever the
widest sibling left — usually nothing.

- *You will see:* separators and fills that were invisible now have a size.
- *What to do:* nothing, unless you were using a collapsed `expand()` as a
  way to hide something.

**Scroll views draw a scrollbar.** `HasScrollView` had no indicator at all —
you could scroll 10k rows with no idea where you were.

- *You will see:* a capsule track and thumb on the overflowing edge of every
  scroll view, on whichever axis is enabled. Six screens changed in wm, all
  under 0.6%.
- *What to do:* nothing, or `show_scrollbar = false` on the component for a
  view that supplies its own. `scrollbar_thickness` and `scrollbar_min_thumb`
  are h720 Sizes, so the bar tracks the window instead of being 720p-shaped.
- Not draggable yet — it reports position, it does not set it.

**Menu shortcuts sit one pad in from the panel edge.** They were flush, so
the last glyph was clipped in half.

- *You will see:* `Cmd+C` renders fully; the shortcut column shifts left
  slightly.

### New APIs

**`with_corner_radius(px)` next to `with_roundness(fraction)`.** `roundness`
is a fraction of each widget's *short side*, so one value is ~8px on a row and
180px on a full-height panel. That is rarely what anyone means.

- Available on `ComponentConfig`, `Theme::Builder` and `HasRoundedCorners`.
- Precedence: **caller px > caller roundness > theme px > theme roundness.**
  At theme level the two are mutually exclusive — setting one clears the other.
- `with_roundness(r > 1.f)` now warns once per value and names the
  replacement. **floatinghotel will see this immediately**: five sites pass
  `4.0f` / `2.0f` into the fraction and currently render as full pills
  (`src/ui/diff_renderer.h:614`, `src/ecs/main_content_system.h:815`,
  `src/ecs/sidebar_system.h:700`, `:706`, `:781`).
- The default is unchanged. `Theme::corner_radius` stays unset and `roundness`
  stays `0.5`, so nothing moves until you opt in — and code that assigns
  `theme.roundness` directly (wordproc zeroes it for square corners) keeps
  working.

**Public text measuring** — `ui::measure_text_wrapped` / `ui::wrap_text`
(`ui/text_measure.h`). Takes a font name and size instead of the callable the
`ui::detail::` versions wanted:

```cpp
panel_height = ui::measure_text_wrapped(body, width, font, 16.f).height;
```

**`animation::set_instant(bool)`** — every track lands on its final value on
the first update. The animation still runs and `on_complete` still fires, so
nothing downstream branches on it. For e2e (settled screenshots), reduce-motion,
and skipping the wait during development. `animation::clear_all()` drops tracks
a screen change left behind. e2e: `disable_animations` / `enable_animations`.

**`expect_text_i`** — `expect_text` ignoring case, for text styled to a
different case than the source string.

**Trackpad pinch (macOS).** `input::get_pinch_delta()` returns the
magnification since the last frame (+0.01 = grow 1%), `input::is_pinching()`
the gesture state. Consume as `zoom *= (1.f + delta)`.

Opt-in: build with `-fblocks -framework AppKit
-DAFTER_HOURS_ENABLE_MACOS_GESTURES` and call
`gestures::install_pinch_monitor()` once after the window exists. Without it
everything reads 0, which is correct on a machine with no trackpad — there is
no per-frame `@notimplemented` log.

Uses a local `NSEvent` monitor via the Objective-C runtime C API, so the
library stays header-only and no consumer is forced into Objective-C++. Being
app-wide, it needs no window handle and serves both the raylib and sokol/Metal
backends from one implementation.

e2e: `pinch <delta>` command, plus `input_injector::set_pinch/consume_pinch`.
An injected pinch survives one extra `reset_frame`, because app code may build
its frame before the command system runs and a test should not have to know the
system order. It is drained on read, so it is delivered exactly once.

**Right-click.** `MousePointerState` tracks the secondary button, and
`UIContext::is_right_click(id)` answers "a secondary click finished over this
element or something inside it":

```cpp
if (ctx.is_right_click(row.ent().id)) { at = ctx.mouse.pos; open = true; }
```

- The target must carry a click or drag listener, because that is what
  hit-testing resolves against. Asking about a plain `div` warns rather than
  silently never firing.
- e2e gains `right_click x y` alongside `click`.

### Fixes that affect e2e

**Injected right-clicks release.** `reset_frame` gated its press-expiry on the
left button, so an injected secondary press stuck down for the rest of the run
and broke unrelated scripts several files later. Both branches now accept
either button, and `just_released` stays the left button's flag so a
right-only press cannot fake a left click.

**`is_mouse_button_down(1)` returns injector state in test mode** instead of
always `false`. If a test relied on button 1 reading false, it will now see
whatever the injector holds.

### Two silent fallbacks now warn

Same behaviour, they just say so. Expect a burst on first run — pre-existing,
not new breakage.

- `with_font_weight` needs a variant registered as `"<font>@bold"`; without one
  it falls back to the base font. This is why it gets filed as "no font weight
  support".
- `TextOverflow::Wrap` does nothing unless a font size is also pinned.

### e2e assertions that were passing without testing anything

Four bugs that all failed green. If you have e2e coverage, expect some of it to
start failing — that is the point.

- **Injected `scroll_wheel` did nothing.** The reader can run before the
  command that sets it, and the value was wiped by `reset_frame()` in between.
  The wheel now survives one reset. It is *not* drained on read, matching
  raylib's `GetMouseWheelMove()`.
- **Injected pinch was delivered twice.** Now drained on read, matching
  `gestures::consume_pinch_delta()`. Fixes the double-apply reported downstream.
- **`expect_no_text` always passed.** It was missing from `runner.h`'s parse
  chain, so its argument arrived as `"quoted` fragments, and it treated an
  empty (not-yet-rendered) registry as proof of absence.
- **The visible-text registry ignored clip rects.** Text scrolled out of a pane
  still counted as visible, so `expect_no_text` could not express "scrolled
  away" and `expect_text` gave false positives for clipped content. It now
  intersects with the same clip rect the render scissor uses.

*What to do:* re-run your suite and read the new failures before silencing
them. Two of ours were real: a script that resized to 1080p and never resized
back, and an assertion on a row that had never been on screen.

### A horizontally-scrolling column no longer over-reports its content width

`MeasureScrollViews` decided row-vs-column from the *scroll flags*
(`horizontal_enabled && !vertical_enabled`) rather than the container's actual
`flex_direction`. A `vstack` scrolled on X — which is every table — took the
row branch and summed its rows' widths instead of taking the max.

- *You will see:* a table with N rows reported N times its real content width,
  so it scrolled roughly N times too far past its end. Now correct.
- *What to do:* nothing. If you compensated with a fudge factor on
  `content_size.x`, remove it.

### Synchronized scroll views

`HasScrollView::sync_group` — give two or more views the same non-zero id and
scrolling any one of them moves the rest. For side-by-side diffs, a frozen
header over a table, that sort of thing.

```cpp
auto left  = vstack(ctx, mk(parent, 0), cfg.with_overflow(Overflow::Scroll, Axis::Y));
left.ent().get<HasScrollView>().sync_group = 1;
// ...same on the right pane
```

Only the axes a member has enabled get written, so a header row with
`vertical_enabled = false` tracks x without picking up the group's y. Each view
still clamps to its own content, so a shorter pane stops at its own end.

- *You will see:* nothing, unless you set `sync_group`. Purely additive.

### Headless runs degrade instead of collapsing

For apps that run with no window at all. If your headless mode still creates a
GL context (wm_afterhours' does) none of this changes anything.

- **Resolution.** `GetRenderWidth()` returns 0 without a window, which made
  `fetch_current_resolution()` report (0,0) and every layout collapse. It now
  falls back to `window_manager::headless_resolution` (1280x720, assignable)
  and warns once. Same guard on the sokol path, which also divided by a zero
  DPI.
- **Fonts.** `GetFontDefault()` has no atlas without a window, so
  `MeasureTextEx` returned {0,0} and every text-sized widget did too.
  `measure_text` now estimates from the font size and warns once.
  `afterhours::font_is_usable(font)` is public if you want to check yourself.
- **Shutdown.** New `afterhours::shutdown()` (`src/shutdown.h`) deletes
  entities and then the backend, in that order. Leaving both to static
  destruction let the graphics backend die first and entity destructors that
  still called into it threw `std::bad_variant_access`. Call it at the end of
  `main()`; idempotent.
- `SINGLETON_FWD` still does not work in class scope — use
  `SINGLETON_CLASS_FWD`, which has existed since `e9c662b`. Only a comment
  changed here.

### e2e failures are now charged to the script that caused them

Two bugs, both of which made a red suite point at an innocent file.

**A script's own `reset_test_state` ended the script.** That command was also
the batch loader's internal separator, so a script using it legitimately
inserted a phantom boundary and every result after it was attributed to the
following script. The separator is now `__end_of_script`, which is not
spellable in a script; `reset_test_state` just resets input state.

**The last command of a script was finalized in the tick it was dispatched**,
before it had run — so in single-script mode a failing final assertion exited
0. Single-script mode also never consulted the handler error count at all.

- *You will see:* failures land on the right script. If you have been ignoring
  a flaky script, the blame may move to a different (real) one.
- *What to do:* nothing. If you named a command `__end_of_script`, rename it —
  the loader now rejects it.

### `Margin` field order now matches `Padding`

`Margin` declared `{top, bottom, left, right}` while `Padding` declared
`{top, left, bottom, right}`, so a positional `{a, b, c, d}` meant different
things in each with nothing to catch it. `Margin` now uses `Padding`'s order.

- *You will see:* nothing, if you use designated initializers
  (`Margin{.left = pixels(10)}`) or the `Margin::Left(...)` factories — those
  are unchanged, and no positional use exists in any project checked.
- *What to do:* if you wrote `Margin{a, b, c, d}` positionally, swap the middle
  two. This will not produce a compile error, so grep for it.

The factories are `static constexpr` now, with static_asserts pinning each one
to its side so the order cannot drift again.

### Already works, despite the gap docs

Don't rebuild: styled spans wrap (hanabi #22), `\n` is a hard break (#24),
ellipsis + `expand()` doesn't hang, `with_styled_label` gives multi-colour
text, `with_font_tier` is deprecated for `with_font_size(FontSize::Small)`.

### Docs

`FlexWrap`'s comment claimed `Wrap` was the default. It is not — `UIComponent`
and `ComponentConfig` both start at `NoWrap`. No behaviour change; the comment
was simply wrong, and 3860 of 3863 nodes in a downstream sweep are `NoWrap` as
a result.
