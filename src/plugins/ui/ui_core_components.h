#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../../ecs.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../color.h"
#include "layout_types.h"
#include "theme.h"

namespace afterhours {

using Rectangle = RectangleType;
using vec2 = Vector2Type;

namespace ui {

struct AutoLayoutRoot : BaseComponent {};

struct UIComponent : BaseComponent {
  static constexpr std::string UNSET_FONT = "__unset";
  static constexpr std::string DEFAULT_FONT = "__default";
  static constexpr std::string SYMBOL_FONT = "__symbol";

  EntityID id;
  UIComponent() { init_values(); }
  explicit UIComponent(EntityID id_) : id(id_) { init_values(); }

  void init_values() {
    computed[Axis::X] = -1;
    computed[Axis::Y] = -1;

    computed_padd[Axis::X] = 0;
    computed_padd[Axis::Y] = 0;
    computed_padd[Axis::top] = 0;
    computed_padd[Axis::left] = 0;
    computed_padd[Axis::bottom] = 0;
    computed_padd[Axis::right] = 0;

    computed_margin[Axis::X] = 0;
    computed_margin[Axis::Y] = 0;
    computed_margin[Axis::top] = 0;
    computed_margin[Axis::left] = 0;
    computed_margin[Axis::bottom] = 0;
    computed_margin[Axis::right] = 0;
  }

  template <typename T, size_t axes = 2> struct AxisArray {
    std::array<T, axes> data;
    T &operator[](Axis axis) { return data[static_cast<size_t>(axis)]; }

    const T &operator[](Axis axis) const {
      return data[static_cast<size_t>(axis)];
    }

  private:
    friend std::ostream &operator<<(std::ostream &os,
                                    const AxisArray<T, axes> &p) {
      os << "AxisArray" << axes << ": ";
      for (auto d : p.data) {
        os << d << ", ";
      }
      return os;
    }
  };

  AxisArray<Size> desired;
  AxisArray<Size> min_size;  // Minimum size constraint (Dim::None = no constraint)
  AxisArray<Size> max_size;  // Maximum size constraint (Dim::None = no constraint)
  AxisArray<Size, 6> desired_padding;
  AxisArray<Size, 6> desired_margin;

  int render_layer = 0;

  FlexDirection flex_direction = FlexDirection::Column;
  JustifyContent justify_content = JustifyContent::FlexStart;
  AlignItems align_items = AlignItems::FlexStart;
  SelfAlign self_align = SelfAlign::Auto; // Override parent's align_items for this element
  FlexWrap flex_wrap = FlexWrap::Wrap;    // Controls wrapping behavior
  bool debug_wrap = false;                // Opt-in wrap debugging

  bool should_hide = false;
  bool was_rendered_to_screen = false;
  bool absolute = false;

  // Absolute position in pixels, set from with_absolute_position(x, y)
  // during component init. Used by autolayout to set computed_rel for
  // absolute elements so their children are positioned correctly.
  float absolute_pos_x = 0.f;
  float absolute_pos_y = 0.f;

  // Resolved scaling mode for this component (set during creation from the
  // cascade: component override > screen context > app default).
  ScalingMode resolved_scaling_mode = ScalingMode::Proportional;
  AxisArray<float> computed;
  AxisArray<float, 6> computed_margin;
  AxisArray<float, 6> computed_padd;
  AxisArray<float> computed_rel;

  EntityID parent = -1;
  std::vector<EntityID> children;

  std::string font_name = UNSET_FONT;
  Size font_size = pixels(50.f);
  bool font_size_explicitly_set = false;

  auto &enable_font(const std::string &font_name_, Size fs,
                     bool explicit_size = false) {
    font_name = font_name_;
    font_size = fs;
    font_size_explicitly_set = explicit_size;
    return *this;
  }

  // Float overload for backwards compatibility
  auto &enable_font(const std::string &font_name_, float fs) {
    return enable_font(font_name_, pixels(fs));
  }

  Rectangle rect() const {
    if (absolute) {
      // Absolute positioning: margins are position offsets only, don't shrink size
      return Rectangle{
          .x = computed_rel[Axis::X] + computed_margin[Axis::left],
          .y = computed_rel[Axis::Y] + computed_margin[Axis::top],
          .width = fmaxf(0.f, computed[Axis::X]),
          .height = fmaxf(0.f, computed[Axis::Y]),
      };
    }
    // Flow layout: margins reduce available space (standard CSS content-box)
    return Rectangle{
        .x = computed_rel[Axis::X] + computed_margin[Axis::left],
        .y = computed_rel[Axis::Y] + computed_margin[Axis::top],
        // Clamp to 0 to prevent negative dimensions from margin overflow
        .width = fmaxf(0.f, computed[Axis::X] - computed_margin[Axis::X]),
        .height = fmaxf(0.f, computed[Axis::Y] - computed_margin[Axis::Y]),
    };
  };

  Rectangle bounds() const {
    Rectangle rect_ = rect();
    // bounds() = the full allocation including padding (inside) and margins
    // (outside). Padding is internal to the element so we don't subtract it
    // from position; we only expand outward by margin.
    return Rectangle{
        .x = rect_.x - computed_margin[Axis::left],
        .y = rect_.y - computed_margin[Axis::top],
        .width =
            rect_.width + computed_padd[Axis::X] + computed_margin[Axis::X],
        .height =
            rect_.height + computed_padd[Axis::Y] + computed_margin[Axis::Y],
    };
  };

  float x() const { return rect().x; }
  float y() const { return rect().y; }
  float width() const { return rect().width; }
  float height() const { return rect().height; }

  Rectangle focus_rect(int rw = 4) const {
    return Rectangle{x() - (float)rw, y() - (float)rw,
                     width() + (2.f * (float)rw), height() + (2.f * (float)rw)};
  }

  auto &make_absolute() {
    this->absolute = true;
    return *this;
  }

  auto &add_child(EntityID id_) {
    if (id_ == id) {
      log_error("Adding child with id {} that matches our current id {}", id_,
                id);
    }
    children.push_back(id_);
    return *this;
  }

  auto &remove_child(EntityID id_) {
    children.erase(std::remove(children.begin(), children.end(), id_),
                   children.end());
    return *this;
  }

  auto &set_parent(EntityID id_) {
    parent = id_;
    return *this;
  }

  auto &set_parent(Entity &entity) {
    parent = entity.id;
    entity.get<UIComponent>().add_child(this->id);
    return *this;
  }

  auto &set_desired_width(Size s) {
    desired[Axis::X] = s;
    return *this;
  }

  auto &set_desired_height(Size s) {
    desired[Axis::Y] = s;
    return *this;
  }

  auto &set_min_width(Size s) {
    min_size[Axis::X] = s;
    return *this;
  }

  auto &set_max_width(Size s) {
    max_size[Axis::X] = s;
    return *this;
  }

  auto &set_min_height(Size s) {
    min_size[Axis::Y] = s;
    return *this;
  }

  auto &set_max_height(Size s) {
    max_size[Axis::Y] = s;
    return *this;
  }

  auto &set_desired_margin(Size s, Axis axis) {
    if (axis == Axis::X) {
      desired_margin[Axis::left] = s;
      desired_margin[Axis::right] = s;
      return *this;
    }
    if (axis == Axis::Y) {
      desired_margin[Axis::top] = s;
      desired_margin[Axis::bottom] = s;
      return *this;
    }
    desired_margin[axis] = s;
    return *this;
  }

  auto &set_desired_margin(Margin margin) {
    desired_margin[Axis::top] = margin.top;
    desired_margin[Axis::left] = margin.left;
    desired_margin[Axis::bottom] = margin.bottom;
    desired_margin[Axis::right] = margin.right;
    return *this;
  }

  auto &set_desired_padding(Size s, Axis axis) {
    if (axis == Axis::X) {
      // TODO do you think this should be 5 and 5 or 10 and 10?
      // .set_desired_padding(pixels(10.f), Axis::Y)
      //
      // desired_padding[Axis::left] = half_size(s);
      // desired_padding[Axis::right] = half_size(s);
      desired_padding[Axis::left] = s;
      desired_padding[Axis::right] = s;
      return *this;
    }
    if (axis == Axis::Y) {
      desired_padding[Axis::top] = s;
      desired_padding[Axis::bottom] = s;
      return *this;
    }
    desired_padding[axis] = s;
    return *this;
  }

  auto &set_desired_padding(Padding padding) {
    desired_padding[Axis::top] = padding.top;
    desired_padding[Axis::left] = padding.left;
    desired_padding[Axis::bottom] = padding.bottom;
    desired_padding[Axis::right] = padding.right;
    return *this;
  }

  auto &set_flex_direction(FlexDirection flex) {
    flex_direction = flex;
    return *this;
  }

  auto &set_justify_content(JustifyContent jc) {
    justify_content = jc;
    return *this;
  }

  auto &set_align_items(AlignItems ai) {
    align_items = ai;
    return *this;
  }

  auto &set_self_align(SelfAlign sa) {
    self_align = sa;
    return *this;
  }

  auto &set_flex_wrap(FlexWrap fw) {
    flex_wrap = fw;
    return *this;
  }

  auto &set_debug_wrap(bool enabled) {
    debug_wrap = enabled;
    return *this;
  }

  void reset_computed_values() {
    init_values();

    computed_rel[Axis::X] = 0.f;
    computed_rel[Axis::Y] = 0.f;
  }
};

struct FontManager : BaseComponent {
  std::string active_font = UIComponent::DEFAULT_FONT;
  std::map<std::string, Font> fonts;

  auto &load_font(const std::string &font_name, Font font) {
    fonts[font_name] = font;
    return *this;
  }

  auto &load_font(const std::string &font_name, const char *font_file) {
    fonts[font_name] = load_font_from_file(font_file);
    return *this;
  }

  // Add codepoint-based font loading for CJK support
  auto &load_font_with_codepoints(const std::string &font_name,
                                  const char *font_file, int *codepoints,
                                  int codepoint_count) {
    if (font_name.empty()) {
      log_error("Cannot load font with empty name");
      return *this;
    }

    if (!font_file) {
      log_error("Cannot load font '{}' with null file path", font_name.c_str());
      return *this;
    }

    if (!codepoints || codepoint_count <= 0) {
      log_error("Cannot load font '{}' with invalid codepoints",
                font_name.c_str());
      return *this;
    }

    fonts[font_name] = load_font_from_file_with_codepoints(
        font_file, codepoints, codepoint_count);
    return *this;
  }

  auto &set_active(const std::string &font_name) {
    if (!fonts.contains(font_name)) {
      log_warn("{} missing from font manager. Did you call load_font() on it "
               "previously?",
               font_name.c_str());
    }
    active_font = font_name;
    return *this;
  }

  Font get_active_font() const {
    if (!fonts.contains(active_font)) {
      log_warn("{} missing from font manager. Did you call load_font() on it "
               "previously?",
               active_font.c_str());
    }
    return fonts.at(active_font);
  }
  Font get_font(const std::string &name) const { return fonts.at(name); }
};

enum struct TextAlignment {
  Left,
  Center,
  Right,
  None,
};

struct HasLabel : BaseComponent {
  TextAlignment alignment = TextAlignment::None;

  std::string label;
  std::string font_name = UIComponent::UNSET_FONT;
  bool is_disabled = false;

  // For auto-contrast text color calculation
  // When set, the renderer will use colors::auto_text_color() to pick
  // the best text color for readability against this background.
  std::optional<Color> background_hint;

  // Explicit text color override (set via with_text_color())
  // When set, this color is used instead of theme font color or auto-contrast.
  std::optional<Color> explicit_text_color;

  // Text stroke/outline configuration
  // When set, renders text outline behind the main text for better visibility
  std::optional<TextStroke> text_stroke;

  // Text drop shadow configuration
  // When set, renders a shadow behind the text for depth/legibility
  std::optional<TextShadow> text_shadow;

  HasLabel(const std::string &str, bool is_disabled_ = false)
      : label(str), is_disabled(is_disabled_) {}
  HasLabel() : label(""), is_disabled(false) {}

  auto &set_alignment(TextAlignment align_) {
    alignment = align_;
    return *this;
  }

  auto &set_label(const std::string &str) {
    label = str;
    return *this;
  }

  auto &set_disabled(bool dis_) {
    is_disabled = dis_;
    return *this;
  }

  auto &set_background_hint(Color bg) {
    background_hint = bg;
    return *this;
  }

  auto &clear_background_hint() {
    background_hint = std::nullopt;
    return *this;
  }

  auto &set_explicit_text_color(Color color) {
    explicit_text_color = color;
    return *this;
  }

  auto &clear_explicit_text_color() {
    explicit_text_color = std::nullopt;
    return *this;
  }

  auto &set_text_stroke(const TextStroke &stroke) {
    text_stroke = stroke;
    return *this;
  }

  auto &set_text_stroke(Color color, float thickness = 2.0f) {
    text_stroke = TextStroke{color, thickness};
    return *this;
  }

  auto &clear_text_stroke() {
    text_stroke = std::nullopt;
    return *this;
  }

  bool has_text_stroke() const {
    return text_stroke.has_value() && text_stroke->has_stroke();
  }

  auto &set_text_shadow(const TextShadow &shadow) {
    text_shadow = shadow;
    return *this;
  }

  auto &set_text_shadow(Color color, float offset_x = 2.0f,
                        float offset_y = 2.0f) {
    text_shadow = TextShadow{color, offset_x, offset_y};
    return *this;
  }

  auto &clear_text_shadow() {
    text_shadow = std::nullopt;
    return *this;
  }

  bool has_text_shadow() const {
    return text_shadow.has_value() && text_shadow->has_shadow();
  }
};

inline bool
is_dimension_percent_based(const UIComponent::AxisArray<Size, 6> &desire,
                           Axis axis) {
  bool is_pct_dim = false;
  switch (axis) {
  case Axis::X:
    is_pct_dim = desire[Axis::left].dim == Dim::Percent ||
                 desire[Axis::right].dim == Dim::Percent;
    break;
  case Axis::Y:
    is_pct_dim = desire[Axis::top].dim == Dim::Percent ||
                 desire[Axis::bottom].dim == Dim::Percent;
    break;
  case Axis::top:
  case Axis::bottom:
  case Axis::right:
  case Axis::left:
    is_pct_dim = desire[axis].dim == Dim::Percent;
    break;
  }
  return is_pct_dim;
}

} // namespace ui

} // namespace afterhours
