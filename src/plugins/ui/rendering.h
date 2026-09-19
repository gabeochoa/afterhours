#pragma once

#include <set>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../animation.h"
#include "text_selection.h"
#include "text_stroke.h"
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
#include "../e2e_testing/test_input.h"
#include "../e2e_testing/visible_text.h"
#endif
#include "../../memory/arena.h"
#include "../input_system.h"
#include "../texture_manager.h"
#include "animation_keys.h"
#include "components.h"
#include "context.h"
#include "fmt/format.h"
#include "render_primitives.h"
#include "systems.h"
#include "theme.h"

namespace afterhours {

namespace ui {

// Left-side bearing is now calculated per-string using
// get_first_glyph_bearing() in font_helper.h. No more hardcoded offset.

namespace detail {

static inline float compute_effective_opacity(const Entity &entity);

static inline bool is_hidden_for_render(const Entity &entity) {
  const Entity *current = &entity;
  for (int depth = 0; depth < 64; ++depth) {
    if (current->has<ShouldHide>()) return true;
    if (!current->has<UIComponent>()) return false;
    const auto &cmp = current->get<UIComponent>();
    if (cmp.should_hide) return true;
    if (cmp.parent < 0 || cmp.parent == current->id) return false;
    auto parent = UICollectionHolder::getEntityForID(cmp.parent);
    if (!parent) return false;
    current = &parent.asE();
  }
  return false;
}

template <typename Draw>
void draw_drag_preview(Entity &overlay, Draw draw) {
  auto source = UICollectionHolder::getEntityForID(overlay.get<HasDragPreview>().source);
  if (!source || !source.asE().has<UIComponent>()) return;
  auto &root = source.asE().get<UIComponent>();
  const auto from = root.rect();
  const auto to = overlay.get<UIComponent>().rect();
  const Vector2Type offset{to.x - from.x, to.y - from.y};
  const EntityID parent = root.parent;
  const bool had_opacity = source.asE().has<HasOpacity>();
  const float original_opacity = had_opacity ? source.asE().get<HasOpacity>().value : 1.f;
  const float opacity = compute_effective_opacity(source.asE());
  source.asE().addComponentIfMissing<HasOpacity>().value = opacity;
  root.parent = -1;
  const auto visit = [&](auto &&self, Entity &entity, bool first) -> void {
    auto &cmp = entity.get<UIComponent>();
    if (entity.has<ShouldHide>() || (!first && cmp.should_hide)) return;
    const auto position = cmp.computed_rel;
    const bool hidden = cmp.should_hide;
    cmp.should_hide = false;
    cmp.computed_rel[Axis::X] += offset.x;
    cmp.computed_rel[Axis::Y] += offset.y;
    draw(entity);
    for (auto id : cmp.children) {
      auto child = UICollectionHolder::getEntityForID(id);
      if (child && child.asE().has<UIComponent>()) self(self, child.asE(), false);
    }
    cmp.computed_rel = position;
    cmp.should_hide = hidden;
  };
  visit(visit, source.asE(), true);
  root.parent = parent;
  if (had_opacity) source.asE().get<HasOpacity>().value = original_opacity;
  else source.asE().removeComponent<HasOpacity>();
}

static inline float compute_effective_opacity(const Entity &entity) {
  float result = 1.0f;
  EntityID current_id = entity.id;
  int guard = 0;
  while (current_id >= 0 && guard < 64) {
    OptEntity opt_cur = UICollectionHolder::getEntityForID(current_id);
    if (!opt_cur.valid())
      break;
    const Entity &cur = opt_cur.asE();
    if (cur.has<HasOpacity>()) {
      result *= std::clamp(cur.get<HasOpacity>().value, 0.0f, 1.0f);
    }
    if (!cur.has<UIComponent>())
      break;
    EntityID pid = cur.get<UIComponent>().parent;
    if (pid < 0 || pid == current_id)
      break;
    current_id = pid;
    ++guard;
  }
  return std::clamp(result, 0.0f, 1.0f);
}

// Find the nearest ancestor with HasScrollView or HasClipChildren
// Returns invalid OptEntity if no clipping ancestor exists
static inline OptEntity find_clip_ancestor(const Entity &entity) {
  if (!entity.has<UIComponent>())
    return {};

  const UIComponent &cmp = entity.get<UIComponent>();
  EntityID pid = cmp.parent;

  int guard = 0;
  while (pid >= 0 && guard < 64) {
    OptEntity opt_parent = UICollectionHolder::getEntityForID(pid);
    if (!opt_parent.valid()) {
      break;
    }
    Entity &parent = opt_parent.asE();

    if (parent.has<HasScrollView>() || parent.has<HasClipChildren>()) {
      return opt_parent;
    }
    if (!parent.has<UIComponent>())
      break;
    pid = parent.get<UIComponent>().parent;
    ++guard;
  }
  return {};
}

// Legacy alias for backwards compatibility
static inline OptEntity find_scroll_view_ancestor(const Entity &entity) {
  return find_clip_ancestor(entity);
}

// compute_intersected_clip_rect moved to systems.h (detail namespace) so both
// the render scissor here and hit-testing in HandleClicks share one definition.

// Get scroll offset from ancestor scroll view, returns {0,0} if none
static inline Vector2Type get_scroll_offset(const Entity &entity) {
  OptEntity scroll_ancestor = find_scroll_view_ancestor(entity);
  if (scroll_ancestor.valid() && scroll_ancestor->has<HasScrollView>()) {
    return scroll_ancestor->get<HasScrollView>().scroll_offset;
  }
  return {0.0f, 0.0f};
}

// accumulated_scroll_offset lives in systems.h (included above) — one helper
// shared by render (here) and hit-test so they stay aligned.

// Get the scissor rect from a scroll view ancestor (viewport bounds)
static inline RectangleType get_scroll_scissor_rect(const Entity &entity) {
  OptEntity scroll_ancestor = find_scroll_view_ancestor(entity);
  if (scroll_ancestor.valid() && scroll_ancestor->has<UIComponent>()) {
    return scroll_ancestor->get<UIComponent>().rect();
  }
  return {0, 0, 0, 0};
}

// Content size for a scroll view: sum of its children, not their screen span.
static inline void update_scroll_view_content_size(Entity &entity) {
  if (!entity.has<HasScrollView>() || !entity.has<UIComponent>()) return;
  auto &scroll = entity.get<HasScrollView>();
  const auto &cmp = entity.get<UIComponent>();
  scroll.content_size = measure_scroll_content(cmp, scroll);
  scroll.clamp_scroll();
}

// Everything the focus ring needs, resolved in one place. Both renderers ask
// this and differ only in how they submit the rects.
struct FocusRing {
  RectangleType rect;      // the innermost ring rect; thickness grows outward
  Color color;             // Theme::Usage::Focus
  Color contrast;          // hairline edge drawn just outside AND just inside
  float thickness = 0.f;
  float roundness = 0.f;
  int segments = 8;
  std::bitset<4> corners;

  // Outward expansion `t` of the main ring.
  // TODO: downstream apps keep re-implementing this (puzzle has
  // rect::Rect::expand/pad). A rect expander belongs in the library so nobody
  // writes a fifth one.
  [[nodiscard]] RectangleType expanded(float t) const {
    return {rect.x - t, rect.y - t, rect.width + t * 2.f,
            rect.height + t * 2.f};
  }
  // The contrast edges: one ring outside the whole stack, one inside it. The
  // inner one is what makes the ring survive on a widget whose fill is the
  // same colour as the ring (a selected/active item).
  [[nodiscard]] RectangleType outer_contrast() const {
    return expanded(thickness);
  }
  [[nodiscard]] RectangleType inner_contrast() const { return expanded(-1.f); }

  // roundness is a FRACTION of the shorter side, so reusing it on an expanded
  // rect grows the radius with the rect and the outer rings bow away from the
  // inner ones -- the bracket marks hanging off each corner. Offsetting a
  // rounded rect outward by t keeps concentric corners only if the radius grows
  // by exactly t, so convert back to the fraction that yields r0 + t here.
  [[nodiscard]] float roundness_at(float t) const {
    const RectangleType r = expanded(t);
    const float shorter = std::min(r.width, r.height);
    if (shorter <= 0.f)
      return 0.f;
    const float r0 = roundness * 0.5f * std::min(rect.width, rect.height);
    return std::clamp(2.f * std::max(0.f, r0 + t) / shorter, 0.f, 1.f);
  }
};

template <typename InputAction>
static inline std::optional<FocusRing>
focus_ring_for(const UIContext<InputAction> &context, const Entity &entity,
               const UIComponent &cmp, Vector2Type scroll_offset) {
  if (context.visual_focus_id != entity.id)
    return {};

  // thickness 0 means "no ring". It has to be checked before anything is
  // emitted: the contrast edges are not gated on it, so an app that set 0 to
  // opt out still got lines painted into its widgets.
  FocusRing ring;
  ring.thickness = context.theme.focus_ring_thickness;
  if (ring.thickness <= 0.f)
    return {};

  ring.rect = cmp.focus_rect(context.theme.focus_ring_offset);
  ring.rect.x -= scroll_offset.x; // ride the scroll like draw_rect
  ring.rect.y -= scroll_offset.y;
  if (entity.has<HasUIModifiers>())
    ring.rect = entity.get<HasUIModifiers>().apply_modifier(ring.rect);

  // No HasRoundedCorners means square, the same reading the fill uses. Falling
  // back to the theme here instead drew a rounded ring inside a square border
  // on anything that called disable_rounded_corners().
  const bool custom = entity.has<HasRoundedCorners>();
  ring.corners = custom ? entity.get<HasRoundedCorners>().rounded_corners
                        : std::bitset<4>().reset();
  if (custom) {
    const auto &corners = entity.get<HasRoundedCorners>();
    const auto rect = cmp.rect();
    const float radius = resolve_roundness(corners.radius_px, corners.roundness, rect) *
                         std::min(rect.width, rect.height) * .5f;
    const float inset = std::min(context.theme.focus_ring_offset,
                                std::max(0.f, (std::min(rect.width, rect.height) - 1.f) * .5f));
    const float scale = entity.has<HasUIModifiers>() ? entity.get<HasUIModifiers>().scale : 1.f;
    const float shorter = std::min(ring.rect.width, ring.rect.height);
    ring.roundness = shorter > 0.f ? std::clamp(2.f * std::max(0.f, radius - inset) * scale / shorter, 0.f, 1.f) : 0.f;
  }
  ring.segments =
      custom ? entity.get<HasRoundedCorners>().segments : context.theme.segments;

  ring.color = context.theme.from_usage(Theme::Usage::Focus);
  const float lum = colors::luminance(ring.color);
  ring.contrast = lum > 0.5f ? Color{0, 0, 0, 180} : Color{255, 255, 255, 180};
  const float opacity = compute_effective_opacity(entity);
  if (opacity < 1.0f) {
    ring.color = colors::opacity_pct(ring.color, opacity);
    ring.contrast = colors::opacity_pct(ring.contrast, opacity);
  }
  return ring;
}

struct FocusPaint {
  FocusRing ring;
  EntityID entity_id;
  size_t after_command;
  int layer;
  float rotation;
  std::optional<RectangleType> clip;
};

template <typename InputAction>
std::optional<FocusPaint> prepare_focus_paint(const UIContext<InputAction> &context) {
  auto opt = UICollectionHolder::getEntityForID(context.visual_focus_id);
  if (!opt.valid() || !opt->template has<UIComponent>()) return {};
  const Entity &entity = opt.asE();
  const auto &cmp = entity.get<UIComponent>();
  if (is_hidden_for_render(entity)) return {};
  auto ring = focus_ring_for(context, entity, cmp, accumulated_scroll_offset(entity));
  if (!ring) return {};
  std::set<EntityID> descendants{entity.id};
  std::vector<EntityID> pending{entity.id};
  while (!pending.empty()) {
    const auto id = pending.back();
    pending.pop_back();
    auto child = UICollectionHolder::getEntityForID(id);
    if (!child.valid() || !child->template has<UIComponent>()) continue;
    for (auto next : child->template get<UIComponent>().children)
      if (descendants.insert(next).second) pending.push_back(next);
  }
  for (size_t i = context.render_cmds.size(); i > 0; --i) {
    const auto &cmd = context.render_cmds[i - 1];
    if (!descendants.contains(cmd.id)) continue;
    const auto [has_clip, clip] = compute_intersected_clip_rect(entity);
    return FocusPaint{*ring, entity.id, i - 1, cmd.layer,
                      entity.has<HasUIModifiers>() ? entity.get<HasUIModifiers>().rotation : 0.f,
                      has_clip && !entity.has<HasScrollView>() ? std::optional{clip} : std::nullopt};
  }
  return {};
}

inline void draw_focus_paint(const FocusPaint &paint) {
  const auto &ring = paint.ring;
  if (paint.clip) {
    const auto &r = *paint.clip;
    begin_scissor_mode(static_cast<int>(r.x), static_cast<int>(r.y),
                       static_cast<int>(r.width), static_cast<int>(r.height));
  }
  capture::Scope attribute(paint.entity_id, paint.layer);
  push_rotation(ring.rect.x + ring.rect.width * .5f,
                ring.rect.y + ring.rect.height * .5f, paint.rotation);
  draw_rectangle_rounded_lines(ring.outer_contrast(), ring.roundness_at(ring.thickness),
                               ring.segments, ring.contrast, ring.corners);
  if (ring.rect.width > 2.f && ring.rect.height > 2.f)
    draw_rectangle_rounded_lines(ring.inner_contrast(), ring.roundness_at(-1.f),
                                 ring.segments, ring.contrast, ring.corners);
  for (float t = 0; t < ring.thickness; t += 1.f)
    draw_rectangle_rounded_lines(ring.expanded(t), ring.roundness_at(t),
                                 ring.segments, ring.color, ring.corners);
  pop_rotation();
  if (paint.clip) end_scissor_mode();
}

inline void collect_focus_paint(RenderCommandBuffer &buffer, const FocusPaint &paint) {
  const auto &ring = paint.ring;
  if (paint.clip) {
    const auto &r = *paint.clip;
    buffer.add_scissor_start(static_cast<int>(r.x), static_cast<int>(r.y),
                              static_cast<int>(r.width), static_cast<int>(r.height),
                              paint.layer, paint.entity_id);
  }
  buffer.add_rounded_rectangle_outline(ring.outer_contrast(), ring.contrast,
      ring.roundness_at(ring.thickness), ring.segments, ring.corners,
      paint.layer, paint.entity_id, 0.f, paint.rotation);
  if (ring.rect.width > 2.f && ring.rect.height > 2.f)
    buffer.add_rounded_rectangle_outline(ring.inner_contrast(), ring.contrast,
        ring.roundness_at(-1.f), ring.segments, ring.corners,
        paint.layer, paint.entity_id, 0.f, paint.rotation);
  buffer.add_rounded_rectangle_outline(ring.rect, ring.color, ring.roundness,
      ring.segments, ring.corners, paint.layer, paint.entity_id, ring.thickness, paint.rotation);
  if (paint.clip) buffer.add_scissor_end(paint.layer, paint.entity_id);
}

} // namespace detail

// Auto-fit's floor, matching TypographyScale::MIN_ACCESSIBLE_SIZE_720P.
#ifndef AFTERHOURS_MIN_FONT_SIZE
#define AFTERHOURS_MIN_FONT_SIZE 16.0f
#endif
constexpr float MIN_FONT_SIZE = AFTERHOURS_MIN_FONT_SIZE;
// Font size threshold for debug visualization - text is likely unreadable
constexpr float DEBUG_FONT_SIZE_THRESHOLD = 8.0f;

// Convert CursorType to backend mouse cursor ID
// Values match across raylib and sokol backends
inline int to_cursor_id(CursorType cursor) {
  switch (cursor) {
  case CursorType::Default:
    return 0; // MOUSE_CURSOR_DEFAULT
  case CursorType::Pointer:
    return 4; // MOUSE_CURSOR_POINTING_HAND
  case CursorType::Text:
    return 2; // MOUSE_CURSOR_IBEAM
  case CursorType::ResizeH:
    return 5; // MOUSE_CURSOR_RESIZE_EW
  case CursorType::ResizeV:
    return 6; // MOUSE_CURSOR_RESIZE_NS
  }
  return 0;
}

// Enable visual debug indicators for text that can't fit in containers
// Define AFTERHOURS_DEBUG_TEXT_OVERFLOW to show red corner indicators
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
constexpr bool SHOW_TEXT_OVERFLOW_DEBUG = true;
#else
constexpr bool SHOW_TEXT_OVERFLOW_DEBUG = false;
#endif

// Result struct for position_text that includes whether text fits properly
// Where a label sits inside its box. Device pixels, so it does not scale.
inline constexpr float kTextInset = 5.f;

// kTextInset, or less on a box too small to spend it.
inline Vector2Type text_inset_for(const RectangleType &rect) {
  if (rect.width <= 0.0f || rect.height <= 0.0f)
    return Vector2Type{0.0f, 0.0f};
  return Vector2Type{std::min(kTextInset, rect.width * 0.4f),
                     std::min(kTextInset, rect.height * 0.4f)};
}

// Padding on a label-only element does nothing. Honouring it would move every
// existing label, so say so instead.
inline void warn_ignored_label_padding(const Entity &entity,
                                      const UIComponent &cmp) {
  if (cmp.padding_is_default || !cmp.children.empty())
    return;
  const float padd = cmp.computed_padd[Axis::X] + cmp.computed_padd[Axis::Y];
  if (padd <= 0.f)
    return;
  // Once per run: wm alone has 541 of these.
  static bool warned = false;
  static int suppressed = 0;
  if (warned) {
    suppressed++;
    return;
  }
  warned = true;
  const std::string name = entity.has<UIComponentDebug>()
                               ? entity.get<UIComponentDebug>().name()
                               : std::string();
  log_warn("'{}' sets padding on a label with no children. That does nothing "
           "Use text_inset for text spacing, margin for external spacing, or "
           "padding on a wrapper. Further cases are not repeated.",
           name.empty() ? "<unnamed>" : name);
  (void)suppressed;
}

struct TextPositionResult {
  RectangleType rect;
  bool text_fits; // false if font was clamped to minimum (text won't fit)
};

static inline TextPositionResult
position_text_ex(const ui::FontManager &fm, const std::string &text,
                 RectangleType container, TextAlignment alignment,
                 Vector2Type margin_px, float explicit_font_size = 0.f,
                 float extra_spacing = 0.f,
                 TextOverflow text_overflow = TextOverflow::Clip,
                 // False for callers that clip on purpose (a text field) or
                 // that are only measuring. The overflow warning means "your
                 // text is being cut off and you may not have noticed"; for
                 // those callers it is neither news nor actionable.
                 bool report_overflow = true) {
  // Early return for empty text - prevents infinite loop in font size
  // calculation
  if (text.empty()) {
    return TextPositionResult{
        .rect =
            RectangleType{
                .x = container.x + margin_px.x,
                .y = container.y + margin_px.y,
                .width = MIN_FONT_SIZE,
                .height = MIN_FONT_SIZE,
            },
        .text_fits = true,
    };
  }

  Font font = fm.get_active_font();
  float bearing = get_first_glyph_bearing(font, text.c_str());

  // Calculate the maximum text size based on the container size and margins
  Vector2Type max_text_size = Vector2Type{
      .x = container.width - 2 * margin_px.x,
      .y = container.height - 2 * margin_px.y,
  };

  // Check for invalid container (negative or zero usable space)
  if (max_text_size.x <= 0 || max_text_size.y <= 0) {
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
    // Only warn when overflow persists across multiple frames. Elements with
    // percent/absolute sizing may have zero dimensions on their first layout
    // frame before the autolayout pass resolves them.
    static std::unordered_map<std::string, int> overflow_frame_count;
    overflow_frame_count[text]++;
    if (overflow_frame_count[text] == 3) {
      log_warn("Container too small for text: container={}x{}, margins={}x{}, "
               "text='{}'",
               container.width, container.height, margin_px.x, margin_px.y,
               text.length() > 20 ? text.substr(0, 20) + "..." : text);
    }
#endif
    return TextPositionResult{
        .rect =
            RectangleType{
                .x = container.x + margin_px.x,
                .y = container.y + margin_px.y,
                .width = MIN_FONT_SIZE,
                .height = MIN_FONT_SIZE,
            },
        .text_fits = false,
    };
  }
  // TODO add some caching here?

  float font_size;
  bool text_fits;

  // "Does it fit" has to be asked of the layout the text will actually be
  // drawn as. A wrapping or hard-broken label measured as one long line is
  // always wider than its box, so every one of them reported as overflowing --
  // which is both wrong and loud, since the warning fires per frame per size
  // and a dragged box is a new size every frame.
  const bool measures_as_block = text_overflow == TextOverflow::Wrap ||
                                 text.find('\n') != std::string::npos;
  const auto measure_laid_out = [&](float size) {
    if (!measures_as_block)
      return measure_text(font, text.c_str(), size, 1.f + extra_spacing);
    const auto m = detail::measure_wrapped(
        text, max_text_size.x, [&](const std::string &s) {
          return measure_text(font, s.c_str(), size, 1.f + extra_spacing);
        });
    return Vector2Type{m.width, m.height};
  };

  if (explicit_font_size > 0.f) {
    // Exact-size mode: use the requested size directly (never downscale to fit).
    // Text may overflow; report that accurately via text_fits so the overflow
    // debug indicator + warning cover explicitly-sized text too (previously
    // hardcoded true, so an oversized explicit font silently clipped).
    // Report it, do not only resize it silently.
    if (const float warn_at =
            imm::ThemeDefaults::get().theme.min_font_size_warn_720p;
        warn_at > 0.f && explicit_font_size < warn_at) {
      warn_once(static_cast<int>(explicit_font_size * 100.f),
                "text asks for {}px, below this app's {}px readability floor",
                explicit_font_size, warn_at);
    }
    font_size = explicit_font_size;
    if (std::getenv("AH_TRACE_LBL") && !text.empty())
      log_warn("LBL '{}' fs={} box={}x{}", text.substr(0, 14), font_size,
               container.width, container.height);
    Vector2Type ts = measure_laid_out(font_size);
    // A block is centred in the FULL rect with no vertical margin (see the
    // multi-line branch of draw_text_in_rect), so charging it the y-margin
    // would report a box sized to exactly its own text -- a Dim::Text box --
    // as overflowing by precisely the margin, always.
    const float usable_y = measures_as_block ? container.height : max_text_size.y;
    text_fits = ts.y <= usable_y && ts.x <= max_text_size.x;
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
    if (!text_fits && report_overflow) {
      // Keyed on the LAYOUT, not the string. Keying on text meant an editable
      // field logged a fresh line on every keystroke -- and grew this set
      // without bound for the life of the process. The container and font are
      // the actionable part anyway; the string is only there to locate it.
      //
      // Quantised, because a container being *dragged* is a new exact size
      // every frame, which brings back both the per-frame spam and the
      // unbounded set. A resize crosses a 25px bucket rarely enough to stay
      // readable while still reporting each distinct size it settles at. The
      // cap is the backstop for anything that defeats the bucketing.
      constexpr int kBucket = 25;
      constexpr size_t kMaxLogged = 64;
      static std::unordered_set<std::string> logged_explicit;
      const std::string key =
          fmt::format("{}x{}@{}", (int)container.width / kBucket,
                      (int)container.height / kBucket, (int)font_size);
      if (logged_explicit.size() < kMaxLogged &&
          logged_explicit.insert(key).second) {
        // Name the API: with_wrap() is flex wrap and does nothing here, and
        // that is the first thing people reach for.
        log_warn("Text '{}' at explicit font {} overflows container {}x{} "
                 "(margins {}x{}) - it will be clipped, not downscaled. To "
                 "wrap it use with_text_overflow(TextOverflow::Wrap); "
                 "with_wrap() is flex wrap and will not help",
                 text.length() > 20 ? text.substr(0, 20) + "..." : text,
                 font_size, container.width, container.height, margin_px.x,
                 margin_px.y);
        if (logged_explicit.size() == kMaxLogged)
          log_warn("... further text-overflow warnings suppressed");
      }
    }
#endif
  } else {
    // Use binary search to find largest font size that fits
    float low = MIN_FONT_SIZE;
    float high = std::min(max_text_size.y, 200.f); // Cap at reasonable max
    font_size = low;

    // When text_overflow is Ellipsis, text will be truncated to fit width,
    // so only constrain font size by height.
    const bool width_constrained = (text_overflow != TextOverflow::Ellipsis);

    while (high - low > 0.5f) {
      float mid = (low + high) / 2.f;
      Vector2Type ts = measure_laid_out(mid);
      bool fits = ts.y <= max_text_size.y &&
                  (!width_constrained || ts.x <= max_text_size.x);
      if (fits) {
        font_size = mid;
        low = mid;
      } else {
        high = mid;
      }
    }

    // The search starts at the floor and only ever raises, so asking whether
    // font_size landed below it is always false. Measure instead.
    const Vector2Type fitted = measure_laid_out(font_size);
    text_fits = fitted.y <= max_text_size.y &&
                (!width_constrained || fitted.x <= max_text_size.x);
    if (!text_fits) {
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
      // Only log once per unique text to avoid spamming
      static std::unordered_set<std::string> logged_texts;
      if (logged_texts.find(text) == logged_texts.end()) {
        logged_texts.insert(text);
        log_warn("Text '{}' cannot fit in container {}x{} with margins {}x{} "
                 "even at the {} floor - it will be clipped",
                 text.length() > 20 ? text.substr(0, 20) + "..." : text,
                 container.width, container.height, margin_px.x, margin_px.y,
                 MIN_FONT_SIZE);
      }
#endif
    }
  }

  // Measure with final font size for accurate positioning
  Vector2Type text_size =
      measure_text(font, text.c_str(), font_size, 1.f + extra_spacing);

  // Calculate the text position based on the alignment and margins
  Vector2Type position;
  switch (alignment) {
  default:
    log_warn("Unknown alignment: {}", static_cast<int>(alignment));
    [[fallthrough]];
  case TextAlignment::None: // None defaults to Left alignment
  case TextAlignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x + bearing,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Center: {
    // Calculate centered position, but clamp to prevent starting before
    // container left edge
    float centered_offset =
        (container.width - 2 * margin_px.x - text_size.x) / 2;
    float text_x = container.x + margin_px.x + centered_offset;
    // Clamp so text never starts before container left edge
    text_x = std::max(container.x + margin_px.x, text_x);
    position = Vector2Type{
        .x = text_x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  }
  case TextAlignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x - text_size.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  }

  return TextPositionResult{
      .rect =
          RectangleType{
              .x = position.x,
              .y = position.y,
              .width = font_size,
              .height = font_size,
          },
      .text_fits = text_fits,
  };
}

// Backwards-compatible wrapper that returns just the rectangle
static inline RectangleType position_text(const ui::FontManager &fm,
                                          const std::string &text,
                                          RectangleType container,
                                          TextAlignment alignment,
                                          Vector2Type margin_px) {
  return position_text_ex(fm, text, container, alignment, margin_px).rect;
}

// Internal helper to draw text at a specific position (used by stroke, shadow,
// and main text) The 'sizing' rect contains any offset (shadow/stroke) that
// should be applied
namespace detail {

// TextRunLine, wrap_runs_to_width, wrap_text_to_width and measure_wrapped now
// live in text_selection.h, so selection geometry and autolayout can share
// them without pulling in a graphics backend.

static inline void
draw_text_at_position(const ui::FontManager &fm, const std::string &text,
                      RectangleType rect, TextAlignment alignment,
                      RectangleType sizing, Color color, float rotation = 0.0f,
                      float rot_center_x = 0.0f, float rot_center_y = 0.0f,
                      float extra_spacing = 0.0f) {
  // Always use UTF-8 aware rendering (works for all text including CJK)
  Font font = fm.get_active_font();
  float fontSize = sizing.height;
  float spacing = 1.0f + extra_spacing;

  // position_text_ex already computed alignment-aware position in sizing.x/y.
  // Shadow/stroke offsets are also pre-applied to sizing before this call.
  // Just use the pre-calculated position directly.
  Vector2Type startPos = {sizing.x, sizing.y};

  // Use provided rotation center (component center), or default to text rect
  // center
  float centerX = (rot_center_x != 0.0f || rot_center_y != 0.0f)
                      ? rot_center_x
                      : rect.x + rect.width / 2.0f;
  float centerY = (rot_center_x != 0.0f || rot_center_y != 0.0f)
                      ? rot_center_y
                      : rect.y + rect.height / 2.0f;
  draw_text_ex(font, text.c_str(), startPos, fontSize, spacing, color, rotation,
               centerX, centerY);
}

// Explicit override, else auto-contrast, else theme font; disabled in each.
// Shared by RenderImm and RenderBatched, which had identical copies.
// Disabled text darkens by 0.5 while disabled backgrounds use
// Theme::disabled_variant -- kept as-is, see todo.md D14b.
inline Color resolve_label_color(const HasLabel &hasLabel, const Theme &theme) {
  if (hasLabel.explicit_text_color.has_value()) {
    const Color c = hasLabel.explicit_text_color.value();
    return hasLabel.is_disabled ? colors::darken(c, 0.5f) : c;
  }
  if (hasLabel.background_hint.has_value()) {
    const Color c = colors::auto_text_color(hasLabel.background_hint.value(),
                                            theme.font, theme.darkfont);
    return hasLabel.is_disabled ? colors::darken(c, 0.5f) : c;
  }
  return theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled);
}
} // namespace detail

// Public wrap-aware text measurement. Returns the {width, height, line_count}
// of `text` laid out within `max_width` (honoring hard newlines), using the
// shared TextMeasureCache for per-line sizing. Lets apps size a container to a
// wrapping paragraph instead of hand-rolling a height estimate.
using WrappedTextMetrics = detail::WrappedTextMetrics;
[[nodiscard]] inline WrappedTextMetrics
measure_text_wrapped(TextMeasureCache &cache, std::string_view text,
                     std::string_view font_name, float font_size,
                     float max_width, float spacing = 1.0f) {
  return detail::measure_wrapped(
      std::string(text), max_width, [&](const std::string &s) {
        return cache.measure(s, font_name, font_size, spacing);
      });
}

// Space reserved between a label and its own box, per side. A widget's own
// override wins over the theme, and the result scales with ui_scale -- held as
// a device-pixel constant it slid every label toward its leading edge as the
// app zoomed in.
//
// Takes the theme rather than reaching for ThemeDefaults, because a screen is
// free to assign context.theme a whole preset and that is what the colour and
// focus-ring paths read. Resolving the inset from the other struct would let
// one label take its colour from one theme and its geometry from another.
static inline Vector2Type
resolve_text_inset(const Theme &theme,
                   const std::optional<Vector2Type> &override_ = std::nullopt) {
  const Vector2Type base = override_.value_or(theme.text_inset);
  return Vector2Type{base.x * theme.ui_scale, base.y * theme.ui_scale};
}

// For the free draw helpers, which are not handed a context. ThemeDefaults is
// what context.theme is seeded from each frame, so this agrees with the entity
// path for every screen that does not swap its theme.
static inline Vector2Type default_text_inset() {
  return resolve_text_inset(imm::ThemeDefaults::get().theme);
}

static inline void draw_text_in_rect(
    const ui::FontManager &fm, const std::string &text, RectangleType rect,
    TextAlignment alignment, Color color, bool show_debug_indicator = false,
    const std::optional<TextStroke> &stroke = std::nullopt,
    const std::optional<TextShadow> &shadow = std::nullopt,
    float rotation = 0.0f, float rot_center_x = 0.0f, float rot_center_y = 0.0f,
    TextOverflow text_overflow = TextOverflow::Clip,
    float letter_spacing = 0.0f,
    float explicit_font_size = 0.0f,
    // False for the per-line calls this makes on itself. Each of those gets a
    // rect exactly one line tall, which can never contain that line once the
    // 5px margins come off -- so they would all report overflow. Whether the
    // block fits is the parent call's question, and it has already answered it.
    bool report_overflow = true,
    Vector2Type inset = default_text_inset()) {
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
  // Register text for E2E testing assertions (only visible-in-viewport text)
  if (testing::test_input::detail::test_mode) {
    auto *pcr = EntityHelper::get_singleton_cmp<
        window_manager::ProvidesCurrentResolution>();
    float vw = pcr ? static_cast<float>(pcr->width()) : 1280.f;
    float vh = pcr ? static_cast<float>(pcr->height()) : 720.f;
    testing::VisibleTextRegistry::instance().register_text_if_visible(
        text, rect.x, rect.y, rect.width, rect.height, vw, vh);
  }
#endif

  // Multi-line: split into lines and draw each stacked vertically. Two ways in
  // -- a hard '\n', which always breaks, or TextOverflow::Wrap with an explicit
  // font size (soft wrapping is only well-defined at a known size). Each line
  // is drawn with Clip and contains no '\n', so this never re-enters.
  //
  // Hard breaks are handled here rather than left to the backend: raylib's
  // DrawTextEx honours '\n' but sokol and the recording backend do not, so
  // without this the same label renders differently per backend.
  const bool has_hard_break = text.find('\n') != std::string::npos;
  const bool soft_wraps =
      text_overflow == TextOverflow::Wrap && explicit_font_size > 0.f;
  if ((has_hard_break || soft_wraps) && !text.empty() && rect.width > 0.f) {
    Font font = fm.get_active_font();
    float spacing = 1.f + letter_spacing;
    // No pinned size: auto-fit as the single-line path would, then shrink so
    // every line fits rather than just the first.
    float font_size = explicit_font_size;
    if (font_size <= 0.f)
      font_size = position_text_ex(fm, text, rect, alignment,
                                   inset, explicit_font_size,
                                   letter_spacing, text_overflow)
                      .rect.height;
    // Only soft-wrap when asked; otherwise break on '\n' alone.
    float max_width = soft_wraps ? rect.width - 2.f * inset.x : 1e9f;

    auto line_width = [&](const std::string &s) {
      return measure_text(font, s.c_str(), font_size, spacing).x;
    };

    // Greedy word wrap. Words wider than max_width are placed on their own
    // line (not character-split) — callers that need hard breaks can size up.
    std::vector<std::string> lines =
        detail::wrap_text_to_width(text, max_width, line_width);

    if (lines.size() > 1) {
      float line_h = measure_text(font, "Ag", font_size, spacing).y;
      if (explicit_font_size <= 0.f && rect.height > 0.f) {
        const float want = line_h * static_cast<float>(lines.size());
        if (want > rect.height) {
          font_size *= rect.height / want;
          line_h = measure_text(font, "Ag", font_size, spacing).y;
        }
      }
      float total_h = line_h * static_cast<float>(lines.size());
      // Vertically center the block within the rect.
      float y = rect.y + std::max(0.f, (rect.height - total_h) * 0.5f);
      for (const auto &ln : lines) {
        RectangleType line_rect{rect.x, y, rect.width, line_h};
        draw_text_in_rect(fm, ln, line_rect, alignment, color,
                          show_debug_indicator, stroke, shadow, rotation,
                          rot_center_x, rot_center_y, TextOverflow::Clip,
                          letter_spacing, font_size, /*report_overflow=*/false,
                          inset);
        y += line_h;
      }
      return;
    }
    // Single line that fits: fall through to normal single-line rendering.
  }

  TextPositionResult result = [&]() {
    return position_text_ex(fm, text, rect, alignment, inset,
                            explicit_font_size, letter_spacing, text_overflow);
  }();

  // Draw visual debug indicator if text doesn't fit and debug is enabled
  // Shows a semi-transparent red overlay and border around the container
  if (show_debug_indicator && !result.text_fits) {
    Color overlay_color = Color{255, 50, 50, 60}; // Semi-transparent red fill
    Color border_color = Color{255, 50, 50, 200}; // Solid red border
    float border_thickness = 2.0f;

    // Draw semi-transparent red overlay on the entire container
    draw_rectangle(rect, overlay_color);

    // Draw red border lines around the container
    // Top
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y,
                                 .width = rect.width,
                                 .height = border_thickness},
                   border_color);
    // Bottom
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y + rect.height - border_thickness,
                                 .width = rect.width,
                                 .height = border_thickness},
                   border_color);
    // Left
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y,
                                 .width = border_thickness,
                                 .height = rect.height},
                   border_color);
    // Right
    draw_rectangle(RectangleType{.x = rect.x + rect.width - border_thickness,
                                 .y = rect.y,
                                 .width = border_thickness,
                                 .height = rect.height},
                   border_color);
  }

  // Effectively zero, as in nothing to rasterise. This used to test
  // MIN_FONT_SIZE, so any text below the floor was silently not drawn, and the
  // clamp above was the only thing keeping that from happening.
  if (result.rect.height < 1.f) {
    return;
  }

  // Handle text overflow ellipsis truncation
  std::string truncated_text;
  const std::string &render_text = [&]() -> const std::string & {
    if (text_overflow != TextOverflow::Ellipsis || text.empty()) {
      return text;
    }
    Font font = fm.get_active_font();
    float font_size = result.rect.height;
    float spacing = 1.f + letter_spacing;
    float max_width = rect.width - 2.f * inset.x;
    if (max_width <= 0.f)
      return text;

    Vector2Type text_size =
        measure_text(font, text.c_str(), font_size, spacing);
    if (text_size.x <= max_width) {
      return text;
    }

    // Text overflows — find longest prefix that fits with "..."
    const std::string ellipsis = "...";
    Vector2Type ellipsis_size =
        measure_text(font, ellipsis.c_str(), font_size, spacing);
    float available = max_width - ellipsis_size.x;
    if (available <= 0.f) {
      truncated_text = ellipsis;
      return truncated_text;
    }

    // Binary search for the longest prefix that fits
    size_t low = 0;
    size_t high = text.size();
    size_t best = 0;
    while (low <= high && high <= text.size()) {
      size_t mid = (low + high) / 2;
      std::string prefix = text.substr(0, mid);
      Vector2Type ps = measure_text(font, prefix.c_str(), font_size, spacing);
      if (ps.x <= available) {
        best = mid;
        low = mid + 1;
      } else {
        if (mid == 0)
          break;
        high = mid - 1;
      }
    }

    truncated_text = text.substr(0, best) + ellipsis;
    return truncated_text;
  }();

  RectangleType sizing = result.rect;

  // Draw text shadow first (behind everything)
  // Renders text at a single offset position to create a drop shadow effect
  if (shadow.has_value() && shadow->has_shadow()) {
    RectangleType shadow_sizing = sizing;
    shadow_sizing.x += shadow->offset_x;
    shadow_sizing.y += shadow->offset_y;
    detail::draw_text_at_position(fm, render_text, rect, alignment,
                                  shadow_sizing, shadow->color, rotation,
                                  rot_center_x, rot_center_y, letter_spacing);
  }

  if (stroke.has_value() && stroke->has_stroke()) {
    const float center_x = (rot_center_x != 0.f || rot_center_y != 0.f)
                               ? rot_center_x : rect.x + rect.width / 2.f;
    const float center_y = (rot_center_x != 0.f || rot_center_y != 0.f)
                               ? rot_center_y : rect.y + rect.height / 2.f;
    text_stroke::draw(fm.get_active_font(), render_text, {sizing.x, sizing.y},
                       sizing.height, 1.f + letter_spacing, stroke->thickness,
                       stroke->color, rotation, center_x, center_y);
  }

  // Draw main text on top
  detail::draw_text_at_position(fm, render_text, rect, alignment, sizing, color,
                                rotation, rot_center_x, rot_center_y,
                                letter_spacing);
}

// Draw coloured runs through the same wrap primitive as the plain path.
// `joined` is the concatenated text, only used to derive a font size when none
// was set explicitly.
// Non-const fm: per-run weights are drawn by swapping the active font around
// each run, since draw_text_in_rect resolves the face itself. The active font
// is restored before returning.
static inline void draw_runs_in_rect(
    ui::FontManager &fm, const std::vector<TextSpan> &runs,
    RectangleType rect, TextAlignment alignment,
    bool show_debug_indicator = false,
    const std::optional<TextStroke> &stroke = std::nullopt,
    const std::optional<TextShadow> &shadow = std::nullopt,
    float rotation = 0.0f, float rot_center_x = 0.0f, float rot_center_y = 0.0f,
    TextOverflow text_overflow = TextOverflow::Clip, float letter_spacing = 0.0f,
    float explicit_font_size = 0.0f, const std::string &joined = "",
    Vector2Type inset = default_text_inset()) {
  if (runs.empty() || rect.width <= 0.f)
    return;

  Font font = fm.get_active_font();
  float font_size = explicit_font_size;
  if (font_size <= 0.f) {
    font_size = position_text_ex(fm, joined, rect, alignment,
                                 inset, explicit_font_size,
                                 letter_spacing, text_overflow)
                    .rect.height;
  }
  const float spacing = 1.f + letter_spacing;

  // Per-run weight resolves against the ACTIVE font's family. resolve_weighted
  // returns the base name when that variant was never loaded, so an app with
  // no bold face renders regular rather than throwing in get_font.
  const std::string base_font = fm.active_font;
  const auto font_for = [&](colors::FontWeight w) {
    return fm.get_font(fm.resolve_weighted(base_font, w));
  };
  const auto weighted_width = [&](const std::string &s, colors::FontWeight w) {
    return measure_text(font_for(w), s.c_str(), font_size, spacing).x;
  };
  const auto line_width = [&](const std::string &s) {
    return measure_text(font, s.c_str(), font_size, spacing).x;
  };

  // Only wrap when asked and a size is known, matching draw_text_in_rect.
  const bool wants_wrap =
      text_overflow == TextOverflow::Wrap && explicit_font_size > 0.f;
  const float wrap_width = wants_wrap ? (rect.width - 2.f * inset.x) : 1e9f;
  if (wrap_width <= 0.f)
    return;

  // Memoised: this runs every frame, usually on text that hasn't changed.
  const std::uint64_t wrap_key = detail::wrap_memo::key_for(runs, wrap_width);
  const std::vector<detail::TextRunLine> *cached =
      detail::wrap_memo::lookup(wrap_key);
  const std::vector<detail::TextRunLine> &lines =
      cached ? *cached
             : detail::wrap_memo::store(
                   wrap_key,
                   detail::wrap_runs_to_width(runs, wrap_width,
                                              weighted_width));
  float line_h = measure_text(font, "Ag", font_size, spacing).y;
  // Auto-fit above sized the font against the joined text, i.e. for a single
  // line. Hard breaks still split it, so shrink to fit every line.
  if (explicit_font_size <= 0.f && lines.size() > 1 && rect.height > 0.f) {
    const float want = line_h * static_cast<float>(lines.size());
    if (want > rect.height) {
      font_size *= rect.height / want;
      line_h = measure_text(font, "Ag", font_size, spacing).y;
    }
  }
  const float total_h = line_h * static_cast<float>(lines.size());
  float y = rect.y + std::max(0.f, (rect.height - total_h) * 0.5f);

  for (const auto &line : lines) {
    if (line.empty()) {
      y += line_h; // blank line from a "\n\n"
      continue;
    }
    // Line width sums the runs at their own weights; a bold run is wider than
    // the same characters in regular, so centring on a single-font measure
    // would drift the whole line left.
    float line_w = 0.f;
    for (const auto &run : line)
      line_w += weighted_width(run.text, run.weight);

    // Mirror position_text_ex's alignment maths so a label lands in the same
    // place whether it is drawn as plain text or as styled runs.
    float x = rect.x + inset.x;
    if (alignment == TextAlignment::Center)
      x = std::max(rect.x + inset.x,
                   rect.x + inset.x + (rect.width - 2.f * inset.x - line_w) / 2.f);
    else if (alignment == TextAlignment::Right)
      x = rect.x + rect.width - inset.x - line_w;

    for (const auto &run : line) {
      const float w = weighted_width(run.text, run.weight);
      RectangleType run_rect{x, y, w, line_h};
      // draw_text_in_rect draws with the ACTIVE font, so the weight has to be
      // swapped in around the call and put back afterwards.
      const std::string want = fm.resolve_weighted(base_font, run.weight);
      const bool swap = want != fm.active_font;
      if (swap)
        fm.set_active(want);
      // Same as the per-line calls above: a run's rect is sized to that run,
      // so charging it margins would report overflow for every one of them.
      draw_text_in_rect(fm, run.text, run_rect, TextAlignment::Left, run.color,
                        show_debug_indicator, stroke, shadow, rotation,
                        rot_center_x, rot_center_y, TextOverflow::Clip,
                        letter_spacing, font_size, /*report_overflow=*/false,
                        Vector2Type{0.f, 0.f});
      if (swap)
        fm.set_active(base_font);
      x += w;
    }
    y += line_h;
  }
}

static inline Vector2Type
position_texture(texture_manager::Texture, Vector2Type size,
                 RectangleType container,
                 texture_manager::HasTexture::Alignment alignment,
                 Vector2Type margin_px = {0.f, 0.f}) {
  // Calculate the text position based on the alignment and margins
  Vector2Type position;

  switch (alignment) {
  case texture_manager::HasTexture::Alignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y + size.x,
    };
    break;
  case texture_manager::HasTexture::Alignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x + (container.width / 2) + (size.x / 2),
        .y = container.y + margin_px.y + (container.height / 2) + (size.y / 2),
    };
    break;
  case texture_manager::HasTexture::Alignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x + size.x,
        .y = container.y + margin_px.y + size.y,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return Vector2Type{
      .x = position.x,
      .y = position.y,
  };
}

static inline void
draw_texture_in_rect(texture_manager::Texture texture, RectangleType rect,
                     texture_manager::HasTexture::Alignment alignment,
                     texture_manager::Color tint = colors::UI_WHITE) {
  float scale = (float)texture.height / rect.height;
  Vector2Type size = {
      (float)texture.width / scale,
      (float)texture.height / scale,
  };

  Vector2Type location = position_texture(texture, size, rect, alignment);

  texture_manager::draw_texture_pro(texture,
                                    RectangleType{
                                        0.0f,
                                        0.0f,
                                        (float)texture.width,
                                        (float)texture.height,
                                    },
                                    RectangleType{
                                        .x = location.x,
                                        .y = location.y,
                                        .width = size.x,
                                        .height = size.y,
                                    },
                                    size, 0.f, tint);
}

namespace detail {

template <typename InputAction, typename Draw>
void draw_layer_scrollbars(UIContext<InputAction> &context, size_t begin,
                           size_t end, Draw draw) {
  for (size_t index = begin; index < end; ++index) {
    const auto &cmd = context.render_cmds[index];
    auto owner = UICollectionHolder::getEntityForID(cmd.id);
    if (!owner.valid()) continue;
    Entity &entity = owner.asE();
    if (!entity.has<HasScrollView>() || !entity.has<UIComponent>()) continue;
    const auto &cmp = entity.get<UIComponent>();
    if (!cmp.was_rendered_to_screen || detail::is_hidden_for_render(entity)) continue;
    const auto &scroll = entity.get<HasScrollView>();
    // Ride an outer view's scroll the same way the frame around us does.
    RectangleType view = cmp.rect();
    const Vector2Type outer = detail::accumulated_scroll_offset(entity);
    view.x -= outer.x;
    view.y -= outer.y;
    const auto metrics = scrollbar_metrics(
        scroll, cmp.resolved_scaling_mode, context.screen_height);
    const auto [has_clip, clip] = detail::compute_intersected_clip_rect(entity);
    const float opacity = detail::compute_effective_opacity(entity);
    const auto draw_axis = [&](bool vertical) {
      const auto geometry = scrollbar_geometry(
          scroll, view, vertical, metrics.thickness, metrics.min_thumb);
      if (!geometry.visible) return;
      const auto track = colors::opacity_pct(scroll.scrollbar_track_color.value_or(
          context.theme.from_usage(scroll.scrollbar_track_usage)), opacity);
      const auto thumb = colors::opacity_pct(scroll.scrollbar_thumb_color.value_or(
          context.theme.from_usage(scroll.scrollbar_thumb_usage)), opacity);
      // Fully rounded: at 6px wide that is a capsule, which reads as a bar.
      draw(cmd, geometry.track, track, has_clip, clip);
      draw(cmd, geometry.thumb, thumb, has_clip, clip);
    };
    if (scroll.vertical_enabled) draw_axis(true);
    if (scroll.horizontal_enabled) draw_axis(false);
  }
}

}

template <typename InputAction>
struct RenderDebugAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {
  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;

  int level = 0;
  int indent = 0;
  EntityID isolated_id = -1;
  bool isolate_enabled = false;
  enum struct IsolationMode { NodeOnly, NodeAndDescendants };
  IsolationMode isolation_mode = IsolationMode::NodeOnly;
  UIEntityMappingCache *cache = nullptr;

  float fontSize = 20.0f;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
    // Don't run if cache singleton doesn't exist yet
    if (!EntityHelper::get_singleton_cmp<UIEntityMappingCache>()) {
      return false;
    }

    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector inpc = input::get_input_collector();
      for (auto &actions_done : inpc.inputs()) {
        if (static_cast<InputAction>(actions_done.action) == toggle_action) {
          enabled = !enabled;
          break;
        }
      }
    }
    return enabled;
  }

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

    // Get cached entity mapping
    this->cache = EntityHelper::get_singleton_cmp<UIEntityMappingCache>();

    draw_text(fmt::format("mouse({}, {})", this->context->mouse.pos.x,
                          this->context->mouse.pos.y)
                  .c_str(),
              0.0f, 0.0f, fontSize,
              this->context->theme.from_usage(Theme::Usage::Font));

    // starting at 1 to avoid the mouse text
    this->level = 1;
    this->indent = 0;
  }

  bool is_descendant_of_isolated(const Entity &entity) const {
    if (!isolate_enabled)
      return false;
    if (entity.id == isolated_id)
      return false;
    EntityID current_id = entity.id;
    int guard = 0;
    while (guard < 64) {
      OptEntity opt_cur = UICollectionHolder::getEntityForID(current_id);
      if (!opt_cur.valid() || !opt_cur.asE().has<UIComponent>())
        break;
      const UIComponent &cur_cmp = opt_cur.asE().get<UIComponent>();
      if (cur_cmp.parent < 0)
        break;
      if (cur_cmp.parent == isolated_id)
        return true;
      current_id = cur_cmp.parent;
      ++guard;
    }
    return false;
  }

  void render_me(const Entity &entity) {
    const UIComponent &cmp = entity.get<UIComponent>();

    const float x = 10 * indent;
    const float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      component_name = cmpdebug.name();
    }

    const std::string widget_str = fmt::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    auto *debug_fm = EntityHelper::get_singleton_cmp<FontManager>();
    const Font debug_font =
        debug_fm ? debug_fm->get_active_font() : get_default_font();
    const float text_width =
        measure_text(debug_font, widget_str.c_str(), fontSize, 1.f).x;
    const Rectangle debug_label_location =
        Rectangle{x, y, text_width, fontSize};

    const bool is_hovered =
        is_mouse_inside(this->context->mouse.pos, debug_label_location);
    bool show = true;
    if (isolate_enabled) {
      if (entity.id == isolated_id) {
        show = true;
      } else if (isolation_mode == IsolationMode::NodeAndDescendants) {
        show = is_descendant_of_isolated(entity);
      } else {
        show = false;
      }
    }
    const bool hidden = !show;

    const auto color_or_hidden = [hidden](Color c) {
      return hidden ? colors::opacity_pct(c, 0.f) : c;
    };

    if (is_hovered) {
      draw_rectangle_outline(cmp.rect(),
                             color_or_hidden(this->context->theme.from_usage(
                                 Theme::Usage::Error)));
      draw_rectangle_outline(cmp.bounds(), color_or_hidden(colors::UI_BLACK));
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLUE));
    } else {
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLACK));
    }

    Color baseText = this->context->is_hot(entity.id)
                         ? this->context->theme.from_usage(Theme::Usage::Error)
                         : this->context->theme.from_usage(Theme::Usage::Font);
    draw_text_ex(debug_font, widget_str.c_str(), Vector2Type{x, y}, fontSize,
                 1.f, color_or_hidden(baseText));

    const bool left_released = input::is_mouse_button_released(0);
    const bool right_released = input::is_mouse_button_released(1);
    if (is_hovered && (left_released || right_released)) {
      IsolationMode new_mode = left_released ? IsolationMode::NodeAndDescendants
                                             : IsolationMode::NodeOnly;
      if (isolate_enabled && isolated_id == entity.id &&
          isolation_mode == new_mode) {
        isolate_enabled = false;
        isolated_id = -1;
      } else {
        isolate_enabled = true;
        isolated_id = entity.id;
        isolation_mode = new_mode;
      }
    }
  }

  void render(const Entity &entity) {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.was_rendered_to_screen) {
      render_me(entity);
      level++;
    }

    indent++;
    for (EntityID child : cmp.children) {
      render(cache->to_ent(child));
    }
    indent--;
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &,
                                     AutoLayoutRoot &, float) {
    render(entity);
    level += 2;
    indent = 0;
  }
};

template <typename InputAction>
struct RenderImm : System<UIContext<InputAction>, FontManager> {
  RenderImm() : System<UIContext<InputAction>, FontManager>() {
    this->include_derived_children = true;
  }

  void render_shadow(const Entity &entity, RectangleType draw_rect,
                     const std::bitset<4> &corner_settings,
                     float effective_opacity, float roundness = 0.5f,
                     int segments = 8) {
    if (!entity.has<HasShadow>())
      return;

    const Shadow &shadow = entity.get<HasShadow>().shadow;
    Color shadow_color = shadow.color;
    if (effective_opacity < 1.0f) {
      shadow_color.a =
          static_cast<unsigned char>(shadow_color.a * effective_opacity);
    }

    if (shadow.style == ShadowStyle::Hard) {
      // Hard shadow: single offset rectangle
      RectangleType shadow_rect = {draw_rect.x + shadow.offset_x,
                                   draw_rect.y + shadow.offset_y,
                                   draw_rect.width, draw_rect.height};
      if (corner_settings.any()) {
        draw_rectangle_rounded(shadow_rect, roundness, segments, shadow_color,
                               corner_settings);
      } else {
        draw_rectangle(shadow_rect, shadow_color);
      }
    } else {
      // Soft shadow: layered rectangles for blur effect
      int layers = static_cast<int>(shadow.blur_radius / 2.0f);
      layers = std::max(3, std::min(layers, 8)); // Clamp between 3-8 layers

      for (int i = layers; i >= 0; --i) {
        float spread = shadow.blur_radius *
                       (static_cast<float>(i) / static_cast<float>(layers));
        float alpha_factor =
            1.0f - (static_cast<float>(i) / static_cast<float>(layers + 1));

        RectangleType shadow_rect = {
            draw_rect.x + shadow.offset_x - spread * 0.5f,
            draw_rect.y + shadow.offset_y - spread * 0.5f,
            draw_rect.width + spread, draw_rect.height + spread};

        Color layer_color = shadow_color;
        layer_color.a = static_cast<unsigned char>(
            static_cast<float>(shadow_color.a) * alpha_factor *
            (1.0f / static_cast<float>(layers)));

        if (corner_settings.any()) {
          draw_rectangle_rounded(shadow_rect, roundness, segments, layer_color,
                                 corner_settings);
        } else {
          draw_rectangle(shadow_rect, layer_color);
        }
      }
    }
  }

  void render_nine_slice(const Entity &entity, RectangleType draw_rect,
                         float effective_opacity) {
    if (!entity.has<HasNineSliceBorder>())
      return;

    const NineSliceBorder &nine_slice =
        entity.get<HasNineSliceBorder>().nine_slice;
    Color tint = nine_slice.tint;
    if (effective_opacity < 1.0f) {
      tint.a = static_cast<unsigned char>(tint.a * effective_opacity);
    }

    draw_texture_npatch(nine_slice.texture, draw_rect, nine_slice.left,
                        nine_slice.top, nine_slice.right, nine_slice.bottom,
                        tint);
  }

  void render_circular_progress(const Entity &entity, RectangleType draw_rect,
                                float effective_opacity) {
    if (!entity.has<HasCircularProgressState>()) {
      return;
    }

    const HasCircularProgressState &state =
        entity.get<HasCircularProgressState>();

    // Calculate center and radius from the draw_rect
    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    float outerRadius = std::min(draw_rect.width, draw_rect.height) / 2.0f;
    float innerRadius = outerRadius - state.thickness;
    if (innerRadius < 0.0f)
      innerRadius = 0.0f;

    // Apply opacity to colors
    Color track_color = state.track_color;
    Color fill_color = state.fill_color;
    if (effective_opacity < 1.0f) {
      track_color = colors::opacity_pct(track_color, effective_opacity);
      fill_color = colors::opacity_pct(fill_color, effective_opacity);
    }

    // Calculate segments based on radius for smoothness
    int segments = std::max(32, static_cast<int>(outerRadius * 0.5f));

    // Draw background track (full circle)
    draw_ring(centerX, centerY, innerRadius, outerRadius, segments,
              track_color);

    // Draw progress fill (arc from start_angle)
    if (state.value > 0.001f) {
      float end_angle = state.start_angle + (state.value * 360.0f);
      draw_ring_segment(centerX, centerY, innerRadius, outerRadius,
                        state.start_angle, end_angle, segments, fill_color);
    }
  }

  void render_bevel(const Entity &entity, RectangleType draw_rect,
                    float effective_opacity) {
    if (!entity.has<HasBevelBorder>())
      return;

    const BevelBorder &bevel = entity.get<HasBevelBorder>().bevel;
    if (!bevel.has_bevel())
      return;

    Color light = bevel.light_color;
    Color dark = bevel.dark_color;
    if (effective_opacity < 1.0f) {
      light = colors::opacity_pct(light, effective_opacity);
      dark = colors::opacity_pct(dark, effective_opacity);
    }

    Color base_fill = colors::UI_WHITE;
    if (entity.has<HasColor>()) {
      base_fill = entity.get<HasColor>().color();
    }
    if (effective_opacity < 1.0f) {
      base_fill = colors::opacity_pct(base_fill, effective_opacity);
    }

    const Color strong_light = light;
    const Color strong_dark = dark;
    const Color mid_light = colors::lighten(base_fill, 0.35f);
    const Color mid_dark = colors::darken(base_fill, 0.35f);

    const Color base_top_left =
        bevel.style == BevelStyle::Raised ? strong_light : strong_dark;
    const Color base_bottom_right =
        bevel.style == BevelStyle::Raised ? strong_dark : strong_light;
    const Color inner_top_left =
        bevel.style == BevelStyle::Raised ? mid_light : mid_dark;
    const Color inner_bottom_right =
        bevel.style == BevelStyle::Raised ? mid_dark : mid_light;

    int layers = std::max(1, static_cast<int>(std::ceil(bevel.thickness)));
    for (int i = 0; i < layers; ++i) {
      bool inner = i > 0;
      Color top_left = inner ? inner_top_left : base_top_left;
      Color bottom_right = inner ? inner_bottom_right : base_bottom_right;

      float inset = static_cast<float>(i);
      float w = draw_rect.width - (inset * 2.0f);
      float h = draw_rect.height - (inset * 2.0f);
      if (w <= 0.0f || h <= 0.0f)
        break;

      RectangleType top = {draw_rect.x + inset, draw_rect.y + inset, w, 1.0f};
      RectangleType left = {draw_rect.x + inset, draw_rect.y + inset, 1.0f, h};
      RectangleType bottom = {draw_rect.x + inset,
                              draw_rect.y + inset + h - 1.0f, w, 1.0f};
      RectangleType right = {draw_rect.x + inset + w - 1.0f,
                             draw_rect.y + inset, 1.0f, h};

      draw_rectangle(top, top_left);
      draw_rectangle(left, top_left);
      draw_rectangle(bottom, bottom_right);
      draw_rectangle(right, bottom_right);
    }
  }

  void render_me(UIContext<InputAction> &context, FontManager &font_manager,
                 Entity &entity) {
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    const float effective_opacity = detail::compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    // Scroll of all scroll-view ancestors; reused for focus_rect below.
    const Vector2Type scroll_offset = detail::accumulated_scroll_offset(entity);
    draw_rect.y -= scroll_offset.y;
    draw_rect.x -= scroll_offset.x;

    if (entity.has<HasUIModifiers>()) {
      draw_rect = entity.get<HasUIModifiers>().apply_modifier(draw_rect);
    }

    // Get rotation from modifiers (applied separately since rectangles rotate
    // around center)
    float rotation = entity.has<HasUIModifiers>()
                         ? entity.get<HasUIModifiers>().rotation
                         : 0.0f;

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();
    float roundness =
        entity.has<HasRoundedCorners>()
            ? resolve_roundness(entity.get<HasRoundedCorners>().radius_px,
                                entity.get<HasRoundedCorners>().roundness, draw_rect)
            : 0.5f;
    int segments = entity.has<HasRoundedCorners>()
                       ? entity.get<HasRoundedCorners>().segments
                       : 8;

    // Push rotation transform - all subsequent drawing will be rotated around
    // component center
    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    push_rotation(centerX, centerY, rotation);

    // Draw shadow first (behind the element)
    render_shadow(entity, draw_rect, corner_settings, effective_opacity,
                  roundness, segments);

    // Custom draw (behind): drawn before the widget's own fill.
    if (entity.has<HasOnDraw>() && entity.get<HasOnDraw>().bg)
      entity.get<HasOnDraw>().bg(draw_rect);

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      // In FollowsMostRecentInput the ring is the only highlight, so skip the
      // hover fill entirely (mouse and keyboard render identically).
      if (context.theme.highlight_mode == HighlightMode::Split &&
          context.is_hot(entity.id) &&
          !entity.template get<HasColor>().skip_hover_override) {
        col = entity.template get<HasColor>().hover_bg();
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      if (col.a > 0) {
        draw_rectangle_rounded(draw_rect, roundness, segments, col,
                               corner_settings);
      }
    }

    // Over the fill: filled panel art would otherwise be hidden by it, and a
    // transparent-center border still lets the fill show through.
    if (entity.has<HasNineSliceBorder>()) {
      render_nine_slice(entity, draw_rect, effective_opacity);
    }

    render_bevel(entity, draw_rect, effective_opacity);

    // Render circular progress if present (uses ring primitives instead of
    // rectangles)
    render_circular_progress(entity, draw_rect, effective_opacity);

    if (entity.has<HasBorder>()) {
      const Border &border = entity.template get<HasBorder>().border;
      if (border.has_border()) {
        // Draw a dashed run of small rects along a side rect. Orientation is
        // inferred from aspect: wider-than-tall = horizontal, else vertical.
        // ponytail: dotted ignores rounded corners (four straight dashed
        // sides).
        auto draw_dashed = [&](float sx, float sy, float sw, float sh,
                               float thickness, Color c) {
          float dash = std::max(1.0f, thickness * 2.0f);
          float gap_len = std::max(1.0f, thickness * 2.0f);
          float step = dash + gap_len;
          bool horizontal = sw >= sh;
          float len = horizontal ? sw : sh;
          for (float pos = 0.0f; pos < len; pos += step) {
            float seg = std::min(dash, len - pos);
            if (horizontal)
              draw_rectangle(RectangleType{sx + pos, sy, seg, sh}, c);
            else
              draw_rectangle(RectangleType{sx, sy + pos, sw, seg}, c);
          }
        };
        bool uniform_dotted =
            border.is_uniform() && border.top.style == BorderStyle::Dotted;
        if (border.is_uniform() && !uniform_dotted) {
          Color border_col = border.uniform_color();
          if (effective_opacity < 1.0f) {
            border_col = colors::opacity_pct(border_col, effective_opacity);
          }
          draw_rectangle_rounded_lines(draw_rect, roundness, segments,
                                       border_col, corner_settings);
        } else {
          // Per-side (and uniform-dotted) border rendering
          float x = draw_rect.x, y = draw_rect.y;
          float w = draw_rect.width, h = draw_rect.height;
          auto draw_side = [&](const BorderSide &side, float sx, float sy,
                               float sw, float sh) {
            if (!side.has_border())
              return;
            Color c = side.color;
            if (effective_opacity < 1.0f)
              c = colors::opacity_pct(c, effective_opacity);
            if (side.style == BorderStyle::Dotted)
              draw_dashed(sx, sy, sw, sh, side.thickness.value, c);
            else
              draw_rectangle(RectangleType{sx, sy, sw, sh}, c);
          };
          float tt = border.top.thickness.value;
          float bt = border.bottom.thickness.value;
          float lt = border.left.thickness.value;
          float rt = border.right.thickness.value;
          draw_side(border.top, x, y, w, tt);
          draw_side(border.bottom, x, y + h - bt, w, bt);
          draw_side(border.left, x, y + tt, lt, h - tt - bt);
          draw_side(border.right, x + w - rt, y + tt, rt, h - tt - bt);
        }
      }
    }

    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      warn_ignored_label_padding(entity, cmp);
      Color font_col = detail::resolve_label_color(hasLabel, context.theme);

      if (effective_opacity < 1.0f) {
        font_col = colors::opacity_pct(font_col, effective_opacity);
      }

      // Prepare text stroke with opacity applied if needed
      std::optional<TextStroke> stroke = hasLabel.text_stroke;
      if (stroke.has_value() && effective_opacity < 1.0f) {
        stroke->color = colors::opacity_pct(stroke->color, effective_opacity);
      }

      // Prepare text shadow with opacity applied if needed
      std::optional<TextShadow> shadow = hasLabel.text_shadow;
      if (shadow.has_value() && effective_opacity < 1.0f) {
        shadow->color = colors::opacity_pct(shadow->color, effective_opacity);
      }

      // No nine-slice inset here: component_init already put the slice into
      // text_inset, which position_text applies. Subtracting it again left a
      // 70px box with a 16px slice only 6px of room for a 15px font.
      RectangleType text_rect = draw_rect;

      // TODO: unify this font-size resolution with the batched path
      // (see position_text_ex call ~100 lines below) so they don't diverge.
      float explicit_fs = 0.f;
      if (cmp.font_size_explicitly_set) {
        float uis = imm::ThemeDefaults::get().theme.ui_scale;
        explicit_fs = resolve_to_pixels(cmp.font_size, context.screen_height,
                                        cmp.resolved_scaling_mode, uis);
      }

      const Vector2Type immediate_inset =
          resolve_text_inset(context.theme, hasLabel.text_inset);

      RectangleType label_rect = text_rect;
      label_rect.x += hasLabel.text_x_offset;
      label_rect.width -= hasLabel.text_x_offset;
      label_rect.y += hasLabel.text_y_offset;
      // Without this branch with_styled_label is a no-op here, so the same
      // UI renders differently depending on `use_batched`.
      if (!hasLabel.spans.empty()) {
        draw_runs_in_rect(font_manager, hasLabel.spans, label_rect,
                          hasLabel.alignment, SHOW_TEXT_OVERFLOW_DEBUG, stroke,
                          shadow, rotation, centerX, centerY,
                          hasLabel.text_overflow, hasLabel.letter_spacing,
                          explicit_fs, hasLabel.label, immediate_inset);
      } else {
        draw_text_in_rect(font_manager, hasLabel.label.c_str(), label_rect,
                          hasLabel.alignment, font_col,
                          SHOW_TEXT_OVERFLOW_DEBUG, stroke, shadow, rotation,
                          centerX, centerY, hasLabel.text_overflow,
                          hasLabel.letter_spacing, explicit_fs,
                          /*report_overflow=*/true, immediate_inset);
      }
    }

    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      Color tex_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        tex_col = colors::opacity_pct(tex_col, effective_opacity);
      }
      draw_texture_in_rect(texture.texture, draw_rect, texture.alignment,
                           tex_col);
    } else if (entity.has<ui::HasImage>()) {
      const ui::HasImage &img = entity.get<ui::HasImage>();
      texture_manager::Rectangle src =
          img.source_rect.value_or(texture_manager::Rectangle{
              0.0f, 0.0f, (float)img.texture.width, (float)img.texture.height});

      // Scale to fit height of rect
      float scale = src.height / draw_rect.height;
      Vector2Type size = {src.width / scale, src.height / scale};
      Vector2Type location =
          position_texture(img.texture, size, draw_rect, img.alignment);

      Color img_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        img_col = colors::opacity_pct(img_col, effective_opacity);
      }
      texture_manager::draw_texture_pro(img.texture, src,
                                        RectangleType{
                                            .x = location.x,
                                            .y = location.y,
                                            .width = size.x,
                                            .height = size.y,
                                        },
                                        size, 0.f, img_col);
    }

    // Custom draw (on top): drawn after all of the widget's own primitives.
    if (entity.has<HasOnDraw>() && entity.get<HasOnDraw>().fg)
      entity.get<HasOnDraw>().fg(draw_rect);

    pop_rotation();
  }

  void render(UIContext<InputAction> &context, FontManager &font_manager,
              Entity &entity) {
    if (detail::is_hidden_for_render(entity)) return;
    if (entity.has<HasDragPreview>()) {
      detail::draw_drag_preview(entity, [&](Entity &source) { render(context, font_manager, source); });
      return;
    }
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.font_name != UIComponent::UNSET_FONT) {
      font_manager.set_active(
          font_manager.resolve_weighted(cmp.font_name, cmp.font_weight));
    }

    // Scroll views define their own viewport — don't clip them by ancestors.
    // Everything else (including HasClipChildren) gets clipped by the
    // intersection of ALL ancestor clip rects.
    bool needs_scissor = false;
    if (!entity.has<HasScrollView>()) {
      auto [has_clip, clip_rect] =
          detail::compute_intersected_clip_rect(entity);
      if (has_clip) {
        // Off-screen cull: if this entity is entirely outside its clip
        // viewport, skip drawing it. Scrolled-away rows (e.g. the thousands of
        // diff lines in a long diff) no longer pay text-shaping + draw cost
        // every frame — only the visible slice does. Children carry their own
        // render_cmds and are culled the same way, so returning here is safe.
        // Use the same scroll-adjusted rect render_me draws with, so the
        // intersection is against where the row actually paints on screen.
        // Skip the cull for modifier-transformed elements — their painted rect
        // is offset by apply_modifier() (translate/rotate) which we don't model
        // here, so culling on the untransformed rect could wrongly drop them.
        if (!entity.has<HasUIModifiers>()) {
          RectangleType er = cmp.rect();
          const Vector2Type so = detail::accumulated_scroll_offset(entity);
          er.x -= so.x;
          er.y -= so.y;
          RectangleType vis = detail::intersect_rects(er, clip_rect);
          if (vis.width <= 0.0f || vis.height <= 0.0f)
            return;
        }
        begin_scissor_mode(static_cast<int>(clip_rect.x),
                           static_cast<int>(clip_rect.y),
                           static_cast<int>(clip_rect.width),
                           static_cast<int>(clip_rect.height));
        needs_scissor = true;
      }
    }

    // Update scroll view content size before rendering (after layout is done)
    if (entity.has<HasScrollView>()) {
      detail::update_scroll_view_content_size(entity);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>() || entity.has<HasOnDraw>() ||
        entity.has<HasCircularProgressState>() || entity.has<HasScrollView>() ||
        context.visual_focus_id == entity.id) {
      render_me(context, font_manager, entity);
    }

    if (needs_scissor) {
      end_scissor_mode();
    }

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with_derived(Entity &entity,
                                     UIContext<InputAction> &context,
                                     FontManager &font_manager,
                                     float) override {
    // Stable-sort by layer only. render_cmds are queued in document pre-order
    // (parent before child), which is the correct paint order within a layer.
    // Do NOT tiebreak by entity id: ids are recycled across screens and are
    // not monotonic with document order, so an id tiebreak lets an opaque
    // ancestor paint over its own lower-id descendants.
#if __WIN32
    // Bubble sort (mingw lacks std::ranges::sort); stable because it only
    // swaps on a strictly greater layer.
    for (size_t i = 0; i < context.render_cmds.size(); ++i) {
      for (size_t j = i + 1; j < context.render_cmds.size(); ++j) {
        if (context.render_cmds[i].layer > context.render_cmds[j].layer) {
          std::swap(context.render_cmds[i], context.render_cmds[j]);
        }
      }
    }
#else
    std::ranges::stable_sort(
        context.render_cmds,
        [](RenderInfo a, RenderInfo b) { return a.layer < b.layer; });
#endif

    // The buffer holds THIS frame, so an assertion reads what is on screen
    // rather than everything drawn since the process started. Skipped when the
    // app calls capture::begin_frame(): clearing here would throw away what it
    // drew before the UI, which for a game is the whole world.
    if (capture::enabled() && !capture::app_owns_frame())
      capture::clear();

    const auto focus_paint = detail::prepare_focus_paint(context);
    const auto scrollbar = [](const RenderInfo &cmd, RectangleType rect,
                              ColorType color, bool has_clip, RectangleType clip) {
      capture::Scope attribute(cmd.id, cmd.layer);
      if (has_clip)
        begin_scissor_mode(static_cast<int>(clip.x), static_cast<int>(clip.y),
                           static_cast<int>(clip.width), static_cast<int>(clip.height));
      draw_rectangle_rounded(rect, 1.f, 6, color, std::bitset<4>().set());
      if (has_clip) end_scissor_mode();
    };
    size_t layer_begin = 0;
    int cursor_to_set = 0; // Default cursor
    for (size_t index = 0; index < context.render_cmds.size(); ++index) {
      const auto &cmd = context.render_cmds[index];
      if (cmd.layer != context.render_cmds[layer_begin].layer) {
        detail::draw_layer_scrollbars(context, layer_begin, index, scrollbar);
        layer_begin = index;
      }
      auto id = cmd.id;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(id);
      if (!opt_ent.valid())
        continue; // Skip stale entity IDs
      Entity &ent = opt_ent.asE();
      capture::Scope attribute(ent.id, cmd.layer);
      render(context, font_manager, ent);
      if (focus_paint && focus_paint->after_command == index)
        detail::draw_focus_paint(*focus_paint);
      if (context.is_hot(ent.id) && ent.has<HasCursor>()) {
        cursor_to_set = to_cursor_id(ent.get<HasCursor>().cursor);
      }
    }
    detail::draw_layer_scrollbars(context, layer_begin, context.render_cmds.size(), scrollbar);
    set_mouse_cursor(cursor_to_set);
    context.render_cmds.clear();
  }
};

// Batched renderer system - collects render commands into a buffer
// then executes them all at once for better batching opportunities
template <typename InputAction>
struct RenderBatched : System<UIContext<InputAction>, FontManager> {
  RenderBatched() : System<UIContext<InputAction>, FontManager>() {
    this->include_derived_children = true;
  }

  void collect_shadow(RenderCommandBuffer &buffer, const Entity &entity,
                      RectangleType draw_rect,
                      const std::bitset<4> &corner_settings,
                      float effective_opacity, int layer,
                      float roundness = 0.5f, int segments = 8) {
    if (!entity.has<HasShadow>())
      return;

    const Shadow &shadow = entity.get<HasShadow>().shadow;
    Color shadow_color = shadow.color;
    if (effective_opacity < 1.0f) {
      shadow_color.a =
          static_cast<unsigned char>(shadow_color.a * effective_opacity);
    }

    if (shadow.style == ShadowStyle::Hard) {
      RectangleType shadow_rect = {draw_rect.x + shadow.offset_x,
                                   draw_rect.y + shadow.offset_y,
                                   draw_rect.width, draw_rect.height};
      if (corner_settings.any()) {
        buffer.add_rounded_rectangle(shadow_rect, shadow_color, roundness,
                                     segments, corner_settings, layer,
                                     entity.id);
      } else {
        buffer.add_rectangle(shadow_rect, shadow_color, layer, entity.id);
      }
    } else {
      int layers = static_cast<int>(shadow.blur_radius / 2.0f);
      layers = std::max(3, std::min(layers, 8));

      for (int i = layers; i >= 0; --i) {
        float spread = shadow.blur_radius *
                       (static_cast<float>(i) / static_cast<float>(layers));
        float alpha_factor =
            1.0f - (static_cast<float>(i) / static_cast<float>(layers + 1));

        RectangleType shadow_rect = {
            draw_rect.x + shadow.offset_x - spread * 0.5f,
            draw_rect.y + shadow.offset_y - spread * 0.5f,
            draw_rect.width + spread, draw_rect.height + spread};

        Color layer_color = shadow_color;
        layer_color.a = static_cast<unsigned char>(
            static_cast<float>(shadow_color.a) * alpha_factor *
            (1.0f / static_cast<float>(layers)));

        if (corner_settings.any()) {
          buffer.add_rounded_rectangle(shadow_rect, layer_color, roundness,
                                       segments, corner_settings, layer,
                                       entity.id);
        } else {
          buffer.add_rectangle(shadow_rect, layer_color, layer, entity.id);
        }
      }
    }
  }

  void collect_nine_slice(RenderCommandBuffer &buffer, const Entity &entity,
                          RectangleType draw_rect, float effective_opacity,
                          int layer) {
    if (!entity.has<HasNineSliceBorder>())
      return;

    const NineSliceBorder &nine_slice =
        entity.get<HasNineSliceBorder>().nine_slice;
    Color tint = nine_slice.tint;
    if (effective_opacity < 1.0f) {
      tint.a = static_cast<unsigned char>(tint.a * effective_opacity);
    }

    buffer.add_nine_slice(draw_rect, nine_slice.texture, nine_slice.left,
                          nine_slice.top, nine_slice.right, nine_slice.bottom,
                          tint, layer, entity.id);
  }

  void collect_circular_progress(RenderCommandBuffer &buffer,
                                 const Entity &entity, RectangleType draw_rect,
                                 float effective_opacity, int layer) {
    if (!entity.has<HasCircularProgressState>()) {
      return;
    }

    const HasCircularProgressState &state =
        entity.get<HasCircularProgressState>();

    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    float outerRadius = std::min(draw_rect.width, draw_rect.height) / 2.0f;
    float innerRadius = outerRadius - state.thickness;
    if (innerRadius < 0.0f)
      innerRadius = 0.0f;

    Color track_color = state.track_color;
    Color fill_color = state.fill_color;
    if (effective_opacity < 1.0f) {
      track_color = colors::opacity_pct(track_color, effective_opacity);
      fill_color = colors::opacity_pct(fill_color, effective_opacity);
    }

    int segments = std::max(32, static_cast<int>(outerRadius * 0.5f));

    // Background track
    buffer.add_ring(centerX, centerY, innerRadius, outerRadius, segments,
                    track_color, layer, entity.id);

    // Progress fill
    if (state.value > 0.001f) {
      float end_angle = state.start_angle + (state.value * 360.0f);
      buffer.add_ring_segment(centerX, centerY, innerRadius, outerRadius,
                              state.start_angle, end_angle, segments,
                              fill_color, layer, entity.id);
    }
  }

  void collect_bevel(RenderCommandBuffer &buffer, const Entity &entity,
                     RectangleType draw_rect, float effective_opacity,
                     int layer) {
    if (!entity.has<HasBevelBorder>())
      return;

    const BevelBorder &bevel = entity.get<HasBevelBorder>().bevel;
    if (!bevel.has_bevel())
      return;

    Color light = bevel.light_color;
    Color dark = bevel.dark_color;
    if (effective_opacity < 1.0f) {
      light = colors::opacity_pct(light, effective_opacity);
      dark = colors::opacity_pct(dark, effective_opacity);
    }

    Color base_fill = colors::UI_WHITE;
    if (entity.has<HasColor>()) {
      base_fill = entity.get<HasColor>().color();
    }
    if (effective_opacity < 1.0f) {
      base_fill = colors::opacity_pct(base_fill, effective_opacity);
    }

    const Color strong_light = light;
    const Color strong_dark = dark;
    const Color mid_light = colors::lighten(base_fill, 0.35f);
    const Color mid_dark = colors::darken(base_fill, 0.35f);

    const Color base_top_left =
        bevel.style == BevelStyle::Raised ? strong_light : strong_dark;
    const Color base_bottom_right =
        bevel.style == BevelStyle::Raised ? strong_dark : strong_light;
    const Color inner_top_left =
        bevel.style == BevelStyle::Raised ? mid_light : mid_dark;
    const Color inner_bottom_right =
        bevel.style == BevelStyle::Raised ? mid_dark : mid_light;

    int layers = std::max(1, static_cast<int>(std::ceil(bevel.thickness)));
    for (int i = 0; i < layers; ++i) {
      bool inner = i > 0;
      Color top_left = inner ? inner_top_left : base_top_left;
      Color bottom_right = inner ? inner_bottom_right : base_bottom_right;

      float inset = static_cast<float>(i);
      float w = draw_rect.width - (inset * 2.0f);
      float h = draw_rect.height - (inset * 2.0f);
      if (w <= 0.0f || h <= 0.0f)
        break;

      RectangleType top = {draw_rect.x + inset, draw_rect.y + inset, w, 1.0f};
      RectangleType left = {draw_rect.x + inset, draw_rect.y + inset, 1.0f, h};
      RectangleType bottom = {draw_rect.x + inset,
                              draw_rect.y + inset + h - 1.0f, w, 1.0f};
      RectangleType right = {draw_rect.x + inset + w - 1.0f,
                             draw_rect.y + inset, 1.0f, h};

      buffer.add_rectangle(top, top_left, layer, entity.id);
      buffer.add_rectangle(left, top_left, layer, entity.id);
      buffer.add_rectangle(bottom, bottom_right, layer, entity.id);
      buffer.add_rectangle(right, bottom_right, layer, entity.id);
    }
  }

  void collect_me(RenderCommandBuffer &buffer, UIContext<InputAction> &context,
                  FontManager &font_manager, Entity &entity, int layer) {
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();

    const float effective_opacity = detail::compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    // See render_me. Reused for focus_rect below.
    const Vector2Type scroll_offset = detail::accumulated_scroll_offset(entity);
    draw_rect.y -= scroll_offset.y;
    draw_rect.x -= scroll_offset.x;

    if (entity.has<HasUIModifiers>()) {
      draw_rect = entity.get<HasUIModifiers>().apply_modifier(draw_rect);
    }

    // Get rotation from modifiers (applied separately since rectangles rotate
    // around center)
    float rotation = entity.has<HasUIModifiers>()
                         ? entity.get<HasUIModifiers>().rotation
                         : 0.0f;

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();
    float roundness =
        entity.has<HasRoundedCorners>()
            ? resolve_roundness(entity.get<HasRoundedCorners>().radius_px,
                                entity.get<HasRoundedCorners>().roundness, draw_rect)
            : 0.5f;
    int segments = entity.has<HasRoundedCorners>()
                       ? entity.get<HasRoundedCorners>().segments
                       : 8;

    // Shadow first
    collect_shadow(buffer, entity, draw_rect, corner_settings,
                   effective_opacity, layer, roundness, segments);

    // Custom draw (behind): enqueued before the fill so it renders under the
    // widget; the fn pointer is stable on the HasOnDraw component for the frame.
    if (entity.has<HasOnDraw>() && entity.get<HasOnDraw>().bg) {
      buffer.add_custom(draw_rect, &entity.get<HasOnDraw>().bg, layer,
                        entity.id);
    }

    // Background color
    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      // In FollowsMostRecentInput the ring is the only highlight, so skip the
      // hover fill entirely (mouse and keyboard render identically).
      if (context.theme.highlight_mode == HighlightMode::Split &&
          context.is_hot(entity.id) &&
          !entity.template get<HasColor>().skip_hover_override) {
        col = entity.template get<HasColor>().hover_bg();
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      if (col.a > 0) {
        buffer.add_rounded_rectangle(draw_rect, col, roundness, segments,
                                     corner_settings, layer, entity.id,
                                     rotation);
      }
    }

    // Over the fill: filled panel art would otherwise be hidden by it, and a
    // transparent-center border still lets the fill show through.
    if (entity.has<HasNineSliceBorder>()) {
      collect_nine_slice(buffer, entity, draw_rect, effective_opacity, layer);
    }

    // Bevel border
    collect_bevel(buffer, entity, draw_rect, effective_opacity, layer);

    // Circular progress
    collect_circular_progress(buffer, entity, draw_rect, effective_opacity,
                              layer);

    // Border
    if (entity.has<HasBorder>()) {
      const Border &border = entity.template get<HasBorder>().border;
      if (border.has_border()) {
        // Add a dashed run of small rects along a side rect. Orientation is
        // inferred from aspect: wider-than-tall = horizontal, else vertical.
        // ponytail: dotted ignores rounded corners (four straight dashed
        // sides).
        auto add_dashed = [&](float sx, float sy, float sw, float sh,
                              float thickness, Color c) {
          float dash = std::max(1.0f, thickness * 2.0f);
          float gap_len = std::max(1.0f, thickness * 2.0f);
          float step = dash + gap_len;
          bool horizontal = sw >= sh;
          float len = horizontal ? sw : sh;
          for (float pos = 0.0f; pos < len; pos += step) {
            float seg = std::min(dash, len - pos);
            RectangleType r = horizontal ? RectangleType{sx + pos, sy, seg, sh}
                                         : RectangleType{sx, sy + pos, sw, seg};
            buffer.add_rounded_rectangle(r, c, 0.f, 1, corner_settings, layer,
                                         entity.id, 0.f);
          }
        };
        bool uniform_dotted =
            border.is_uniform() && border.top.style == BorderStyle::Dotted;
        if (border.is_uniform() && !uniform_dotted) {
          Color border_col = border.uniform_color();
          if (effective_opacity < 1.0f) {
            border_col = colors::opacity_pct(border_col, effective_opacity);
          }
          buffer.add_rounded_rectangle_outline(draw_rect, border_col, roundness,
                                               segments, corner_settings, layer,
                                               entity.id);
        } else {
          // Per-side (and uniform-dotted) border rendering (filled rectangles)
          float x = draw_rect.x, y = draw_rect.y;
          float w = draw_rect.width, h = draw_rect.height;
          auto add_side = [&](const BorderSide &side, float sx, float sy,
                              float sw, float sh) {
            if (!side.has_border())
              return;
            Color c = side.color;
            if (effective_opacity < 1.0f)
              c = colors::opacity_pct(c, effective_opacity);
            if (side.style == BorderStyle::Dotted) {
              add_dashed(sx, sy, sw, sh, side.thickness.value, c);
              return;
            }
            RectangleType side_rect{sx, sy, sw, sh};
            buffer.add_rounded_rectangle(side_rect, c, 0.f, 1, corner_settings,
                                         layer, entity.id, 0.f);
          };
          float tt = border.top.thickness.value;
          float bt = border.bottom.thickness.value;
          float lt = border.left.thickness.value;
          float rt = border.right.thickness.value;
          add_side(border.top, x, y, w, tt);
          add_side(border.bottom, x, y + h - bt, w, bt);
          add_side(border.left, x, y + tt, lt, h - tt - bt);
          add_side(border.right, x + w - rt, y + tt, rt, h - tt - bt);
        }
      }
    }

    // Label/text
    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      warn_ignored_label_padding(entity, cmp);
      Color font_col = detail::resolve_label_color(hasLabel, context.theme);

      if (effective_opacity < 1.0f) {
        font_col = colors::opacity_pct(font_col, effective_opacity);
      }

      std::optional<TextStroke> stroke = hasLabel.text_stroke;
      if (stroke.has_value() && effective_opacity < 1.0f) {
        stroke->color = colors::opacity_pct(stroke->color, effective_opacity);
      }

      std::optional<TextShadow> shadow = hasLabel.text_shadow;
      if (shadow.has_value() && effective_opacity < 1.0f) {
        shadow->color = colors::opacity_pct(shadow->color, effective_opacity);
      }

      // See render_me: the slice is already in text_inset.
      RectangleType text_rect = draw_rect;

      // When a font size was explicitly set (via with_font / with_font_size),
      // use it as an upper bound so text doesn't auto-grow beyond that size.
      // Default font sizes (from UIStylingDefaults) are NOT applied as caps
      // to preserve the existing auto-fit-to-container behavior.
      float explicit_fs = 0.f;
      if (cmp.font_size_explicitly_set) {
        // Use scaling-mode-aware resolution so font pixels scale with
        // ui_scale in Adaptive mode.
        float uis = imm::ThemeDefaults::get().theme.ui_scale;
        explicit_fs = resolve_to_pixels(cmp.font_size, context.screen_height,
                                        cmp.resolved_scaling_mode, uis);
      }

      const Vector2Type label_inset =
          resolve_text_inset(context.theme, hasLabel.text_inset);

      // Position text to get font size
      TextPositionResult result = position_text_ex(
          font_manager, hasLabel.label.c_str(), text_rect, hasLabel.alignment,
          label_inset, explicit_fs, hasLabel.letter_spacing,
          hasLabel.text_overflow,
          // An element that clips on purpose has already answered the question
          // the overflow warning asks.
          !entity.template has<HasClipChildren>());

      // See render_me: effectively zero, not the readability floor.
      if (result.rect.height >= 1.f) {
        // Handle text overflow ellipsis truncation for batched path
        std::string display_text = hasLabel.label;
        if (hasLabel.text_overflow == TextOverflow::Ellipsis &&
            !hasLabel.label.empty()) {
          Font font = font_manager.get_active_font();
          float font_size = result.rect.height;
          float spacing = 1.f + hasLabel.letter_spacing;
          float max_width = text_rect.width - 2.f * label_inset.x;
          if (max_width > 0.f) {
            Vector2Type ts =
                measure_text(font, hasLabel.label.c_str(), font_size, spacing);
            if (ts.x > max_width) {
              const std::string ellipsis = "...";
              Vector2Type es =
                  measure_text(font, ellipsis.c_str(), font_size, spacing);
              float available = max_width - es.x;
              if (available <= 0.f) {
                display_text = ellipsis;
              } else {
                size_t low = 0, high = hasLabel.label.size(), best = 0;
                while (low <= high && high <= hasLabel.label.size()) {
                  size_t mid = (low + high) / 2;
                  std::string prefix = hasLabel.label.substr(0, mid);
                  Vector2Type ps =
                      measure_text(font, prefix.c_str(), font_size, spacing);
                  if (ps.x <= available) {
                    best = mid;
                    low = mid + 1;
                  } else {
                    if (mid == 0)
                      break;
                    high = mid - 1;
                  }
                }
                display_text = hasLabel.label.substr(0, best) + ellipsis;
              }
            }
          }
        }

        // Pass the container rect (text_rect) not the position rect
        // (result.rect) render_text will handle centering within the container
        RectangleType label_rect = text_rect;
        label_rect.x += hasLabel.text_x_offset;
        label_rect.width -= hasLabel.text_x_offset;
        label_rect.y += hasLabel.text_y_offset;
        label_rect.x += label_inset.x;
        label_rect.y += label_inset.y;
        label_rect.width = std::max(0.f, label_rect.width - 2.f * label_inset.x);
        label_rect.height = std::max(0.f, label_rect.height - 2.f * label_inset.y);
        float centerX = draw_rect.x + draw_rect.width / 2.0f;
        float centerY = draw_rect.y + draw_rect.height / 2.0f;

        // Wrap into lines of coloured runs, one command per run. Plain text is
        // the single-run case. Wrapping needs an explicit font size (see
        // draw_text_in_rect); styled runs lay out either way.
        bool wrapped = false;
        const bool has_spans = !hasLabel.spans.empty();
        const bool wants_wrap =
            hasLabel.text_overflow == TextOverflow::Wrap && explicit_fs > 0.f;
        if ((wants_wrap || has_spans) && !display_text.empty() &&
            label_rect.width > 0.f) {
          Font font = font_manager.get_active_font();
          const std::string resolved =
              font_manager.resolve_weighted(cmp.font_name, cmp.font_weight);
          const float font_size = wants_wrap ? explicit_fs : result.rect.height;
          const float spacing = 1.f + hasLabel.letter_spacing;
          // A span's weight wins when it asks for one; otherwise the run
          // inherits the component's. So a styled label with no per-span
          // weights resolves exactly as it did before spans had weights.
          const auto name_for = [&](colors::FontWeight sw) {
            return font_manager.resolve_weighted(
                cmp.font_name,
                sw == colors::FontWeight::Regular ? cmp.font_weight : sw);
          };
          const auto weighted_width = [&](const std::string &s,
                                          colors::FontWeight sw) {
            return measure_text(font_manager.get_font(name_for(sw)), s.c_str(),
                                font_size, spacing)
                .x;
          };
          auto line_width = [&](const std::string &s) {
            return measure_text(font, s.c_str(), font_size, spacing).x;
          };
          // Unwrapped styled text uses the same loop, unbounded width.
          const float wrap_width =
              wants_wrap ? label_rect.width : 1e9f;

          if (wrap_width > 0.f) {
            const std::vector<TextSpan> runs =
                has_spans ? hasLabel.spans
                          : std::vector<TextSpan>{
                                TextSpan{display_text, font_col}};
            // Memoised, same as the immediate path.
            const std::uint64_t wrap_key =
                detail::wrap_memo::key_for(runs, wrap_width);
            const std::vector<detail::TextRunLine> *cached =
                detail::wrap_memo::lookup(wrap_key);
            const std::vector<detail::TextRunLine> &lines =
                cached ? *cached
                       : detail::wrap_memo::store(
                             wrap_key, detail::wrap_runs_to_width(
                                           runs, wrap_width, weighted_width));

            // A single colourless line is what the path below already draws.
            if (lines.size() > 1 || has_spans) {
              const float line_h =
                  measure_text(font, "Ag", font_size, spacing).y;
              const float total_h = line_h * static_cast<float>(lines.size());
              float y = label_rect.y +
                        std::max(0.f, (label_rect.height - total_h) * 0.5f);
              for (const auto &line : lines) {
                if (line.empty()) {
                  y += line_h; // blank line from a "\n\n"
                  continue;
                }
                if (line.size() == 1) {
                  // Same call shape as before, so plain wrapped text does
                  // not shift.
                  RectangleType lr{label_rect.x, y, label_rect.width, line_h};
                  buffer.add_text(lr, line[0].text, name_for(line[0].weight),
                                  font_size, line[0].color, hasLabel.alignment,
                                  layer, entity.id, stroke, shadow, rotation,
                                  centerX, centerY, hasLabel.letter_spacing);
                } else {
                  // Sum the runs at their own weights: a bold run is wider
                  // than the same characters regular, so measuring the joined
                  // line with one face would drift the alignment.
                  float line_w = 0.f;
                  for (const auto &run : line)
                    line_w += weighted_width(run.text, run.weight);
                  float x = label_rect.x;
                  if (hasLabel.alignment == TextAlignment::Center)
                    x += std::max(0.f, (label_rect.width - line_w) / 2.f);
                  else if (hasLabel.alignment == TextAlignment::Right)
                    x += std::max(0.f, label_rect.width - line_w);
                  for (const auto &run : line) {
                    const float w = weighted_width(run.text, run.weight);
                    RectangleType sr{x, y, w, line_h};
                    buffer.add_text(sr, run.text, name_for(run.weight),
                                    font_size, run.color, TextAlignment::Left,
                                    layer, entity.id, stroke, shadow, rotation,
                                    centerX, centerY, hasLabel.letter_spacing);
                    x += w;
                  }
                }
                y += line_h;
              }
              wrapped = true;
            }
          }
        }

        if (!wrapped) {
          buffer.add_text(
              label_rect, display_text,
              font_manager.resolve_weighted(cmp.font_name, cmp.font_weight),
              result.rect.height, font_col, hasLabel.alignment, layer, entity.id,
              stroke, shadow, rotation, centerX, centerY,
              hasLabel.letter_spacing);
        }

#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
        // Register text for E2E testing. Through the same clip rect the
        // scissor uses, or text scrolled out of a pane still reads as visible
        // and expect_text/expect_no_text both lie about it.
        if (testing::test_input::detail::test_mode) {
          RectangleType vis = draw_rect;
          auto [has_clip, clip] =
              detail::compute_intersected_clip_rect(entity);
          if (has_clip)
            vis = detail::intersect_rects(draw_rect, clip);
          testing::VisibleTextRegistry::instance().register_text_if_visible(
              hasLabel.label, vis.x, vis.y, vis.width, vis.height,
              context.screen_width, context.screen_height);
        }
#endif
      }
    }

    // Texture
    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      float scale = (float)texture.texture.height / draw_rect.height;
      Vector2Type size = {
          (float)texture.texture.width / scale,
          (float)texture.texture.height / scale,
      };
      Vector2Type location =
          position_texture(texture.texture, size, draw_rect, texture.alignment);
      RectangleType dest = {location.x, location.y, size.x, size.y};
      RectangleType src = {0.0f, 0.0f, (float)texture.texture.width,
                           (float)texture.texture.height};
      // Honor with_opacity like the HasImage branch below (and the direct
      // render path); previously HasTexture ignored it in the buffered pass.
      Color tex_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f)
        tex_col = colors::opacity_pct(tex_col, effective_opacity);
      buffer.add_image(dest, src, texture.texture, tex_col, layer, entity.id);
    } else if (entity.has<ui::HasImage>()) {
      const ui::HasImage &img = entity.get<ui::HasImage>();
      texture_manager::Rectangle src =
          img.source_rect.value_or(texture_manager::Rectangle{
              0.0f, 0.0f, (float)img.texture.width, (float)img.texture.height});

      float scale = src.height / draw_rect.height;
      Vector2Type size = {src.width / scale, src.height / scale};
      Vector2Type location =
          position_texture(img.texture, size, draw_rect, img.alignment);

      Color img_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        img_col = colors::opacity_pct(img_col, effective_opacity);
      }

      RectangleType dest = {location.x, location.y, size.x, size.y};
      buffer.add_image(dest, src, img.texture, img_col, layer, entity.id);
    }

    // Custom draw (on top): enqueued after all of the widget's own primitives.
    if (entity.has<HasOnDraw>() && entity.get<HasOnDraw>().fg) {
      buffer.add_custom(draw_rect, &entity.get<HasOnDraw>().fg, layer,
                        entity.id);
    }
  }

  void collect(RenderCommandBuffer &buffer, UIContext<InputAction> &context,
               FontManager &font_manager, Entity &entity, int layer) {
    if (detail::is_hidden_for_render(entity)) return;
    if (entity.has<HasDragPreview>()) {
      detail::draw_drag_preview(entity, [&](Entity &source) { collect(buffer, context, font_manager, source, layer); });
      return;
    }
    // Defensive check: entity must have UIComponent to be rendered
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.font_name != UIComponent::UNSET_FONT) {
      font_manager.set_active(
          font_manager.resolve_weighted(cmp.font_name, cmp.font_weight));
    }

    // Scroll views define their own viewport — don't clip them by ancestors.
    // Everything else (including HasClipChildren) gets clipped by the
    // intersection of ALL ancestor clip rects.
    bool needs_scissor = false;
    if (!entity.has<HasScrollView>()) {
      auto [has_clip, clip_rect] =
          detail::compute_intersected_clip_rect(entity);
      if (has_clip) {
        buffer.add_scissor_start(
            static_cast<int>(clip_rect.x), static_cast<int>(clip_rect.y),
            static_cast<int>(clip_rect.width),
            static_cast<int>(clip_rect.height), layer, entity.id);
        needs_scissor = true;
      }
    }

    // Update scroll view content size
    if (entity.has<HasScrollView>()) {
      detail::update_scroll_view_content_size(entity);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>() || entity.has<HasOnDraw>() ||
        entity.has<HasCircularProgressState>() || entity.has<HasScrollView>() ||
        context.visual_focus_id == entity.id) {
      collect_me(buffer, context, font_manager, entity, layer);
    }

    if (needs_scissor) {
      buffer.add_scissor_end(layer, entity.id);
    }
  }

  virtual void for_each_with_derived(Entity &, UIContext<InputAction> &context,
                                     FontManager &font_manager,
                                     float) override {
    // Reset arena for new frame
    Arena &arena = get_render_arena();
    arena.reset();

    // Same reason as RenderImm: the buffer is one frame, not the whole run.
    if (capture::enabled() && !capture::app_owns_frame())
      capture::clear();

    // Create command buffer
    RenderCommandBuffer buffer(arena);

    // Stable-sort by layer only. render_cmds are queued in document pre-order
    // (parent before child), which is the correct paint order within a layer.
    // Do NOT tiebreak by entity id: ids are recycled across screens and are
    // not monotonic with document order, so an id tiebreak lets an opaque
    // ancestor paint over its own lower-id descendants.
#if __WIN32
    // Bubble sort (mingw lacks std::ranges::sort); stable because it only
    // swaps on a strictly greater layer.
    for (size_t i = 0; i < context.render_cmds.size(); ++i) {
      for (size_t j = i + 1; j < context.render_cmds.size(); ++j) {
        if (context.render_cmds[i].layer > context.render_cmds[j].layer) {
          std::swap(context.render_cmds[i], context.render_cmds[j]);
        }
      }
    }
#else
    std::ranges::stable_sort(
        context.render_cmds,
        [](RenderInfo a, RenderInfo b) { return a.layer < b.layer; });
#endif

    // Collect all commands
    const auto focus_paint = detail::prepare_focus_paint(context);
    const auto scrollbar = [&](const RenderInfo &cmd, RectangleType rect,
                               ColorType color, bool has_clip, RectangleType clip) {
      if (has_clip)
        buffer.add_scissor_start(static_cast<int>(clip.x), static_cast<int>(clip.y),
                                 static_cast<int>(clip.width), static_cast<int>(clip.height),
                                 cmd.layer, cmd.id);
      buffer.add_rounded_rectangle(rect, color, 1.f, 6, std::bitset<4>().set(),
                                   cmd.layer, cmd.id);
      if (has_clip) buffer.add_scissor_end(cmd.layer, cmd.id);
    };
    size_t layer_begin = 0;
    int cursor_to_set = 0; // Default cursor
    for (size_t index = 0; index < context.render_cmds.size(); ++index) {
      const auto &cmd = context.render_cmds[index];
      if (cmd.layer != context.render_cmds[layer_begin].layer) {
        detail::draw_layer_scrollbars(context, layer_begin, index, scrollbar);
        layer_begin = index;
      }
      auto id = cmd.id;
      auto layer = cmd.layer;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(id);
      if (!opt_ent.valid())
        continue; // Skip stale entity IDs
      Entity &ent = opt_ent.asE();
      collect(buffer, context, font_manager, ent, layer);
      if (focus_paint && focus_paint->after_command == index)
        detail::collect_focus_paint(buffer, *focus_paint);
      if (context.is_hot(ent.id) && ent.has<HasCursor>()) {
        cursor_to_set = to_cursor_id(ent.get<HasCursor>().cursor);
      }
    }
    detail::draw_layer_scrollbars(context, layer_begin, context.render_cmds.size(), scrollbar);
    set_mouse_cursor(cursor_to_set);
    context.render_cmds.clear();

    if (imm::UIStylingDefaults::get().sort_draws_by_layer)
      buffer.sort();

    // Execute all commands with batching
    BatchedRenderer renderer;
    renderer.render(buffer, font_manager);
  }
};

} // namespace ui

} // namespace afterhours
