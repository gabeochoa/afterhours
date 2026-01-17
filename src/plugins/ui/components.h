#pragma once

#include "../texture_manager.h"
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../logging.h"
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

// Border configuration for UI elements
struct Border {
  Color color = Color{0, 0, 0, 0}; // Transparent = no border
  float thickness = 2.0f;

  bool has_border() const { return thickness > 0.0f && color.a > 0; }
};

// Component for entities that have borders
struct HasBorder : BaseComponent {
  Border border;
  HasBorder() = default;
  explicit HasBorder(const Border &b) : border(b) {}
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
    return NineSliceBorder{tex, slice_size, slice_size, slice_size, slice_size,
                           tint_color};
  }

  // Convenience constructor with custom slice sizes
  static NineSliceBorder custom(texture_manager::Texture tex, int left_, int top_,
                                int right_, int bottom_,
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

// Concept for pluggable text storage backends (e.g., gap buffer, rope)
// Allows custom implementations for large text editing (word processors, etc.)
template <typename T>
concept TextStorage = requires(T storage, const T const_storage, size_t pos,
                               const std::string &str) {
  // Get text content for display
  { const_storage.str() } -> std::convertible_to<std::string>;
  // Get size in bytes
  { const_storage.size() } -> std::convertible_to<size_t>;
  // Insert string at position
  { storage.insert(pos, str) } -> std::same_as<void>;
  // Erase n bytes starting at position
  { storage.erase(pos, size_t{}) } -> std::same_as<void>;
  // Clear all content
  { storage.clear() } -> std::same_as<void>;
};

// Default std::string-based storage that satisfies TextStorage concept
struct StringStorage {
  std::string data;

  StringStorage() = default;
  explicit StringStorage(const std::string &s) : data(s) {}

  std::string str() const { return data; }
  size_t size() const { return data.size(); }
  void insert(size_t pos, const std::string &s) { data.insert(pos, s); }
  void erase(size_t pos, size_t len) { data.erase(pos, len); }
  void clear() { data.clear(); }
};

// Text input state - templated on storage backend
// Use HasTextInputState for default std::string storage
// Use HasTextInputStateT<YourStorage> for custom backends (gap buffer, rope)
template <TextStorage Storage = StringStorage>
struct HasTextInputStateT : BaseComponent {
  Storage storage;
  size_t cursor_position = 0; // Byte position in UTF-8 string
  bool changed_since = false;
  bool is_focused = false;
  size_t max_length = 256; // Maximum text length in bytes (0 = unlimited)
  float cursor_blink_timer = 0.f;  // Current timer value
  float cursor_blink_rate = 0.53f; // Seconds per half-cycle (configurable)

  HasTextInputStateT() = default;
  explicit HasTextInputStateT(const std::string &initial_text,
                              size_t max_len = 256, float blink_rate = 0.53f)
      : storage(initial_text), cursor_position(initial_text.size()),
        max_length(max_len), cursor_blink_rate(blink_rate) {}

  // Convenience accessors
  std::string text() const { return storage.str(); }
  size_t text_size() const { return storage.size(); }
};

// Default alias for simple std::string-based text input
using HasTextInputState = HasTextInputStateT<StringStorage>;

// Concept for any text input state (used for abbreviated function template
// syntax)
template <typename T>
concept AnyTextInputState = requires(T t, const T ct) {
  { t.storage };
  { t.cursor_position } -> std::convertible_to<size_t>;
  { t.changed_since } -> std::convertible_to<bool>;
  { t.max_length } -> std::convertible_to<size_t>;
  { t.cursor_blink_timer } -> std::convertible_to<float>;
  { t.cursor_blink_rate } -> std::convertible_to<float>;
  { ct.text() } -> std::convertible_to<std::string>;
  { ct.text_size() } -> std::convertible_to<size_t>;
};

// Listener for text input events (character typing)
struct HasTextInputListener : BaseComponent {
  std::function<void(Entity &, const std::string &)> on_change;
  std::function<void(Entity &)> on_submit; // Called on Enter key

  HasTextInputListener(
      std::function<void(Entity &, const std::string &)> change_cb = nullptr,
      std::function<void(Entity &)> submit_cb = nullptr)
      : on_change(std::move(change_cb)), on_submit(std::move(submit_cb)) {}
};

// Circular progress indicator state
// Stores value (0-1) and visual configuration
struct HasCircularProgressState : BaseComponent {
  float value = 0.0f;                        // Progress value 0.0 to 1.0
  float thickness = 8.0f;                    // Ring thickness in pixels
  Degrees start_angle = Degrees::top();      // Start angle (top = -90°)
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

} // namespace ui

} // namespace afterhours
