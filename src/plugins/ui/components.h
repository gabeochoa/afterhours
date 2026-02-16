#pragma once

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

  std::string name() const {
    if (type == UIComponentDebug::Type::custom) {
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
  HasStepperState(size_t idx, size_t count)
      : index(idx), num_options(count) {}
};

struct HasToggleSwitchState : BaseComponent {
  bool changed_since = false;
  bool on;
  float animation_progress = 0.0f;  // 0.0 = off position, 1.0 = on position
  HasToggleSwitchState(bool b) : on(b), animation_progress(b ? 1.0f : 0.0f) {}
};

struct ShouldHide : BaseComponent {};
struct SkipWhenTabbing : BaseComponent {};
struct SelectOnFocus : BaseComponent {};

struct FocusClusterRoot : BaseComponent {};
struct InFocusCluster : BaseComponent {};

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
  int segments = 8;       // Number of segments per corner

  auto &set(std::bitset<4> input) {
    rounded_corners = input;
    return *this;
  }
  auto &set_roundness(float r) {
    roundness = r;
    return *this;
  }
  auto &set_segments(int s) {
    segments = s;
    return *this;
  }
  auto &get() const { return rounded_corners; }
};

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
  float rotation = 0.f;  // Rotation in degrees

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

// Per-side border configuration
struct BorderSide {
  Color color = Color{0, 0, 0, 0};
  Size thickness = pixels(0.0f);
  bool has_border() const { return thickness.value > 0.0f && color.a > 0; }
};

// Border configuration for UI elements
// Supports uniform or per-side borders.
struct Border {
  BorderSide top, right, bottom, left;

  // Uniform border factory (backwards compatible)
  static Border all(Color color, Size thickness) {
    BorderSide s{color, thickness};
    return {s, s, s, s};
  }

  bool has_border() const {
    return top.has_border() || right.has_border() ||
           bottom.has_border() || left.has_border();
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
enum class Overflow { Visible, Hidden, Scroll };

// Scroll view state - enables scrolling content within a clipped viewport
struct HasScrollView : BaseComponent {
  Vector2Type scroll_offset = {0, 0};  // Current scroll position
  Vector2Type content_size = {0, 0};   // Total size of all children (computed)
  Vector2Type viewport_size = {0, 0};  // Visible area size
  float scroll_speed = 20.0f;          // Pixels per scroll wheel notch
  bool vertical_enabled = true;        // Allow vertical scrolling
  bool horizontal_enabled = false;     // Allow horizontal scrolling
  bool invert_scroll = false;          // Invert scroll direction (non-natural)

  HasScrollView() = default;
  explicit HasScrollView(float speed) : scroll_speed(speed) {}
  HasScrollView(bool vert, bool horiz)
      : vertical_enabled(vert), horizontal_enabled(horiz) {}

  // Clamp scroll offset to valid bounds (0 to max scrollable distance)
  void clamp_scroll() {
    float max_scroll_y = std::max(0.0f, content_size.y - viewport_size.y);
    scroll_offset.y = std::clamp(scroll_offset.y, 0.0f, max_scroll_y);
    // Horizontal scrolling (not enabled in MVP but structure is here)
    float max_scroll_x = std::max(0.0f, content_size.x - viewport_size.x);
    scroll_offset.x = std::clamp(scroll_offset.x, 0.0f, max_scroll_x);
  }

  // Check if content exceeds viewport (scrolling needed)
  bool needs_scroll_y() const { return content_size.y > viewport_size.y; }
  bool needs_scroll_x() const { return content_size.x > viewport_size.x; }
};

// Marker component that enables scissor clipping for children
// Unlike HasScrollView, this only clips without scroll functionality
struct HasClipChildren : BaseComponent {};

// Tag IDs for drag-and-drop entity roles.
// These are set/cleared by the HandleDragGroups systems so that tagged
// entities can be discovered via queries instead of storing EntityIDs.
enum class DragTag : TagId {
  // TODO build more confidence around how to set these number to avoid conflicts
  // Right now since UI elements are in their own collection, its not an issue as they wont 
  // conflict with Userspace tags, but something to keep in mind 
  Group = 50,      // Marker tag attached by drag_group() to its div entity
  Spacer,          // The gap-filling spacer entity
  Overlay,         // The floating visual following the cursor
  DraggedItem,     // The child being dragged
  SourceGroup,     // The drag_group the item was picked from
  HoverGroup,      // The drag_group currently under the cursor
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
