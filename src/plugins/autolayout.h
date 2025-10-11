#pragma once

#include "../bitwise.h"
#include "../developer.h"
#include "../ecs.h"
#include "../font_helper.h"
#include "window_manager.h"

namespace afterhours {
using Rectangle = RectangleType;
using vec2 = Vector2Type;

namespace ui {

enum struct Dim {
  None,
  Pixels,
  Text,
  Percent,
  Children,
  ScreenPercent,
};

inline std::ostream &operator<<(std::ostream &os, const Dim &dim) {
  switch (dim) {
  case Dim::None:
    os << "None";
    break;
  case Dim::Pixels:
    os << "Pixels";
    break;
  case Dim::Text:
    os << "Text";
    break;
  case Dim::Percent:
    os << "Percent";
    break;
  case Dim::Children:
    os << "Children";
    break;
  case Dim::ScreenPercent:
    os << "ScreenPercent";
    break;
  }
  return os;
}

struct Size {
  Dim dim = Dim::None;
  float value = -1;
  float strictness = 1.f;
};

inline std::ostream &operator<<(std::ostream &os, const Size &size) {
  os << "Size(dim: " << size.dim << ", value: " << size.value
     << ", strictness: " << size.strictness << ")";
  return os;
}

inline Size pixels(const float value, const float strictness = 1.f) {
  return ui::Size{
      .dim = ui::Dim::Pixels, .value = value, .strictness = strictness};
}

inline Size percent(const float value, const float strictness = 1.f) {
  if (value > 1.f) {
    log_warn("Value should be between 0 and 1");
  }
  return ui::Size{
      .dim = ui::Dim::Percent, .value = value, .strictness = strictness};
}

inline Size screen_pct(const float value, const float strictness = 0.9f) {
  if (value > 1.f) {
    log_warn("Value should be between 0 and 1");
  }
  return ui::Size{
      .dim = ui::Dim::ScreenPercent, .value = value, .strictness = strictness};
}

inline Size children(const float value = -1) {
  return ui::Size{.dim = ui::Dim::Children, .value = value};
}

inline Size h720(const float px) { return screen_pct(px / 720.f); }
inline Size w1280(const float px) { return screen_pct(px / 1280.f); }

enum struct Spacing {
  xs, // Extra small: 0.01f (7.2px at 720p)
  sm, // Small: 0.02f (14.4px at 720p)
  md, // Medium: 0.04f (28.8px at 720p)
  lg, // Large: 0.08f (57.6px at 720p)
  xl, // Extra large: 0.16f (115.2px at 720p)
};

inline Size spacing_to_size(Spacing spacing) {
  switch (spacing) {
  case Spacing::xs:
    return screen_pct(0.01f);
  case Spacing::sm:
    return screen_pct(0.02f);
  case Spacing::md:
    return screen_pct(0.04f);
  case Spacing::lg:
    return screen_pct(0.08f);
  case Spacing::xl:
    return screen_pct(0.16f);
  }
  return screen_pct(0.04f); // Default to medium
}

struct ComponentSize {
  Size x_axis;
  Size y_axis;

  bool is_default = false;
  ComponentSize(std::pair<Size, Size> pair)
      : x_axis(pair.first), y_axis(pair.second) {}
  ComponentSize(Size x, Size y) : x_axis(x), y_axis(y) {}
  ComponentSize(Size x, Size y, bool is_default_)
      : x_axis(x), y_axis(y), is_default(is_default_) {}
  ComponentSize(std::pair<Size, Size> pair, bool is_default_)
      : x_axis(pair.first), y_axis(pair.second), is_default(is_default_) {}

  auto _scale_x(float s) {
    if ((x_axis.dim == ui::Dim::Children && x_axis.value < 0) ||
        x_axis.dim == ui::Dim::Text || x_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               x_axis.dim);
    }
    x_axis.value *= s;
    return ComponentSize(*this);
  }
  auto _scale_y(float s) {
    if ((y_axis.dim == ui::Dim::Children && y_axis.value < 0) ||
        y_axis.dim == ui::Dim::Text || y_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               y_axis.dim);
    }
    y_axis.value *= s;
    return ComponentSize(*this);
  }
};

inline std::ostream &operator<<(std::ostream &os, const ComponentSize &cs) {
  os << "ComponentSize(x: " << cs.x_axis << ", y: " << cs.y_axis << ")";
  return os;
}

inline ComponentSize pixels_xy(float width, float height) {
  return {pixels(width), pixels(height)};
}

inline ComponentSize children_xy() {
  return {
      children(),
      children(),
  };
}

inline Size half_size(Size size) {

  switch (size.dim) {
  case Dim::Children:
  case Dim::Text:
  case Dim::None: {
    log_warn("half size not supported for dim {}", size.dim);
  } break;
  case Dim::ScreenPercent:
  case Dim::Percent:
  case Dim::Pixels:
    return Size{
        .dim = size.dim,
        .value = size.value / 2.f,
        .strictness = size.strictness,
    };
  }
  return size;
}

enum struct FlexDirection {
  None = 1 << 0,
  Row = 1 << 1,
  Column = 1 << 2,
};

enum struct Axis {
  X = 0,
  Y = 1,

  left = 2,
  top = 3,

  right = 4,
  bottom = 5,
};
inline std::ostream &operator<<(std::ostream &os, const Axis &axis) {
  switch (axis) {
  case Axis::X:
    os << "X-Axis";
    break;
  case Axis::Y:
    os << "Y-Axis";
    break;
  case Axis::left:
    os << "left";
    break;
  case Axis::right:
    os << "right";
    break;
  case Axis::top:
    os << "top";
    break;
  case Axis::bottom:
    os << "bottom";
    break;
  }
  return os;
}

struct Padding {
  Size top;
  Size left;
  Size bottom;
  Size right;
};

struct Margin {
  Size top;
  Size bottom;
  Size left;
  Size right;
};

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
  AxisArray<Size, 6> desired_padding;
  AxisArray<Size, 6> desired_margin;

  FlexDirection flex_direction = FlexDirection::Column;

  bool should_hide = false;
  bool was_rendered_to_screen = false;
  bool absolute = false;
  AxisArray<float> computed;
  AxisArray<float, 6> computed_margin;
  AxisArray<float, 6> computed_padd;
  AxisArray<float> computed_rel;

  EntityID parent = -1;
  std::vector<EntityID> children;

  std::string font_name = UNSET_FONT;
  float font_size = 50.f;

  auto &enable_font(const std::string &font_name_, float fs) {
    font_name = font_name_;
    font_size = fs;
    return *this;
  }

  Rectangle rect() const {
    return Rectangle{
        .x = computed_rel[Axis::X] + computed_margin[Axis::left],
        .y = computed_rel[Axis::Y] + computed_margin[Axis::top],
        .width = computed[Axis::X] - computed_margin[Axis::X],
        .height = computed[Axis::Y] - computed_margin[Axis::Y],
    };
  };

  Rectangle bounds() const {
    Rectangle rect_ = rect();
    return Rectangle{
        .x = rect_.x - computed_padd[Axis::left] - computed_margin[Axis::left],
        .y = rect_.y - computed_padd[Axis::top] - computed_margin[Axis::top],
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
  None = Left,
};

struct HasLabel : BaseComponent {
  TextAlignment alignment = TextAlignment::None;

  std::string label;
  std::string font_name = UIComponent::UNSET_FONT;
  bool is_disabled = false;

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

struct AutoLayout {

  window_manager::Resolution resolution;
  std::map<EntityID, RefEntity> mapping;

  AutoLayout(window_manager::Resolution rez = {},
             const std::map<EntityID, RefEntity> &mapping_ = {})
      : resolution(rez), mapping(mapping_) {}

  Entity &to_ent(EntityID id) {
    auto it = mapping.find(id);
    if (it == mapping.end()) {
      log_error("during autolayout, we looked for an entity with id {} but it "
                "wasnt in the mapping provided",
                id);
    }
    return it->second;
  }

  virtual UIComponent &to_cmp(EntityID id) {
    return to_ent(id).get<UIComponent>();
  }

  std::array<float, 4>
  for_each_spacing(UIComponent &widget,
                   const std::function<float(UIComponent &, Axis)> &cb) {
    std::array<float, 4> data;
    data[0] = cb(widget, Axis::top);
    data[1] = cb(widget, Axis::left);
    data[2] = cb(widget, Axis::right);
    data[3] = cb(widget, Axis::bottom);
    return data;
  }

  void write_each_spacing(UIComponent &widget,
                          UIComponent::AxisArray<float, 6> &computed,
                          const std::function<float(UIComponent &, Axis)> &cb) {
    auto [_top, _left, _right, _bottom] = for_each_spacing(widget, cb);
    computed[Axis::top] = _top;
    computed[Axis::left] = _left;
    computed[Axis::bottom] = _bottom;
    computed[Axis::right] = _right;

    computed[Axis::X] = _left + _right;
    computed[Axis::Y] = _top + _bottom;
  }

  using MeasureTextFn = std::function<Vector2Type(
      const std::string &, const std::string &, float, float)>;

  MeasureTextFn external_measure_text = nullptr;
  auto &set_measure_text_fn(const MeasureTextFn &fn) {
    external_measure_text = fn;
    return *this;
  }

  float get_text_size_for_axis(UIComponent &widget, Axis axis) {
    const std::string &font_name = widget.font_name;

    const Entity &ent = to_ent(widget.id);

    if (ent.is_missing<HasLabel>()) {
      log_warn("Trying to size a component by Text but component doesnt have "
               "any text attached (add HasLabel)");
      return 0;
    }
    const std::string &content = ent.get<HasLabel>().label;
    float font_size = widget.font_size;
    float spacing = 1.f;

    Vector2Type result;
    if (external_measure_text) {
      result = external_measure_text(font_name, content, font_size, spacing);
    } else {
      auto font_manager = EntityHelper::get_singleton_cmp<FontManager>();
      auto font = font_manager->get_font(font_name);
      result = measure_text(font, content.c_str(), font_size, spacing);
    }

    switch (axis) {
    case Axis::X:
      return result.x;
    case Axis::Y:
      return result.y;
    case Axis::left:
    case Axis::top:
    case Axis::right:
    case Axis::bottom:
      log_error("Text size not supported for axis {}", axis);
      break;
    }
    return 0.f;
  }

  float compute_margin_for_exp(UIComponent &widget, Axis axis) {
    float no_change = widget.computed_margin[axis];

    float screenValue = fetch_screen_value_(axis);
    const auto compute_ = [=](Size exp) {
      switch (exp.dim) {
      case Dim::Pixels:
        return exp.value;
      case Dim::Text:
        log_error("Margin by dimension text not supported");
      case Dim::ScreenPercent:
        return exp.value * screenValue;
      case Dim::Children:
        log_error("Margin by dimension children not supported");
      case Dim::Percent:
      case Dim::None:
        // This is not a standalone widget,
        // so just keep moving along
        return no_change;
      }
      return no_change;
    };

    auto margin = widget.desired_margin;
    switch (axis) {
    case Axis::X:
      return compute_(margin[Axis::left]) + compute_(margin[Axis::right]);
    case Axis::Y:
      return compute_(margin[Axis::top]) + compute_(margin[Axis::bottom]);
    case Axis::top:
    case Axis::left:
    case Axis::right:
    case Axis::bottom:
      return compute_(margin[axis]);
    }
    return no_change;
  }

  float fetch_screen_value_(Axis axis) {
    float screenValue = 0;
    switch (axis) {
    case Axis::X:
    case Axis::left:
    case Axis::right:
      screenValue = resolution.width;
      break;
    case Axis::Y:
    case Axis::top:
    case Axis::bottom:
      screenValue = resolution.height;
      break;
    }
    return screenValue;
  }

  float compute_padding_for_standalone_exp(UIComponent &widget, Axis axis) {
    float no_change = widget.computed_padd[axis];
    float screenValue = fetch_screen_value_(axis);

    const auto compute_ = [no_change, screenValue](const Size &exp) {
      switch (exp.dim) {
      case Dim::Pixels:
        return exp.value;
      case Dim::ScreenPercent:
        return exp.value * screenValue;
        //
      case Dim::Text:
        log_error("Padding by dimension text not supported");
        return no_change;
      case Dim::Children:
        log_error("Padding by children not supported");
        return no_change;
        // This is not a standalone widget,
        // so just keep moving along
      case Dim::Percent:
      case Dim::None:
        return no_change;
      }
      return no_change;
    };

    const auto &padd = widget.desired_padding;
    switch (axis) {
    case Axis::X:
      return compute_(padd[Axis::left]) + compute_(padd[Axis::right]);
    case Axis::Y:
      return compute_(padd[Axis::top]) + compute_(padd[Axis::bottom]);
    case Axis::top:
    case Axis::left:
    case Axis::right:
    case Axis::bottom:
      return compute_(padd[axis]);
    }
    return no_change;
  }

  float compute_size_for_standalone_exp(UIComponent &widget, Axis axis) {
    float screenValue = fetch_screen_value_(axis);
    const auto compute_ = [&](const Size &exp) {
      switch (exp.dim) {
      case Dim::Pixels:
        return exp.value;
      case Dim::ScreenPercent:
        return exp.value * screenValue;
      case Dim::Text:
        return get_text_size_for_axis(widget, axis);
      case Dim::Percent:
      case Dim::None:
      case Dim::Children:
        // This is not a standalone widget,
        // so just keep moving along
        return widget.computed[axis];
      }
      return widget.computed[axis];
    };
    return compute_(widget.desired[axis]);
  }

  void calculate_standalone(UIComponent &widget) {
    widget.computed[Axis::X] = compute_size_for_standalone_exp(widget, Axis::X);
    widget.computed[Axis::Y] = compute_size_for_standalone_exp(widget, Axis::Y);

    write_each_spacing(widget, widget.computed_padd,
                       [&](UIComponent &w, Axis a) {
                         return compute_padding_for_standalone_exp(w, a);
                       });

    write_each_spacing(
        widget, widget.computed_margin,
        [&](UIComponent &w, Axis a) { return compute_margin_for_exp(w, a); });

    for (EntityID child_id : widget.children) {
      calculate_standalone(this->to_cmp(child_id));
    }
  }

  float compute_size_for_parent_expectation(const UIComponent &widget,
                                            Axis axis) {
    const Size &exp = widget.desired[axis];

    if (widget.absolute && exp.dim == Dim::Percent) {
      VALIDATE(false, "Absolute widgets should not use Percent");
    }

    float no_change = widget.computed[axis];
    if (widget.parent == -1) {
      if (exp.dim == Dim::Percent) {
        log_error("Trying to compute percent expectation for {}, but never set "
                  "parent",
                  widget.id);
      }
      // root component
      return no_change;
    }

    UIComponent &parent = this->to_cmp(widget.parent);
    if (parent.computed[axis] == -1) {
      if (is_dimension_percent_based(widget.desired_padding, axis)) {
        log_error(
            "Trying to compute expectation for {}, but parent {} size hasnt "
            "been calculated yet",
            widget.id, widget.parent);
      }
      return no_change;
    }

    // We dont include padding because padding is
    // around the content, not inside
    float parent_size = (parent.computed[axis] - parent.computed_margin[axis]);

    switch (exp.dim) {
    case Dim::Percent:
      return exp.value * parent_size;
    case Dim::None:
    case Dim::Text:
    case Dim::ScreenPercent:
    case Dim::Children:
    case Dim::Pixels:
      return no_change;
    }
    return no_change;
  }

  float compute_padding_for_parent_exp(UIComponent &widget, Axis axis) {
    float no_change = widget.computed_padd[axis];

    if (widget.parent == -1) {
      if (widget.desired_padding[axis].dim == Dim::Percent) {
        log_error("Trying to compute padding percent expectation for {}, but "
                  "never set "
                  "parent",
                  widget.id);
      }
      // root component
      return no_change;
    }

    UIComponent &parent = this->to_cmp(widget.parent);
    if (parent.computed[axis] == -1) {
      if (is_dimension_percent_based(widget.desired_padding, axis)) {
        log_error("Trying to compute padding percent expectation for {}, but "
                  "parent {} size not calculated yet",
                  widget.id, widget.parent);
      }
      return no_change;
    }

    const auto parent_size = [&parent](Axis axis_in) -> float {
      return parent.computed[axis_in];
    };

    const auto compute_ = [=](const Size &exp, float parent_value) {
      switch (exp.dim) {
      case Dim::Children:
        log_error("Padding by children not supported");
      case Dim::Text:
        log_error("Padding by dimension text not supported");
      case Dim::Percent:
        return exp.value * parent_value;
      // already handled during standalone
      case Dim::ScreenPercent:
      case Dim::None:
      case Dim::Pixels:
        return no_change;
      }
      return no_change;
    };

    const auto &padd = widget.desired_padding;
    switch (axis) {
    case Axis::X:
      return compute_(padd[Axis::left], parent_size(Axis::X)) +
             compute_(padd[Axis::right], parent_size(Axis::X));
    case Axis::Y:
      return compute_(padd[Axis::top], parent_size(Axis::Y)) +
             compute_(padd[Axis::bottom], parent_size(Axis::Y));
    case Axis::left:
    case Axis::right:
      return compute_(padd[axis], parent_size(Axis::X));
    case Axis::top:
    case Axis::bottom:
      return compute_(padd[axis], parent_size(Axis::Y));
    }
    return no_change;
  }

  float compute_margin_for_parent_exp(UIComponent &widget, Axis axis) {

    const auto &margin = widget.desired_margin;
    float no_change = widget.computed_margin[axis];
    if (widget.parent == -1) {
      if (is_dimension_percent_based(widget.desired_padding, axis)) {
        log_error("Trying to compute margin percent expectation for {}, but "
                  "no parent",
                  widget.id);
      }
      return no_change;
    }

    UIComponent &parent = this->to_cmp(widget.parent);
    if (parent.computed[axis] == -1) {
      if (is_dimension_percent_based(widget.desired_padding, axis)) {
        log_error("Trying to compute margin percent expectation for {}, but "
                  "parent {} size not calculated yet",
                  widget.id, widget.parent);
      }
      return no_change;
    }

    // again ignore padding on purpose

    const auto parent_size = [&parent](Axis axis_in) -> float {
      return parent.computed[axis_in] - parent.computed_margin[axis_in];
    };

    const auto compute_ = [no_change](const Size &exp, float parent_size) {
      switch (exp.dim) {
      case Dim::Percent:
        return exp.value * parent_size;
        //
      case Dim::Children:
      case Dim::None:
      case Dim::Text:
      case Dim::ScreenPercent:
      case Dim::Pixels:
        return no_change;
      }
      return no_change;
    };

    switch (axis) {
    case Axis::X:
      return compute_(margin[Axis::left], parent_size(Axis::X)) +
             compute_(margin[Axis::right], parent_size(Axis::X));
    case Axis::Y:
      return compute_(margin[Axis::top], parent_size(Axis::Y)) +
             compute_(margin[Axis::bottom], parent_size(Axis::Y));
    case Axis::left:
    case Axis::right:
      return compute_(margin[axis], parent_size(Axis::X));
    case Axis::top:
    case Axis::bottom:
      return compute_(margin[axis], parent_size(Axis::Y));
    }
    log_error("computing margin for parent exp but got invalid axis {}", axis);
    return no_change;
  }

  void calculate_those_with_parents(UIComponent &widget) {
    widget.computed[Axis::X] =
        compute_size_for_parent_expectation(widget, Axis::X);
    widget.computed[Axis::Y] =
        compute_size_for_parent_expectation(widget, Axis::Y);

    write_each_spacing(widget, widget.computed_padd,
                       [&](UIComponent &w, Axis a) {
                         return compute_padding_for_parent_exp(w, a);
                       });

    write_each_spacing(widget, widget.computed_margin,
                       [&](UIComponent &w, Axis a) {
                         return compute_margin_for_parent_exp(w, a);
                       });

    for (EntityID child_id : widget.children) {
      calculate_those_with_parents(this->to_cmp(child_id));
    }
  }

  float _sum_children_axis_for_child_exp(UIComponent &widget, Axis axis) {
    float total_child_size = 0.f;
    for (EntityID entityID : widget.children) {
      const UIComponent &child = this->to_cmp(entityID);
      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;
      // Ignore anything that should be hidden
      if (child.should_hide)
        continue;

      float cs = child.computed[axis];
      if ( //
          child.desired[axis].dim == Dim::Percent &&
          widget.desired[axis].dim == Dim::Children
          //
      ) {
        log_error("Parents sized with mode 'children' cannot have "
                  "children sized with mode 'percent'. Failed when checking "
                  "children for {} axis {}",
                  widget.id, axis);
      }

      //  no computed value yet
      if (cs == -1) {
        // Instead of silently ignoring this, check the cases above
        log_error("expect that all children have been solved by now child {} "
                  "parent {}",
                  entityID, widget.id);
      }
      total_child_size += cs;
    }
    return total_child_size;
  }
  float _max_child_size(UIComponent &widget, Axis axis) {
    float max_child_size = 0.f;
    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);
      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;
      // Ignore anything that should be hidden
      if (child.should_hide)
        continue;

      float cs = child.computed[axis];
      if (cs == -1) {
        if (child.desired[axis].dim == Dim::Percent &&
            widget.desired[axis].dim == Dim::Children) {
          VALIDATE(false, "Parents sized with mode 'children' cannot have "
                          "children sized with mode 'percent'.");
        }
        // Instead of silently ignoring this, check the cases above
        VALIDATE(false, "expect that all children have been solved by now");
      }
      max_child_size = fmaxf(max_child_size, cs);
    }

    return max_child_size;
  }

  float compute_size_for_child_expectation(UIComponent &widget, Axis axis) {
    float no_change = widget.computed[axis];

    Size exp = widget.desired[axis];
    if (exp.dim != Dim::Children)
      return no_change;

    float existing_desire = exp.value;
    if (widget.children.empty()) {
      // Make sure we arent setting -1 in this case
      return std::max(0.f,
                      // if the component has no children, but the expected size
                      // was set, use that instead
                      // TODO does this need to be a setting? is this generally
                      // a decent choice
                      std::max(no_change, existing_desire));
    }

    float expectation = _sum_children_axis_for_child_exp(widget, axis);

    // the min non-flex side of the box, should be whatever the largest child is
    // for example, when flexing down, the box should be as wide as the widest
    // child
    if ((widget.flex_direction & FlexDirection::Column) && axis == Axis::X) {
      expectation = _max_child_size(widget, axis);
      expectation = std::max(expectation, existing_desire);
    }

    if ((widget.flex_direction & FlexDirection::Row) && axis == Axis::Y) {
      expectation = _max_child_size(widget, axis);
      expectation = std::max(expectation, existing_desire);
    }

    return expectation;
  }

  void calculate_those_with_children(UIComponent &widget) {
    // Note, we dont early return when empty, because
    // there is some min_height/width logic in the compute
    // size and so we need run those
    // (specifically this is for dropdown but anything with changing children
    // probably needs this)

    for (EntityID child : widget.children) {
      calculate_those_with_children(this->to_cmp(child));
    }

    auto size_x = compute_size_for_child_expectation(widget, Axis::X);
    auto size_y = compute_size_for_child_expectation(widget, Axis::Y);

    widget.computed[Axis::X] = size_x;
    widget.computed[Axis::Y] = size_y;
  }

  void tax_refund(UIComponent &widget, Axis axis, float error) {
    int num_eligible_children = 0;
    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);
      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;
      // Ignore anything that should be hidden
      if (child.should_hide)
        continue;

      Size exp = child.desired[axis];
      if (exp.strictness == 0.f) {
        num_eligible_children++;
      }
    }

    if (num_eligible_children == 0) {
      log_trace("I have all this money to return, but no one wants it :(");
      return;
    }

    float indiv_refund = error / num_eligible_children;
    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);

      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;
      // Ignore anything that should be hidden
      if (child.should_hide)
        continue;

      Size exp = child.desired[axis];
      if (exp.strictness == 0.f) {
        child.computed[axis] += abs(indiv_refund);
        log_trace("Just gave back, time for trickle down");
        tax_refund(child, axis, indiv_refund);
      }
      // TODO idk if we should do this for all non 1.f children?
      // if (exp.strictness == 1.f || child->ignore_size) {
      // continue;
      // }
    }
  }

  void solve_violations(UIComponent &widget) {
    // we dont care if its less than a pixel
    const float ACCEPTABLE_ERROR = 1.f;

    size_t num_children = 0;
    for (EntityID child : widget.children) {
      if (this->to_cmp(child).absolute)
        continue;
      // Ignore anything that should be hidden
      if (this->to_cmp(child).should_hide)
        continue;
      num_children++;
    }
    if (num_children == 0)
      return;

    // me -> left -> right

    const auto _total_child = [this, &widget](Axis axis) {
      float sum = 0.f;
      // TODO support flexing
      for (EntityID child : widget.children) {
        // Dont worry about any children that are absolutely positioned
        if (this->to_cmp(child).absolute)
          continue;
        // Ignore anything that should be hidden
        if (this->to_cmp(child).should_hide)
          continue;
        sum += this->to_cmp(child).computed[axis];
      }
      return sum;
    };

    const auto _solve_error_optional = [this, &widget](Axis axis,
                                                       float *error) {
      int num_optional_children = 0;
      for (EntityID child : widget.children) {
        // TODO Dont worry about absolute positioned children
        if (this->to_cmp(child).absolute)
          continue;
        // Ignore anything that should be hidden
        if (this->to_cmp(child).should_hide)
          continue;

        Size exp = this->to_cmp(child).desired[axis];
        if (exp.strictness == 0.f) {
          num_optional_children++;
        }
      }
      // were there any children who dont care about their size?
      if (num_optional_children != 0) {
        float approx_epc = *error / num_optional_children;
        for (EntityID child : widget.children) {
          UIComponent &child_cmp = this->to_cmp(child);
          // Dont worry about absolute positioned children
          if (child_cmp.absolute)
            continue;
          // Ignore anything that should be hidden
          if (this->to_cmp(child).should_hide)
            continue;

          Size exp = child_cmp.desired[axis];
          if (exp.strictness == 0.f) {
            float cur_size = child_cmp.computed[axis];
            child_cmp.computed[axis] = fmaxf(0, cur_size - approx_epc);
            if (cur_size > approx_epc)
              *error -= approx_epc;
          }
        }
      }
    };

    const auto fix_violating_children = [num_children, this,
                                         &widget](Axis axis, float error) {
      VALIDATE(num_children != 0, "Should never have zero children");

      size_t num_strict_children = 0;
      size_t num_ignorable_children = 0;

      for (EntityID child : widget.children) {
        Size exp = this->to_cmp(child).desired[axis];
        if (exp.strictness == 1.f) {
          num_strict_children++;
        }
        if (this->to_cmp(child).absolute) {
          num_ignorable_children++;
        }
        // Ignore anything that should be hidden
        if (this->to_cmp(child).should_hide) {
          num_ignorable_children++;
        }
      }

      // If there is any error left,
      // we have to take away from the allowed children;

      size_t num_resizeable_children =
          num_children - num_strict_children - num_ignorable_children;

      /* support flex flags
      // TODO we cannot enforce the assert below in the case of wrapping
      // because the positioning happens after error correction
      if (error > ACCEPTABLE_ERROR && num_resizeable_children == 0 &&
          //
          !((widget->flexflags & GrowFlags::Column) ||
            (widget->flexflags & GrowFlags::Row))
          //
      ) {
          widget->print_tree();
          log_warn("Error was {}", error);
          VALIDATE(
              num_resizeable_children > 0,
              "Cannot fit all children inside parent and unable to resize any of
      " "the children");
      }
      */

      float approx_epc =
          error / (1.f * std::max(1, (int)num_resizeable_children));
      for (EntityID child : widget.children) {
        UIComponent &child_cmp = this->to_cmp(child);
        Size exp = child_cmp.desired[axis];
        if (exp.strictness == 1.f || child_cmp.absolute ||
            child_cmp.should_hide) {
          continue;
        }
        float portion_of_error = (1.f - exp.strictness) * approx_epc;
        float cur_size = child_cmp.computed[axis];
        child_cmp.computed[axis] = fmaxf(0, cur_size - portion_of_error);
        // Reduce strictness every round
        // so that eventually we will get there
        exp.strictness = fmaxf(0.f, exp.strictness - 0.05f);
        child_cmp.desired[axis] = exp;
      }
    };

    const auto compute_error = [ACCEPTABLE_ERROR, _total_child,
                                _solve_error_optional, fix_violating_children,
                                &widget](Axis axis) -> float {
      float my_size = widget.computed[axis];
      float all_children = _total_child(axis);
      float error = all_children - my_size;
      log_trace("starting error {} {}", axis, error);

      int i_x = 0;
      while (error > ACCEPTABLE_ERROR) {
        _solve_error_optional(axis, &error);
        i_x++;

        fix_violating_children(axis, error);
        all_children = _total_child(axis);
        error = all_children - my_size;
        if (i_x > 10) {
          log_trace("Hit {} iteration limit trying to solve violations {}",
                    axis, error);
          break;
        }
      }
      return error;
    };

    float error_x = compute_error(Axis::X);
    if (error_x < 0) {
      tax_refund(widget, Axis::X, error_x);
    }

    float error_y = compute_error(Axis::Y);
    if (error_y < 0) {
      tax_refund(widget, Axis::Y, error_y);
    }

    // Solve for children
    for (EntityID child : widget.children) {
      solve_violations(this->to_cmp(child));
    }
  }

  void compute_relative_positions(UIComponent &widget) {
    if (widget.parent == -1) {
      // This already happens by default, but lets be explicit about it
      widget.computed_rel[Axis::X] = 0.f;
      widget.computed_rel[Axis::Y] = 0.f;
    }

    // Assuming we dont care about things smaller than 1 pixel
    widget.computed[Axis::X] = round(widget.computed[Axis::X]);
    widget.computed[Axis::Y] = round(widget.computed[Axis::Y]);

    float sx = widget.computed[Axis::X] + widget.computed_padd[Axis::X];
    float sy = widget.computed[Axis::Y] + widget.computed_padd[Axis::Y];

    // this is 0 instead of margin, because the margin will be added in
    // compute rect
    float offx = 0;
    float offy = 0;

    // Represents the current wrap's largest
    // ex. on Column mode we only care about where to start the next column
    float col_w = 0.f;
    float col_h = 0.f;

    const auto update_max_size = [&](float cx, float cy) {
      col_w = fmax(cx, col_w);
      col_h = fmax(cy, col_h);
    };

    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);

      // Dont worry about any children that are absolutely positioned
      if (child.absolute) {
        compute_relative_positions(child);
        continue;
      }

      // Ignore anything that should be hidden
      if (child.should_hide) {
        compute_relative_positions(child);
        continue;
      }

      // computed bounds
      float cx = child.computed[Axis::X] + child.computed_padd[Axis::X];
      float cy = child.computed[Axis::Y] + child.computed_padd[Axis::Y];

      bool will_hit_max_x = false;
      bool will_hit_max_y = false;

      will_hit_max_x = cx + offx > sx;
      will_hit_max_y = cy + offy > sy;

      bool no_flex = child.flex_direction == FlexDirection::None;
      // We cant flex and are going over the limit
      if (no_flex && (will_hit_max_x || will_hit_max_y)) {
        child.computed_rel[Axis::X] = sx;
        child.computed_rel[Axis::Y] = sy;
        continue;
      }

      // We can flex vertically and current child will push us over height
      // lets wrap
      if (child.flex_direction & FlexDirection::Column && cy + offy > sy) {
        offy = 0;
        offx += col_w;
        col_w = cx;
      }

      // We can flex horizontally and current child will push us over
      // width lets wrap
      if (child.flex_direction & FlexDirection::Row && cx + offx > sx) {
        offx = 0;
        offy += col_h;
        col_h = cy;
      }

      child.computed_rel[Axis::X] = offx;
      child.computed_rel[Axis::Y] = offy;

      // Setup for next child placement
      if (child.flex_direction & FlexDirection::Column) {
        offy += cy;
      }
      if (child.flex_direction & FlexDirection::Row) {
        offx += cx;
      }

      update_max_size(cx, cy);
      compute_relative_positions(child);
    }
  }

  void compute_rect_bounds(UIComponent &widget) {
    // log_trace("computing rect bounds for {}", widget);

    Vector2Type offset = Vector2Type{0.f, 0.f};
    if (widget.parent != -1) {
      UIComponent &parent = this->to_cmp(widget.parent);
      offset = Vector2Type{
          parent.computed_rel[Axis::X] + parent.computed_margin[Axis::left],
          parent.computed_rel[Axis::Y] + parent.computed_margin[Axis::top],
      };
      widget.computed_rel[Axis::X] += offset.x;
      widget.computed_rel[Axis::Y] += offset.y;
    }

    widget.computed_rel[Axis::X] += (widget.computed_padd[Axis::left]);
    widget.computed_rel[Axis::Y] += (widget.computed_padd[Axis::top]);

    for (EntityID child : widget.children) {
      compute_rect_bounds(this->to_cmp(child));
    }
  }

  void reset_computed_values(UIComponent &widget) {
    widget.reset_computed_values();
    for (EntityID child : widget.children) {
      reset_computed_values(this->to_cmp(child));
    }
  }

  static void autolayout(UIComponent &widget,
                         const window_manager::Resolution resolution,
                         const std::map<EntityID, RefEntity> &map) {
    AutoLayout al(resolution, map);

    al.reset_computed_values(widget);
    // - (any) compute solos (doesnt rely on parent/child / other widgets)
    al.calculate_standalone(widget);
    // - (pre) parent sizes
    al.calculate_those_with_parents(widget);
    // - (post) children
    al.calculate_those_with_children(widget);
    // - (pre) solve violations
    al.solve_violations(widget);
    // - (pre) compute relative positions
    al.compute_relative_positions(widget);
    // - (pre) compute rect bounds
    al.compute_rect_bounds(widget);
  }

  static Entity &to_ent_static(EntityID id) {
    return EntityQuery().whereID(id).gen_first_enforce();
  }

  static UIComponent &to_cmp_static(EntityID id) {
    return to_ent_static(id).get<UIComponent>();
  }
};

} // namespace ui

} // namespace afterhours
