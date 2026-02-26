# UI Polish Patterns — Ideas from bazza/ui

**Date:** 2026-02-25
**Source:** https://github.com/bazzalabs/ui (shadcn/ui-style React component library)
**Scope:** What polish techniques can afterhours' C++ immediate-mode UI adopt

---

## Context

bazza/ui is a React component library built on Radix primitives. Its polish comes not from novel components but from *consistent micro-interactions* applied everywhere: coordinated enter/exit animations, directional slide-ins, focus glow rings, tooltips with arrows, and compositional widget APIs.

Afterhours already has many of the building blocks (animation system, focus tracking, theme tokens, opacity). The gaps are in how those tools are applied and in a few missing widget types.

---

## 1. Tooltips

### Priority: HIGH (critical for games)
### Effort: MEDIUM

### Gap

Afterhours has zero tooltip support. In games, tooltips are essential — hovering over items, abilities, stats, enemies, map markers. Currently you'd need to manually create an absolute-positioned div, track hover state, position it relative to the trigger, and manage its lifecycle.

### What bazza/ui Does

```
Tooltip
  TooltipTrigger  (wraps the hover target)
  TooltipContent  (positioned floating panel with arrow)
    - auto-positioned by side (top/bottom/left/right)
    - directional slide-in animation (slides from anchor direction)
    - fade + zoom enter/exit
    - arrow element pointing at trigger
```

### Proposed API for afterhours

```cpp
// Declare a tooltip on any element
auto item_slot = button(ctx, mk(parent), config);

if (tooltip(ctx, mk(item_slot.ent()),
            TooltipConfig{}
                .with_side(TooltipSide::Top)     // preferred side
                .with_delay(0.3f)                // hover delay before showing
                .with_arrow(true))) {
    // Content rendered inside the tooltip
    div(ctx, mk(tooltip_ent), label_config("Sword of Fire"));
    div(ctx, mk(tooltip_ent), label_config("+10 Attack"));
}
```

### Implementation Sketch

A tooltip needs:
1. **Hover timer** — track how long the trigger has been hot. Show after delay.
2. **Positioning** — compute position relative to trigger rect + preferred side. Flip if it would go off-screen.
3. **Arrow rendering** — small triangle drawn at the anchor point.
4. **Render layer** — tooltips render above everything (layer 2000+).
5. **Enter/exit animation** — fade+scale from 0.95 to 1.0 (use existing `AnimTrigger::OnAppear`).
6. **Input blocking** — tooltips should NOT block input to elements below them.

### Positioning Logic

```
side=Top:    tooltip.y = trigger.y - tooltip.height - arrow_size
side=Bottom: tooltip.y = trigger.y + trigger.height + arrow_size
side=Left:   tooltip.x = trigger.x - tooltip.width - arrow_size
side=Right:  tooltip.x = trigger.x + trigger.width + arrow_size

// Center on cross-axis
if side is Top or Bottom:
    tooltip.x = trigger.x + trigger.width/2 - tooltip.width/2

// Clamp to screen bounds, flip side if needed
```

### Arrow Rendering

Draw a small filled triangle at the tooltip edge pointing toward the trigger. This can be done with the existing `draw_triangle` primitive or approximated with a rotated square.

---

## 2. Enter/Exit Animations on Panels

### Priority: HIGH
### Effort: MEDIUM

### Gap

Afterhours has `AnimTrigger::OnAppear` for enter animations but **no exit animation support**. When a modal closes, it simply disappears (the `open` bool flips to false and `ShouldHide` is added). There's no fade-out, no zoom-out, no slide-out.

The same applies to dropdowns, scroll views, and any conditionally-shown UI.

### What bazza/ui Does

Every popup has coordinated enter AND exit animations:
- **Dialog:** fade-in overlay + zoom-in-95 content (enter), fade-out + zoom-out-95 (exit)
- **Sheet:** slide-in-from-right (enter), slide-out-to-right (exit), 300ms/500ms asymmetric timing
- **Dropdown:** fade-in + zoom-in-95 + directional slide (enter), reverse (exit)
- **Tooltip:** fade-in + zoom-in-95 + side-aware slide (enter), reverse (exit)

Key pattern: **exit animations are faster than enter animations** (300ms exit vs 500ms enter for sheets). This feels snappy.

### Proposed: AnimTrigger::OnDisappear

Add a new animation trigger that fires when an element is about to be removed/hidden:

```cpp
// In ComponentConfig
config.with_animation(AnimDef{}
    .trigger(AnimTrigger::OnAppear)
    .property(AnimProperty::Opacity)
    .from(0.0f).to(1.0f)
    .duration(0.2f)
    .easing(EasingType::EaseOutQuad));

config.with_animation(AnimDef{}
    .trigger(AnimTrigger::OnDisappear)
    .property(AnimProperty::Opacity)
    .from(1.0f).to(0.0f)
    .duration(0.15f)               // faster exit
    .easing(EasingType::EaseInQuad));
```

### Implementation Challenge

Exit animations are fundamentally harder in immediate-mode UI because the element stops being declared as soon as the condition is false. To animate an exit, you need to:

1. **Detect that an element is about to disappear** (was rendered last frame, not rendered this frame)
2. **Keep it alive** for the duration of the exit animation (don't remove it yet)
3. **Block input** to the dying element
4. **Remove it** when the exit animation completes

Possible approaches:
- **Ghost entity:** When an element stops being declared, create a "ghost" copy with the exit animation. The ghost auto-cleans up when the animation finishes.
- **Deferred hide:** Instead of immediately hiding, set a `pending_hide` flag and start the exit animation. The element only truly hides when the animation completes.
- **Frame-delayed removal:** Track `was_rendered_last_frame` per entity. If it wasn't rendered this frame but was last frame, keep it visible and play the exit animation.

### Recommendation

Start with the **deferred hide** approach for modals specifically (since they already have `open` state). Generalize later.

```cpp
// In modal_impl, when open transitions to false:
if (!is_open && was_open) {
    // Don't immediately hide — start exit animation
    m.exit_animation_remaining = 0.15f;
    // Entity stays visible but input-blocked
}

// Each frame while exit_animation_remaining > 0:
m.exit_animation_remaining -= dt;
if (m.exit_animation_remaining <= 0) {
    // NOW actually hide
}
```

---

## 3. Focus Ring Polish

### Priority: LOW (afterhours is already good here)
### Effort: SMALL

### Current State

Afterhours already has a solid focus ring implementation:
- Dual-color ring (contrast outline + main ring) for universal visibility
- Configurable thickness, roundness, segments
- Respects rounded corners of the focused element
- Theme-controlled via `focus_ring_offset` and `focus_ring_thickness`

### What bazza/ui Does Differently

- **Glow ring** — uses `ring-ring/50` (50% opacity of the ring color) with `ring-[3px]` (3px width). This creates a subtle glow rather than a hard outline.
- **Box-shadow transitions** — `transition-[color,box-shadow]` on every interactive element so the ring animates in smoothly rather than appearing instantly.

### What afterhours Could Add

1. **Animated focus ring appearance** — fade the focus ring opacity from 0 to 1 over ~100ms when an element gains focus. Currently it pops in instantly.
2. **Glow effect** — render an additional semi-transparent ring outside the main ring at ~30% opacity. This creates the "glow" look.
3. **Smooth focus transitions** — when focus moves between elements, animate the ring position from the old element to the new one (this is advanced and might not be worth it for games).

### Quick Win

Add a `focus_ring_glow` bool to Theme. When true, draw an extra ring at 2x thickness and 30% opacity behind the main focus ring:

```cpp
if (context.theme.focus_ring_glow) {
    Color glow_col = colors::opacity_pct(focus_col, 0.3f);
    float glow_thickness = thickness * 2.0f;
    RectangleType glow_rect = {
        focus_rect.x - glow_thickness,
        focus_rect.y - glow_thickness,
        focus_rect.width + glow_thickness * 2.0f,
        focus_rect.height + glow_thickness * 2.0f
    };
    draw_rectangle_rounded_lines(glow_rect, focus_roundness, focus_segments,
                                 glow_col, focus_corner_settings);
}
```

---

## 4. Sheet / Drawer Component

### Priority: MEDIUM
### Effort: MEDIUM

### Gap

Afterhours has centered modals but no edge-anchored sliding panels. In games, these are common for:
- Inventory panels sliding from the right
- Settings panels sliding from the left
- Notification trays sliding from the top
- Mobile-style bottom sheets for actions

### What bazza/ui Does

Sheet (Dialog-based) and Drawer (vaul-based) both:
- Slide from a configurable edge (top/right/bottom/left)
- Have a backdrop overlay
- Have a drag handle for mobile (drawer only)
- Animate in and out with directional slide

### Proposed API

```cpp
if (sheet(ctx, mk(parent),
          SheetConfig{}
              .with_side(SheetSide::Right)
              .with_width(h720(300))
              .with_title("Inventory")
              .with_backdrop(true),
          open)) {
    // Sheet content
    for (auto& item : inventory) {
        div(ctx, mk(sheet_ent), item_config(item));
    }
}
```

### Implementation

A sheet is simpler than a modal:
1. **Positioned at screen edge** — no centering math needed
2. **Slide animation** — use `with_translate()` to offset off-screen, animate to 0
3. **Backdrop** — same pattern as modal's backdrop
4. **Width/height** — one dimension is fixed (the sheet width/height), the other fills the screen edge

Most of this can be built on top of the existing modal infrastructure with different positioning and animation defaults.

---

## 5. Popover Positioning

### Priority: MEDIUM-HIGH
### Effort: MEDIUM

### Gap

Afterhours' dropdown positions its options directly below the trigger as children. There's no general-purpose "position this floating content relative to that trigger element" primitive.

This is needed for:
- Tooltips (item 1)
- Context menus
- Color pickers
- Date pickers
- Autocomplete dropdowns
- Info popovers

### What bazza/ui Does (via Radix)

- Content is rendered in a portal (outside the DOM tree)
- Positioned relative to the trigger with configurable side and alignment
- Collision detection flips side if content would overflow viewport
- Provides CSS variables for transform origin (animations originate from anchor point)

### Proposed: Floating Positioning Utility

Rather than building this into individual widgets, create a reusable positioning function:

```cpp
struct FloatingPosition {
    float x, y;
    Side actual_side;  // may differ from preferred if flipped
};

FloatingPosition compute_floating_position(
    RectangleType trigger_rect,
    RectangleType content_size,
    Side preferred_side,
    Align align,           // Start, Center, End
    float offset,          // gap between trigger and content
    RectangleType viewport // screen bounds for collision
);
```

This function can be used by tooltip, popover, dropdown, context menu, etc. The key logic:
1. Position content on the preferred side of the trigger
2. Align on the cross-axis (center, start, end)
3. Check if content overflows the viewport
4. If so, try the opposite side
5. Clamp to viewport edges as last resort

### Render Layer

Floating content needs to render above everything. Use the existing `render_layer` system with a high layer number (1500+ for popovers, 2000+ for tooltips).

---

## 6. Compositional Widget API

### Priority: LOW (afterhours' current approach works)
### Effort: LARGE (fundamental API change)

### Current State

Afterhours uses a flat `ComponentConfig` builder:
```cpp
button(ctx, mk(parent),
    ComponentConfig{}
        .with_label("Click me")
        .with_size(ComponentSize{h720(120), h720(44)})
        .with_background(Theme::Usage::Primary)
        .with_rounded_corners({1,1,1,1})
        .with_padding(Spacing::md));
```

### What bazza/ui Does

Complex widgets are composed from named sub-components:
```tsx
<Dialog>
  <DialogTrigger>Open</DialogTrigger>
  <DialogContent>
    <DialogHeader>
      <DialogTitle>Are you sure?</DialogTitle>
      <DialogDescription>This can't be undone.</DialogDescription>
    </DialogHeader>
    <DialogFooter>
      <Button>Confirm</Button>
    </DialogFooter>
  </DialogContent>
</Dialog>
```

### Assessment

The compositional approach is elegant in React where components are functions that return trees. In afterhours' immediate-mode C++ UI, the equivalent would be something like:

```cpp
if (auto dlg = dialog_begin(ctx, mk(parent), open)) {
    dialog_header(ctx, dlg, "Are you sure?", "This can't be undone.");
    // ... content ...
    if (auto footer = dialog_footer(ctx, dlg)) {
        if (button(ctx, mk(footer.ent()), confirm_config)) { ... }
    }
    dialog_end(ctx, dlg);
}
```

Afterhours already does something like this with modals (`modal_impl` creates header, title, close button internally). The pattern could be made more explicit but the value vs cost doesn't justify a large API change.

### Recommendation

Don't refactor the existing API. Instead, when adding new complex widgets (tooltip, sheet, popover), design them with explicit begin/end or auto-scoping patterns from the start.

---

## 7. Consistent Hover/Press Transitions

### Priority: MEDIUM
### Effort: SMALL

### Gap

Afterhours changes button color instantly on hover (`is_hot` → swap to hover color). bazza/ui uses `transition-[color,box-shadow]` so hover states animate smoothly over ~150ms.

### Current State

The color swap in `RenderImm::render_me`:
```cpp
if (context.is_hot(entity.id) && !entity.get<HasColor>().skip_hover_override) {
    col = entity.get<HasColor>().hover_color.value_or(
        context.theme.from_usage(Theme::Usage::Background));
}
```

This is an instant swap — no transition.

### Proposed Fix

Use the existing animation system to lerp between base color and hover color:

```cpp
// In the default button config, add:
config.with_animation(AnimDef{}
    .trigger(AnimTrigger::OnHover)
    .property(AnimProperty::Opacity)   // or a new AnimProperty::ColorBlend
    .from(0.0f).to(1.0f)
    .duration(0.12f)
    .easing(EasingType::EaseOutQuad));
```

Alternatively, add a `hover_transition_duration` to Theme. In the renderer, lerp between `base_color` and `hover_color` based on a per-entity hover timer:

```cpp
float t = entity.get<HoverState>().blend;  // 0..1, animated
Color col = colors::lerp(base_color, hover_color, t);
```

This is a small change with high visual impact. Every interactive element would feel smoother.

---

## 8. Disabled State Polish

### Priority: LOW
### Effort: SMALL

### What bazza/ui Does

Every interactive element has:
- `disabled:pointer-events-none` (no hover/click)
- `disabled:opacity-50` (visual dimming)
- `disabled:cursor-not-allowed` (cursor feedback)

### Current State in afterhours

Disabled state is partially handled via `HasLabel::is_disabled` which darkens font color. There's no consistent opacity reduction or cursor change for disabled elements.

### Quick Win

Add a `disabled` flag to `ComponentConfig` that:
1. Sets `HasLabel::is_disabled = true`
2. Sets opacity to 0.5
3. Skips click/drag listeners
4. Sets cursor to default (not pointer)
5. Removes from tab order

Most of this infrastructure exists — it just needs to be wired together consistently.

---

## 9. Summary: What to Do and When

### Phase 1 — Quick Wins (can do now)

| Item | Value | Effort |
|------|-------|--------|
| Hover transitions (color lerp) | HIGH | SMALL |
| Focus ring glow option | LOW | SMALL |
| Consistent disabled state | LOW | SMALL |

### Phase 2 — New Components

| Item | Value | Effort |
|------|-------|--------|
| Floating position utility | HIGH | MEDIUM |
| Tooltip component (uses floating) | HIGH | MEDIUM |
| Popover component (uses floating) | MEDIUM | MEDIUM |

### Phase 3 — Animation Polish

| Item | Value | Effort |
|------|-------|--------|
| Exit animations (deferred hide) | HIGH | MEDIUM |
| Sheet/drawer component | MEDIUM | MEDIUM |
| Modal enter/exit animations | MEDIUM | SMALL (once exit infra exists) |

### Phase 4 — If Needed

| Item | Value | Effort |
|------|-------|--------|
| Compositional widget API | LOW | LARGE |

---

## 10. Key Takeaway

The biggest bang-for-buck items are:

1. **Floating position utility** — unlocks tooltips, popovers, context menus, and better dropdowns. One function, many consumers.
2. **Hover transition** — instant color swaps feel jarring. A 120ms lerp makes everything feel polished. Small code change, huge perceptual impact.
3. **Exit animations** — the hardest one architecturally but the most noticeable. Every panel, modal, dropdown, and tooltip should animate out, not just vanish. Start with modals since they already have explicit open/close state.
