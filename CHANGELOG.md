# Changelog

Written for the person bumping the submodule. Twenty projects vendor this, so
anything that moves pixels or changes an API is listed here with **what you
will see** and **what to do about it** — a bump should never be a mystery.

Started partway through the project's life, so it does not go all the way back.

## Unreleased

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
Note an injected pinch survives one extra `reset_frame` where the wheel does
not — app code may build its frame before the command system runs, and a test
should not have to know the system order.

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

### Already works, despite the gap docs

Don't rebuild: styled spans wrap (hanabi #22), `\n` is a hard break (#24),
ellipsis + `expand()` doesn't hang, `with_styled_label` gives multi-colour
text, `with_font_tier` is deprecated for `with_font_size(FontSize::Small)`.

### Docs

`FlexWrap`'s comment claimed `Wrap` was the default. It is not — `UIComponent`
and `ComponentConfig` both start at `NoWrap`. No behaviour change; the comment
was simply wrong, and 3860 of 3863 nodes in a downstream sweep are `NoWrap` as
a result.
