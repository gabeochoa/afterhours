#pragma once

#include <cmath>

#include "../texture_manager.h"
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif
#include <algorithm>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../logging.h"
#include "layout_types.h"
#include "theme.h"

namespace afterhours {

namespace ui {

// Type-safe angle wrapper to prevent radians/degrees confusion
// Use degrees() helper to construct: degrees(-90.0f)
struct Degrees {
  float value = 0.0f;

  constexpr Degrees() = default;
  constexpr explicit Degrees(float v) : value(v) {}

  // Implicit conversion to float for compatibility with drawing functions
  constexpr operator float() const { return value; }

  // Common angle presets
  static constexpr Degrees top() { return Degrees(-90.0f); }
  static constexpr Degrees right() { return Degrees(0.0f); }
  static constexpr Degrees bottom() { return Degrees(90.0f); }
  static constexpr Degrees left() { return Degrees(180.0f); }
};

// Helper function for cleaner syntax: degrees(-90.0f)
constexpr Degrees degrees(float v) { return Degrees(v); }

struct UIComponentDebug : BaseComponent {
  enum struct Type {
    unknown,
    custom,
    auto_label, // auto-derived from label text, not explicitly set
  } type;

  std::string name_value;

  UIComponentDebug(Type type_) : type(type_) {}
  UIComponentDebug(const std::string &name_)
      : type(Type::custom), name_value(name_) {}

  void set(const std::string &name_) {
    if (name_ == "") {
      type = Type::unknown;
      return;
    }
    type = Type::custom;
    name_value = name_;
  }

  void set_from_label(const std::string &name_) {
    type = Type::auto_label;
    name_value = name_;
  }

  std::string name() const {
    if (type == UIComponentDebug::Type::custom ||
        type == UIComponentDebug::Type::auto_label) {
      return name_value;
    }
    return std::string(magic_enum::enum_name<UIComponentDebug::Type>(type));
  }
};

struct HasClickListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasClickListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasClickActivationMode : BaseComponent {
  ClickActivationMode mode = ClickActivationMode::Default;
  HasClickActivationMode() = default;
  explicit HasClickActivationMode(ClickActivationMode mode_) : mode(mode_) {}
};

// Invisible to hit-testing: never hot or active, the element behind is picked.
// Not with_skip_tabbing, which is keyboard focus order only.
struct IgnorePointerEvents : BaseComponent {};

struct HasDragListener : BaseComponent {
  bool down = false;
  std::function<void(Entity &)> cb;
  HasDragListener(const std::function<void(Entity &)> &callback)
      : cb(callback) {}
};

struct HasLeftRightListener : BaseComponent {
  std::function<void(Entity &, int)> cb;
  HasLeftRightListener(const std::function<void(Entity &, int)> &callback)
      : cb(callback) {}
};

struct HasCheckboxState : BaseComponent {
  bool changed_since = false;
  bool on;
  HasCheckboxState(bool b) : on(b) {}
};

struct HasSliderState : BaseComponent {
  bool changed_since = false;
  float value;
  HasSliderState(float val) : value(val) {}
};

// TODO: Consider unifying HasStepperState and HasDropdownState — a stepper is
// really just a dropdown variant where only the selected element is shown.
struct HasStepperState : BaseComponent {
  bool changed_since = false;
  size_t index;
  size_t num_options;
  HasStepperState(size_t idx, size_t count) : index(idx), num_options(count) {}
};

struct HasToggleSwitchState : BaseComponent {
  bool changed_since = false;
  bool on;
  float animation_progress = 0.0f; // 0.0 = off position, 1.0 = on position
  HasToggleSwitchState(bool b) : on(b), animation_progress(b ? 1.0f : 0.0f) {}
};

struct ShouldHide : BaseComponent {};
struct SkipWhenTabbing : BaseComponent {};
struct SelectOnFocus : BaseComponent {};

struct FocusClusterRoot : BaseComponent {};
struct InFocusCluster : BaseComponent {};

struct HasTray : BaseComponent {
  int selection_index = 0;
  std::vector<EntityID> navigable_children; // rebuilt each frame

  // Key repeat timing
  float repeat_timer = 0.f;
  float repeat_delay = 0.4f;     // seconds before repeat starts
  float repeat_interval = 0.15f; // seconds between repeats
  bool was_held = false;
};

struct HasChildrenComponent : BaseComponent {
  std::vector<EntityID> children;
  std::function<void(Entity &)> on_child_add;

  HasChildrenComponent() {}

  void add_child(Entity &child) {
    children.push_back(child.id);
    if (on_child_add)
      on_child_add(child);
  }

  auto &register_on_child_add(const std::function<void(Entity &)> &cb) {
    on_child_add = cb;
    return *this;
  }
};

struct HasDropdownState : ui::HasCheckboxState {
  using Options = std::vector<std::string>;
  Options options;
  std::function<Options(HasDropdownState &)> fetch_options = nullptr;
  std::function<void(size_t)> on_option_changed = nullptr;
  size_t last_option_clicked = 0;
  bool was_open_last_frame = false;

  HasDropdownState(
      const Options &opts,
      const std::function<Options(HasDropdownState &)> fetch_opts = nullptr,
      const std::function<void(size_t)> opt_changed = nullptr)
      : HasCheckboxState(false), options(opts), fetch_options(fetch_opts),
        on_option_changed(opt_changed) {}

  HasDropdownState(const std::function<Options(HasDropdownState &)> fetch_opts)
      : HasDropdownState(fetch_opts(*this), fetch_opts, nullptr) {}

  template <size_t N>
  HasDropdownState(
      const std::array<std::string_view, N> &opts,
      const std::function<Options(HasDropdownState &)> fetch_opts = nullptr,
      const std::function<void(size_t)> opt_changed = nullptr)
      : HasDropdownState(Options(opts.begin(), opts.end()), fetch_opts,
                         opt_changed) {}

  size_t current_index() const { return last_option_clicked; }
  void set_current_index(size_t index) { last_option_clicked = index; }
};

struct HasNavigationBarState : ui::HasDropdownState {
  HasNavigationBarState(const Options &opts,
                        const std::function<void(size_t)> opt_changed = nullptr)
      : HasDropdownState(opts, nullptr, opt_changed) {}

  template <size_t N>
  HasNavigationBarState(const std::array<std::string_view, N> &opts,
                        const std::function<void(size_t)> opt_changed = nullptr)
      : HasNavigationBarState(Options(opts.begin(), opts.end()), opt_changed) {}

  size_t current_index() const { return last_option_clicked; }
  void set_current_index(size_t index) { last_option_clicked = index; }
};

struct HasRoundedCorners : BaseComponent {
  std::bitset<4> rounded_corners = std::bitset<4>().reset();
  float roundness = 0.5f; // 0.0 = sharp, 1.0 = fully rounded
  // A radius in pixels, which wins over roundness when set. roundness is a
  // fraction of the widget's short side, so one theme value is 8px on a row
  // and 180px on a full-height panel; say px when you mean px.
  std::optional<float> radius_px;
  int segments = 8; // Number of segments per corner

  auto &set(std::bitset<4> input) {
    rounded_corners = input;
    return *this;
  }
  auto &set_roundness(float r) {
    roundness = r;
    return *this;
  }
  auto &set_radius_px(std::optional<float> px) {
    radius_px = px;
    return *this;
  }
  auto &set_segments(int s) {
    segments = s;
    return *this;
  }
  auto &get() const { return rounded_corners; }
};

/// The 0..1 short-side fraction both backends draw with. A pixel radius is
/// converted against `rect` and clamped, since a radius past half the short
/// side is already a pill.
inline float resolve_roundness(const std::optional<float> &radius_px,
                               float roundness, const RectangleType &rect) {
  if (!radius_px.has_value())
    return roundness;
  const float shorter = std::min(rect.width, rect.height);
  if (shorter <= 0.f)
    return 0.f;
  return std::clamp(2.f * radius_px.value() / shorter, 0.f, 1.f);
}

struct HasImage : BaseComponent {
  afterhours::texture_manager::Texture texture;
  std::optional<afterhours::texture_manager::Rectangle> source_rect;
  afterhours::texture_manager::HasTexture::Alignment alignment =
      afterhours::texture_manager::HasTexture::Alignment::Center;

  HasImage(
      afterhours::texture_manager::Texture tex,
      std::optional<afterhours::texture_manager::Rectangle> src = std::nullopt,
      afterhours::texture_manager::HasTexture::Alignment align =
          afterhours::texture_manager::HasTexture::Alignment::Center)
      : texture(tex), source_rect(src), alignment(align) {}
};

struct HasOpacity : BaseComponent {
  float value = 1.0f;
  HasOpacity() = default;
  explicit HasOpacity(float v) : value(v) {}
};

struct HasButtonAnimState : BaseComponent {
  bool hovered = false;
};

struct HasUIModifiers : BaseComponent {
  float scale = 1.0f;
  float translate_x = 0.f;
  float translate_y = 0.f;
  float rotation = 0.f; // Rotation in degrees

  RectangleType apply_modifier(RectangleType rect) const {
    // Apply scale first
    float s = scale;
    if (s != 1.0f) {
      float cx = rect.x + rect.width / 2.0f;
      float cy = rect.y + rect.height / 2.0f;
      float new_w = rect.width * s;
      float new_h = rect.height * s;
      rect.x = cx - new_w / 2.0f;
      rect.y = cy - new_h / 2.0f;
      rect.width = new_w;
      rect.height = new_h;
    }
    // Apply translate (only once!)
    if (translate_x != 0.f || translate_y != 0.f) {
      rect.x += translate_x;
      rect.y += translate_y;
    }
    return rect;
  }
};

// Shadow styles for UI elements
enum struct ShadowStyle {
  Hard, // Sharp offset shadow (retro/flat design)
  Soft  // Blurred/layered shadow (modern/soft design)
};

// Shadow configuration (plain struct like Margin/Padding)
struct Shadow {
  ShadowStyle style = ShadowStyle::Soft;
  float offset_x = 4.0f;
  float offset_y = 4.0f;
  float blur_radius = 8.0f;
  Color color = Color{0, 0, 0, 80};

  static Shadow hard(float ox = 4.0f, float oy = 4.0f,
                     Color c = Color{0, 0, 0, 120}) {
    return Shadow{ShadowStyle::Hard, ox, oy, 0.0f, c};
  }

  static Shadow soft(float ox = 4.0f, float oy = 6.0f, float blur = 12.0f,
                     Color c = Color{0, 0, 0, 60}) {
    return Shadow{ShadowStyle::Soft, ox, oy, blur, c};
  }
};

// Component for entities that have shadows
struct HasShadow : BaseComponent {
  Shadow shadow;
  HasShadow() = default;
  explicit HasShadow(const Shadow &s) : shadow(s) {}
};

// Border line style
enum struct BorderStyle { Solid, Dotted };

// Per-side border configuration
struct BorderSide {
  Color color = Color{0, 0, 0, 0};
  Size thickness = pixels(0.0f);
  BorderStyle style = BorderStyle::Solid;
  bool has_border() const { return thickness.value > 0.0f && color.a > 0; }
};

// Border configuration for UI elements
// Supports uniform or per-side borders.
struct Border {
  BorderSide top, right, bottom, left;

  // Uniform border factory (backwards compatible)
  static Border all(Color color, Size thickness,
                    BorderStyle style = BorderStyle::Solid) {
    BorderSide s{color, thickness, style};
    return {s, s, s, s};
  }

  bool has_border() const {
    return top.has_border() || right.has_border() || bottom.has_border() ||
           left.has_border();
  }

  bool is_uniform() const {
    auto colors_equal = [](const Color &a, const Color &b) {
      return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    return colors_equal(top.color, right.color) &&
           colors_equal(right.color, bottom.color) &&
           colors_equal(bottom.color, left.color) &&
           top.thickness.value == right.thickness.value &&
           right.thickness.value == bottom.thickness.value &&
           bottom.thickness.value == left.thickness.value;
  }

  // Convenience accessors for uniform border (returns top side values)
  Color uniform_color() const { return top.color; }
  Size uniform_thickness() const { return top.thickness; }

  // Legacy accessors for code that assumes uniform borders
  Color color_compat() const { return top.color; }
  Size thickness_compat() const { return top.thickness; }
};

// Component for entities that have borders
struct HasBorder : BaseComponent {
  Border border;
  HasBorder() = default;
  explicit HasBorder(const Border &b) : border(b) {}
};

// Cursor types for hover behavior
enum struct CursorType { Default, Pointer, Text, ResizeH, ResizeV };

// Component for entities that change cursor on hover
struct HasCursor : BaseComponent {
  CursorType cursor = CursorType::Default;
  explicit HasCursor(CursorType c = CursorType::Default) : cursor(c) {}
};

// Bevel styles for classic raised/sunken borders
enum class BevelStyle { None, Raised, Sunken };

// Bevel border configuration
struct BevelBorder {
  Color light_color = Color{255, 255, 255, 255};
  Color dark_color = Color{128, 128, 128, 255};
  float thickness = 1.0f;
  BevelStyle style = BevelStyle::Raised;

  bool has_bevel() const {
    return thickness > 0.0f && style != BevelStyle::None;
  }
};

// Component for entities that have bevel borders
struct HasBevelBorder : BaseComponent {
  BevelBorder bevel;
  HasBevelBorder() = default;
  explicit HasBevelBorder(const BevelBorder &b) : bevel(b) {}
};

// Nine-slice border configuration
// Renders a texture as a 9-slice border that scales properly
struct NineSliceBorder {
  texture_manager::Texture texture;
  int left = 16;   // Source texture slice width (left edge)
  int top = 16;    // Source texture slice height (top edge)
  int right = 16;  // Source texture slice width (right edge)
  int bottom = 16; // Source texture slice height (bottom edge)
  Color tint = Color{255, 255, 255, 255};

  // Convenience constructor with uniform slice size
  static NineSliceBorder uniform(texture_manager::Texture tex, int slice_size,
                                 Color tint_color = Color{255, 255, 255, 255}) {
    return NineSliceBorder{tex,        slice_size, slice_size,
                           slice_size, slice_size, tint_color};
  }

  // Convenience constructor with custom slice sizes
  static NineSliceBorder custom(texture_manager::Texture tex, int left_,
                                int top_, int right_, int bottom_,
                                Color tint_color = Color{255, 255, 255, 255}) {
    return NineSliceBorder{tex, left_, top_, right_, bottom_, tint_color};
  }
};

// Component for entities that have 9-slice borders
struct HasNineSliceBorder : BaseComponent {
  NineSliceBorder nine_slice;
  HasNineSliceBorder() = default;
  explicit HasNineSliceBorder(const NineSliceBorder &n) : nine_slice(n) {}
};

// Circular progress indicator state
// Stores value (0-1) and visual configuration
struct HasCircularProgressState : BaseComponent {
  float value = 0.0f;                            // Progress value 0.0 to 1.0
  float thickness = 8.0f;                        // Ring thickness in pixels
  Degrees start_angle = Degrees::top();          // Start angle (top = -90°)
  Color track_color = Color{128, 128, 128, 100}; // Background track color
  Color fill_color = Color{100, 200, 100, 255};  // Progress fill color

  HasCircularProgressState() = default;
  explicit HasCircularProgressState(float val, float thick = 8.0f)
      : value(val), thickness(thick) {}

  HasCircularProgressState &set_value(float v) {
    value = std::clamp(v, 0.0f, 1.0f);
    return *this;
  }
  HasCircularProgressState &set_thickness(float t) {
    thickness = t;
    return *this;
  }
  HasCircularProgressState &set_start_angle(Degrees angle) {
    start_angle = angle;
    return *this;
  }
  HasCircularProgressState &set_track_color(Color c) {
    track_color = c;
    return *this;
  }
  HasCircularProgressState &set_fill_color(Color c) {
    fill_color = c;
    return *this;
  }
};

// Overflow behavior for a UI box on a given axis.
//   Visible — children can overflow; no clipping (default).
//   Hidden  — children are clipped to the box bounds.
//   Scroll  — children are clipped and the user can scroll.
//   Auto    — like Scroll, but only clips/scrolls when content exceeds
//   viewport.
//             When content fits, behaves like Visible (no clipping, no scroll).
enum class Overflow { Visible, Hidden, Scroll, Auto };

// Scroll view state - enables scrolling content within a clipped viewport
struct HasScrollView : BaseComponent {
  Vector2Type scroll_offset = {0, 0}; // Current (rendered) scroll position
  Vector2Type scroll_target = {0, 0}; // Desired scroll position (wheel writes
                                      // here; scroll_offset eases toward it for
                                      // smooth, momentum-like scrolling)
  Vector2Type content_size = {0, 0};  // Total size of all children (computed)
  // Extent the content has beyond its actual children, for a virtualized list
  // whose un-built tail still has to count toward the scroll range.
  Vector2Type content_extra = {0, 0};
  // Unset until a layout pass has measured it. A plain {0,0} could not be told
  // apart from a view genuinely measured as empty, so a consumer windowing its
  // content read zero on frame one and quietly built everything.
  std::optional<Vector2Type> viewport_size;
  float scroll_speed = 20.0f;         // Pixels per scroll wheel notch
  // Fraction of the remaining distance covered per 60fps frame. 1.0 (default)
  // snaps, matching the behaviour before smoothing existed; ~0.25 glides.
  float scroll_smoothing = 1.0f;
  // What ease_scroll last wrote, so a caller assigning scroll_offset directly
  // can be told apart from the easing's own output.
  Vector2Type last_eased_offset = {0, 0};
  bool vertical_enabled = true;       // Allow vertical scrolling
  bool horizontal_enabled = false;    // Allow horizontal scrolling
  bool invert_scroll = false;         // Invert scroll direction (non-natural)
  bool auto_overflow =
      false; // Auto mode: only clip/scroll when content overflows
  // Non-zero: views sharing an id scroll together, on enabled axes only.
  size_t sync_group = 0;
  Vector2Type last_synced = {0, 0}; // what the sync last wrote

  HasScrollView() = default;
  explicit HasScrollView(float speed) : scroll_speed(speed) {}
  HasScrollView(bool vert, bool horiz)
      : vertical_enabled(vert), horizontal_enabled(horiz) {}

  // Measured size, or zero while unmeasured. Callers that must tell the two
  // apart should read viewport_size directly.
  Vector2Type viewport_or_zero() const {
    return viewport_size.value_or(Vector2Type{0, 0});
  }

  // Clamp scroll offset AND target to valid bounds (0 to max scrollable).
  void clamp_scroll() {
    // An unmeasured view has no bounds to clamp against; clamping to a zero
    // viewport would pin the offset to the top before the first layout.
    if (!viewport_size.has_value())
      return;
    float max_scroll_y = std::max(0.0f, content_size.y - viewport_size->y);
    scroll_offset.y = std::clamp(scroll_offset.y, 0.0f, max_scroll_y);
    scroll_target.y = std::clamp(scroll_target.y, 0.0f, max_scroll_y);
    // Horizontal scrolling (not enabled in MVP but structure is here)
    float max_scroll_x = std::max(0.0f, content_size.x - viewport_size->x);
    scroll_offset.x = std::clamp(scroll_offset.x, 0.0f, max_scroll_x);
    scroll_target.x = std::clamp(scroll_target.x, 0.0f, max_scroll_x);
    // Clamping is our own edit, not a caller's, so do not let the next ease
    // mistake it for one and drag the target back out of bounds.
    last_eased_offset = scroll_offset;
  }

  // Ease the rendered offset toward the target, once per frame.
  void ease_scroll(float dt) {
    // A caller that assigns scroll_offset directly (scroll-to-top, scrollbar
    // drag) is authoritative: bring the target along instead of reverting it.
    if (scroll_offset.x != last_eased_offset.x ||
        scroll_offset.y != last_eased_offset.y)
      scroll_target = scroll_offset;

    if (scroll_smoothing >= 1.0f || scroll_smoothing <= 0.0f) {
      scroll_offset = scroll_target;
      last_eased_offset = scroll_offset;
      return;
    }

    // Convert the per-60fps-frame fraction to elapsed time, so the glide takes
    // the same wall-clock time on a 120Hz display as on a 60Hz one.
    const float alpha =
        1.0f - std::pow(1.0f - scroll_smoothing, std::max(dt, 0.0f) * 60.0f);
    auto step = [alpha](float cur, float tgt) {
      const float d = tgt - cur;
      if (d > -0.5f && d < 0.5f)
        return tgt; // settle exactly, no perpetual sub-pixel redraw
      return cur + d * alpha;
    };
    scroll_offset.y = step(scroll_offset.y, scroll_target.y);
    scroll_offset.x = step(scroll_offset.x, scroll_target.x);
    last_eased_offset = scroll_offset;
  }

  // Check if content exceeds viewport (scrolling needed)
  bool needs_scroll_y() const { return content_size.y > viewport_or_zero().y; }
  bool needs_scroll_x() const { return content_size.x > viewport_or_zero().x; }

  // Off for a view that supplies its own bar, or where one would be noise.
  bool show_scrollbar = true;
  // Set while the thumb is held. The drag owns scroll_offset until released.
  bool dragging_scrollbar = false;
  bool drag_is_vertical = true;
  float drag_grab_offset = 0.f; // cursor's distance from the thumb's near edge
  // Sizes, not raw px: a 6px bar authored at 720p is half as thick, relative
  // to everything around it, on a 1440p window. h720 scales with the screen.
  Size scrollbar_thickness = h720(6.f);
  Size scrollbar_min_thumb = h720(24.f); // grabbable even on a 10k-row list
};

// Where a scroll view's bar goes; empty when it needs none. Pure, so it is
// testable without a renderer and both render paths read the same answer.
struct ScrollbarGeometry {
  bool visible = false;
  RectangleType track{};
  RectangleType thumb{};
};

/// Thickness and minimum thumb, resolved to pixels. Both the draw and the drag
/// need these, and a scrollbar you cannot grab where you can see it is exactly
/// what two copies of this arithmetic would produce.
struct ScrollbarMetrics {
  float thickness = 0.f;
  float min_thumb = 0.f;
};
inline ScrollbarMetrics scrollbar_metrics(const HasScrollView &scroll,
                                          ScalingMode mode, float screen_h,
                                          float ui_scale = 1.f) {
  return {resolve_to_pixels(scroll.scrollbar_thickness, screen_h, mode, ui_scale),
          resolve_to_pixels(scroll.scrollbar_min_thumb, screen_h, mode, ui_scale)};
}

/// `view` is the scroll view's on-screen rect, `vertical` picks the axis, and
/// the two sizes arrive already resolved to pixels -- the caller has the screen
/// dimensions, and keeping them out of here keeps this a pure function.
inline ScrollbarGeometry scrollbar_geometry(const HasScrollView &scroll,
                                            const RectangleType &view,
                                            bool vertical, float thickness_px,
                                            float min_thumb_px) {
  ScrollbarGeometry g;
  const bool needed = vertical ? scroll.needs_scroll_y() : scroll.needs_scroll_x();
  if (!scroll.show_scrollbar || !needed)
    return g;

  const float thickness = std::max(1.f, thickness_px);
  const float extent = vertical ? view.height : view.width;
  const float content = vertical ? scroll.content_size.y : scroll.content_size.x;
  const float offset = vertical ? scroll.scroll_offset.y : scroll.scroll_offset.x;
  if (extent <= 0.f || content <= 0.f)
    return g;

  // Floored to stay grabbable, which is why travel below is (extent - len) and
  // not the plain ratio -- the ratio parks it short of the end on long content.
  const float len =
      std::min(extent, std::max(min_thumb_px, extent * (extent / content)));
  const float max_scroll = std::max(0.f, content - extent);
  const float progress = max_scroll > 0.f ? std::clamp(offset / max_scroll, 0.f, 1.f) : 0.f;
  const float travel = std::max(0.f, extent - len);

  g.visible = true;
  if (vertical) {
    g.track = {view.x + view.width - thickness, view.y, thickness, view.height};
    g.thumb = {g.track.x, view.y + progress * travel, thickness, len};
  } else {
    g.track = {view.x, view.y + view.height - thickness, view.width, thickness};
    g.thumb = {view.x + progress * travel, g.track.y, len, thickness};
  }
  return g;
}

// Marker component that enables scissor clipping for children
// Unlike HasScrollView, this only clips without scroll functionality
struct HasClipChildren : BaseComponent {};

// Tag IDs for UI overlay input exclusivity.
enum class UITag : TagId {
  InputExclusivity =
      40, // Entity's tree receives exclusive input, blocking all
          // other entities. Used by overlay components (dropdowns,
          // popovers, etc.) that should consume clicks without
          // passing them through to elements behind.
};

// Tag IDs for drag-and-drop entity roles.
// These are set/cleared by the HandleDragGroups systems so that tagged
// entities can be discovered via queries instead of storing EntityIDs.
enum class DragTag : TagId {
  // TODO build more confidence around how to set these number to avoid
  // conflicts
  // Right now since UI elements are in their own collection, its not an issue
  // as they wont
  // conflict with Userspace tags, but something to keep in mind
  Group = 50,  // Marker tag attached by drag_group() to its div entity
  Spacer,      // The gap-filling spacer entity
  Overlay,     // The floating visual following the cursor
  DraggedItem, // The child being dragged
  SourceGroup, // The drag_group the item was picked from
  HoverGroup,  // The drag_group currently under the cursor
};

// Singleton component tracking drag-and-drop state across drag_group instances.
// TODO: Consider adding named drag groups and accept-list filtering so the
// engine can prevent drops visually (no spacer in disallowed targets).
// Options:
//   - Event could carry a user-defined type/tag so screens can switch on it
//     instead of comparing EntityIDs.
//   - with_drag_group_id("shop") + with_drag_accept_from({"shop","loot"})
//     would let the pre-layout system skip spacer insertion for disallowed
//     targets, giving proper visual feedback without screen-side workarounds.
struct DragGroupState : BaseComponent {
  struct Event {
    EntityID source_group;
    int source_index;
    EntityID target_group;
    int target_index;
  };

  bool dragging = false;
  int drag_source_index = -1;
  int hover_index = -1;

  // Original size of dragged item (for spacer + overlay)
  float dragged_width = 0;
  float dragged_height = 0;

  // Completed events for the screen to consume
  std::vector<Event> events;

  void reset_drag() {
    dragging = false;
    drag_source_index = -1;
    hover_index = -1;
    dragged_width = 0;
    dragged_height = 0;
  }
};

} // namespace ui

} // namespace afterhours
