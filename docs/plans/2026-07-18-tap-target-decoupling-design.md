# Tap-Target Decoupling — Interactive Area vs Visual Bounds

Captured idea: let a component's hit/hover region be defined independently of
its drawn bounds, so a thin wire or a tiny icon can be easy to click without
changing how it looks.

**Date:** 2026-07-18
**Sources:** Figma community tip on decoupled tap targets (Tier 1); HCI
pointing-facilitation literature (Tier 2).

---

## Problem

The interactive area of a widget is currently welded to its visual area.
That fails at both extremes:

- **Too small to hit.** A wire is a 1–2px line. Right-clicking it to delete
  is fiddly. An icon button drawn at 24×24 is below the ~44–48px accessible
  touch target.
- **Too small even when the row is big.** A full-width list row with external
  margin has dead gutters — the visual has margin, so the hit region stops
  short of the container edge even though the whole row "reads" as clickable.

The consuming game (puzzle) papers over the first case per-system:

- `HasTransform::contains_tap(point, padding={10,10})` / `tap_target(padding)`
  in `src/components.h` — the *only* mechanism today. It grows `bounds()`
  outward by a **uniform, positive** padding. Symmetric, and can only make the
  target bigger, never smaller or offset.
- `DeleteWireSystem` / `wire_at()` in `src/systems.cpp` — wires have no
  transform, so hit testing is a hand-rolled point-to-line test with a
  zoom-scaled tolerance (`10.0f / camera.zoom`).
- `DeleteNodeSystem` in `src/systems.cpp` — uses `contains_tap(mouse)` plus a
  manual `wire_at()` precedence check so the thinner wire wins over the wide
  node behind it.

So hit geometry is re-derived ad hoc in every system that needs it, each with
its own tolerance convention. There is no single place that answers "what
region does this entity respond to?" — which is also why the debug view can't
draw hit regions today (`vendor/todo.md`: *"add tap target rendering to debug
view"*).

---

## Context

CSS solves this with negative margin/padding: a 24×24 icon gets `padding: 12px`
(or a `::before` overlay with negative insets) to reach 48×48 without moving
the glyph. A layout engine without box-model negative margins has to model the
hit region as its own rect anchored to the component.

This note has two tiers. **Tier 1 is near-term-practical** — a small
generalization of `contains_tap`. **Tier 2 is a speculative research log** —
dynamic, context-aware hit targets, captured so the keywords are searchable
later. Nothing here is committed to implementation.

---

## Tier 1 — Static tap-target decoupling (practical)

### The generalization

Replace "uniform positive padding" with an explicit **per-side inset rect that
may be negative or asymmetric**. The hit region becomes the visual bounds
adjusted per edge:

```cpp
// Alongside HasTransform. Insets are per-side; POSITIVE grows the hit
// region outward, NEGATIVE shrinks it inward. Asymmetric is fine.
struct HitInsets : BaseComponent {
    float l = 0, r = 0, t = 0, b = 0;

    // How each side is anchored when the visual bounds change (see below).
    enum class Anchor { Fixed, Proportional };
    Anchor anchor_l = Anchor::Fixed, anchor_r = Anchor::Fixed;
    Anchor anchor_t = Anchor::Fixed, anchor_b = Anchor::Fixed;
};
```

```
    +--------------------------+   <- hit_bounds() (t,l = +8 outward)
    |                          |
    |   +------------------+   |
    |   |  visual bounds   |   |   icon drawn at 24x24
    |   +------------------+   |
    |                          |
    +--------------------------+   <- b,r = +8 outward  => 40x40 hit
```

A negative inset does the opposite — useful when two visuals abut and you want
a dead gutter between their hit regions so neither steals the other's edge.

### One shared `hit_bounds()`

Every system that hit-tests should call one function instead of re-deriving
geometry:

```cpp
rect::Rect hit_bounds(const Entity &e) {
    rect::Rect b = e.get<HasTransform>().bounds();
    if (const auto *hi = e.get_if<HitInsets>())
        return { b.x - hi->l, b.y - hi->t,
                 b.width + hi->l + hi->r,
                 b.height + hi->t + hi->b };
    return b;                       // no insets => hit == visual
}
```

`contains_tap` collapses to `contains(hit_bounds(e), point)`. `DeleteNodeSystem`
uses it directly. `DeleteWireSystem` still needs its point-to-line test (a wire
is a polyline, not a rect), but the zoom-scaled tolerance becomes the wire's
`HitInsets` value expressed as a stroke half-width — the tolerance stops being a
magic constant in one system and becomes the wire's declared hit width. That
directly serves `vendor/todo.md`: *"make wire tap target larger for right
clicking."*

Because input and debug rendering both read the same `hit_bounds()`, the debug
view can draw exactly what input tests — no drift between "what I see
highlighted" and "what actually responds." That closes the second todo line.

### The anchor tension (the subtle part)

Fixed insets keep *padding* consistent but not *absolute position*. This bites
themed components. Suppose a hit target is anchored 8px inside a component whose
visual size is driven by a typography token — brand A renders the label at 14px,
brand B at 20px. The visual grows, and with fixed insets the hit region's edge
shifts relative to the container edge:

```
brand A (small type)          brand B (large type)
+----------------------+      +--------------------------+
| [==visual==]  8px .. |      | [====visual====]  8px .. |
+----------------------+      +--------------------------+
      ^ hit edge here               ^ hit edge moved right
```

Two anchor modes, neither universally correct:

- **Fixed** — inset is a constant distance from the visual edge. Padding is
  consistent across brands; absolute position drifts as the visual resizes.
- **Proportional** — inset is a fraction of the visual size. Position/proportion
  is stable; absolute padding becomes inconsistent (a 20% inset is a bigger gap
  on the larger visual).

Multiple people in the source discussion hit exactly this and couldn't pick a
single answer — because there isn't one. So the model shouldn't force one: let a
component choose **per side** (hence `Anchor` per edge in `HitInsets`). A
full-width row might want Fixed top/bottom (keep the 8px touch slop) but
Proportional left/right (stay glued to the container gutters as type scales).

### Scope for Tier 1

- Add `HitInsets` + `hit_bounds()`; route `contains_tap`, `DeleteNodeSystem`,
  and the debug renderer through it.
- Support negative and asymmetric insets. Per-side Fixed/Proportional anchor.
- Wire hit width becomes a declared inset, not a per-system constant.

Skipped: pointer kinematics, history, anything dynamic — that's Tier 2.

---

## Tier 2 — Dynamic / predictive hit targets (research log)

Speculative. The effective hit area could change with **interaction context**,
not just be a static rect. Established techniques, named so they're searchable:

- **Bubble Cursor** — the cursor carries a dynamic activation area that grows to
  encompass the single nearest target; the closest target activates without the
  pointer physically entering it. (Grossman & Balakrishnan, CHI 2005.)
- **Trajectory / direction-aware selection** — use pointer velocity and heading
  to bias which of several nearby targets is intended; expand the hitbox toward
  the direction of travel.
- **Adaptive / predictive target acquisition** — use interaction history
  (previous clicked element, common A→B transitions) and current UI state to
  temporarily enlarge the *likely-next* target's effective hitbox, visuals
  unchanged.
- **Two-Mode Target Selection** and probabilistic target expansion —
  visualizing/resolving ambiguity between several candidate targets.

### Illustrative scoring sketch (hypothetical)

```
effective_target =
    visual_bounds
  + pointer_position + pointer_velocity + pointer_direction
  + previous_clicked_element + interaction_history + current UI state

score(widget) = distance_score
              + trajectory_score
              + transition_score(prev_target, widget)
              + state_score(game_state, widget)
```

```cpp
struct InteractionContext {
    EntityID previous_target;
    vec2     pointer_position;
    vec2     pointer_velocity;
    std::vector<EntityID> recently_clicked;  // ring buffer
};
```

The likely flow: each frame, score every candidate under/near the cursor, pick
the max, and let the winner claim an enlarged effective hitbox for that frame.

**Honest caveat:** the individual pieces are established, but this exact
combination — interaction history + live pointer kinematics + per-frame
reshaped hitboxes, in an immediate-mode ECS UI — is not a single named
technique. Building it would be a novel system, so Tier 2 is a research idea,
not a plan.

### Searchable keywords

adaptive target acquisition · dynamic activation area · target-aware pointing ·
predictive pointing · intent-aware interfaces · trajectory-based target
prediction · probabilistic target expansion · context-aware hit testing ·
Markov model target prediction · pointing target prediction

---

## Status

Captured idea, no implementation committed. **Tier 1** (`HitInsets` +
shared `hit_bounds()`, negative/asymmetric insets, per-side Fixed/Proportional
anchoring) is actionable and directly serves both `vendor/todo.md` lines —
*"make wire tap target larger for right clicking"* and *"add tap target
rendering to debug view"* (one shared hit region means the debug view draws
what input tests). **Tier 2** (dynamic, predictive, context-aware hit targets)
is a research log only.
