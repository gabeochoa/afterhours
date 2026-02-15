#pragma once

// Re-export layout types and core components for backwards compatibility
#include "ui/components.h"
#include "ui/layout_types.h"
#include "ui/ui_collection.h"
#include "ui/ui_core_components.h"

#include "../bitwise.h"
#include "../core/text_cache.h"
#include "../developer.h"
#include "window_manager.h"

namespace afterhours {

namespace ui {

struct AutoLayout {
  window_manager::Resolution resolution;
  std::map<EntityID, RefEntity> mapping;
  bool enable_grid_snapping = false;
  float ui_scale = 1.0f;

  AutoLayout(window_manager::Resolution rez = {},
             const std::map<EntityID, RefEntity> &mapping_ = {})
      : resolution(rez), mapping(mapping_) {}

  /// Resolve a pixel value respecting the component's scaling mode.
  /// In Adaptive mode, pixels are multiplied by ui_scale.
  /// In Proportional mode (default), pixels are returned as-is.
  float resolve_pixels(float value, const UIComponent &widget) const {
    if (widget.resolved_scaling_mode == ScalingMode::Adaptive) {
      return value * ui_scale;
    }
    return value;
  }

  auto &set_grid_snapping(bool enabled) {
    enable_grid_snapping = enabled;
    return *this;
  }

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
    // Resolve font_size to pixels using screen height
    float screen_height = fetch_screen_value_(Axis::Y);
    float font_size = resolve_to_pixels(widget.font_size, screen_height,
                                         widget.resolved_scaling_mode,
                                         ui_scale);
    float spacing = 1.f;

    Vector2Type result;
    if (external_measure_text) {
      result = external_measure_text(font_name, content, font_size, spacing);
    } else if (auto *text_cache =
                   EntityHelper::get_singleton_cmp<ui::TextMeasureCache>()) {
      result = text_cache->measure(content, font_name, font_size, spacing);
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
    const auto compute_ = [&](Size exp) {
      switch (exp.dim) {
      case Dim::Pixels:
        return resolve_pixels(exp.value, widget);
      case Dim::Text:
        log_error("Margin by dimension text not supported");
      case Dim::ScreenPercent:
        return exp.value * screenValue;
      case Dim::Children:
        log_error("Margin by dimension children not supported");
      case Dim::Percent:
      case Dim::None:
      case Dim::Expand:
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

  float snap_to_8pt_grid(float value, Axis axis) {
    constexpr float GRID_UNIT_720P = 4.0f;
    float screen_value = fetch_screen_value_(axis);
    float grid_unit = GRID_UNIT_720P * (screen_value / 720.0f);
    return std::round(value / grid_unit) * grid_unit;
  }

  float compute_padding_for_standalone_exp(UIComponent &widget, Axis axis) {
    float no_change = widget.computed_padd[axis];
    float screenValue = fetch_screen_value_(axis);

    const auto compute_ = [&](const Size &exp) {
      switch (exp.dim) {
      case Dim::Pixels:
        return resolve_pixels(exp.value, widget);
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
      case Dim::Expand:
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
        return resolve_pixels(exp.value, widget);
      case Dim::ScreenPercent:
        return exp.value * screenValue;
      case Dim::Text:
        return get_text_size_for_axis(widget, axis);
      case Dim::Percent:
      case Dim::None:
      case Dim::Children:
      case Dim::Expand:
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
        log_error("Trying to compute percent expectation for {}, but never "
                  "set "
                  "parent",
                  widget.id);
      }
      // root component
      return no_change;
    }

    UIComponent &parent = this->to_cmp(widget.parent);
    if (parent.computed[axis] == -1) {
      if (is_dimension_percent_based(widget.desired_padding, axis)) {
        log_error("Trying to compute expectation for {}, but parent {} size "
                  "hasnt "
                  "been calculated yet",
                  widget.id, widget.parent);
      }
      return no_change;
    }

    // Content-box model: percent sizing is relative to parent's content area
    // (after subtracting margin and padding). This matches CSS behavior where
    // percent(1.f) fills the interior space, not the full box.
    float parent_size = (parent.computed[axis] - parent.computed_margin[axis] -
                         parent.computed_padd[axis]);

    switch (exp.dim) {
    case Dim::Percent:
      return exp.value * parent_size;
    case Dim::Expand:
      // Expand children are sized later in distribute_expand_space()
      // after all other children have been sized
      return 0.f;
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
      case Dim::Expand:
        log_error("Padding by expand not supported");
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
      case Dim::Expand:
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

    // Include padding so that computed[axis] for Children-sized widgets
    // consistently represents the full box (content + padding), matching
    // the semantics of Pixels/ScreenPercent/Percent sizing modes. This
    // lets solve_violations and compute_relative_positions uniformly use
    // (computed - padding) as the content area for all sizing modes.
    float pad = widget.computed_padd[axis];

    float existing_desire = exp.value;
    if (widget.children.empty()) {
      // For leaf elements with a text label (e.g. buttons using children()
      // sizing), fall back to text measurement so the element is sized to
      // fit its label content rather than collapsing to padding-only.
      float text_size = 0.f;
      const Entity &ent = to_ent(widget.id);
      if (!ent.is_missing<HasLabel>()) {
        text_size = get_text_size_for_axis(widget, axis);
      }

      // Make sure we arent setting -1 in this case
      return std::max(0.f,
                      // if the component has no children, but the
                      // expected size was set, use that instead
                      // TODO does this need to be a setting? is this
                      // generally a decent choice
                      std::max({no_change, existing_desire, text_size})) + pad;
    }

    float expectation = _sum_children_axis_for_child_exp(widget, axis);

    // the min non-flex side of the box, should be whatever the largest
    // child is for example, when flexing down, the box should be as wide as
    // the widest child
    if ((widget.flex_direction & FlexDirection::Column) && axis == Axis::X) {
      expectation = _max_child_size(widget, axis);
      expectation = std::max(expectation, existing_desire);
    }

    if ((widget.flex_direction & FlexDirection::Row) && axis == Axis::Y) {
      expectation = _max_child_size(widget, axis);
      expectation = std::max(expectation, existing_desire);
    }

    return expectation + pad;
  }

  void calculate_those_with_children(UIComponent &widget) {
    // Note, we dont early return when empty, because
    // there is some min_height/width logic in the compute
    // size and so we need run those
    // (specifically this is for dropdown but anything with changing
    // children probably needs this)

    for (EntityID child : widget.children) {
      calculate_those_with_children(this->to_cmp(child));
    }

    auto size_x = compute_size_for_child_expectation(widget, Axis::X);
    auto size_y = compute_size_for_child_expectation(widget, Axis::Y);

    widget.computed[Axis::X] = size_x;
    widget.computed[Axis::Y] = size_y;
  }

  /// Resolve a constraint Size to pixels for min/max application.
  /// Returns -1 if the constraint is not applicable (Dim::None).
  float resolve_constraint(UIComponent &widget, Size constraint, Axis axis) {
    switch (constraint.dim) {
    case Dim::None:
      return -1.f;  // No constraint
    case Dim::Pixels:
      return resolve_pixels(constraint.value, widget);
    case Dim::ScreenPercent:
      return constraint.value * (axis == Axis::X ? resolution.width : resolution.height);
    case Dim::Percent: {
      // Percent of parent's content area
      if (widget.parent == -1)
        return -1.f;
      UIComponent &parent = this->to_cmp(widget.parent);
      float parent_size = parent.computed[axis] - parent.computed_margin[axis] -
                          parent.computed_padd[axis];
      return constraint.value * parent_size;
    }
    case Dim::Children:
    case Dim::Text:
    case Dim::Expand:
      // These don't make sense as min/max constraints
      return -1.f;
    }
    return -1.f;
  }

  /// Apply min/max constraints to a widget's computed size.
  void apply_size_constraints(UIComponent &widget) {
    for (Axis axis : {Axis::X, Axis::Y}) {
      float min_val = resolve_constraint(widget, widget.min_size[axis], axis);
      float max_val = resolve_constraint(widget, widget.max_size[axis], axis);

      if (min_val >= 0.f && widget.computed[axis] < min_val) {
        widget.computed[axis] = min_val;
      }
      if (max_val >= 0.f && widget.computed[axis] > max_val) {
        widget.computed[axis] = max_val;
      }
    }
  }

  void tax_refund(UIComponent &widget, Axis axis, float error) {
    // Build cached list of layout children (non-absolute, non-hidden) once
    std::vector<UIComponent *> layout_children;
    layout_children.reserve(widget.children.size());
    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);
      // Dont worry about any children that are absolutely positioned
      // Ignore anything that should be hidden
      if (child.absolute || child.should_hide)
        continue;
      layout_children.push_back(&child);
    }

    // First, check for Expand children and distribute to them
    float total_expand_weight = 0.f;
    for (UIComponent *child : layout_children) {
      if (child->desired[axis].dim == Dim::Expand) {
        total_expand_weight += child->desired[axis].value;
      }
    }

    if (total_expand_weight > 0.f) {
      // Distribute remaining space to Expand children proportionally
      float available_space = std::abs(error);
      for (UIComponent *child : layout_children) {
        if (child->desired[axis].dim == Dim::Expand) {
          float weight = child->desired[axis].value;
          float share = available_space * (weight / total_expand_weight);
          child->computed[axis] = share;
        }
      }
      // All extra space was distributed to Expand children
      return;
    }

    // No Expand children - fall back to original strictness=0 distribution
    int num_eligible_children = 0;
    for (UIComponent *child : layout_children) {
      if (child->desired[axis].strictness == 0.f) {
        num_eligible_children++;
      }
    }

    if (num_eligible_children == 0) {
      log_trace("I have all this money to return, but no one wants it :(");
      return;
    }

    float indiv_refund = error / num_eligible_children;
    for (UIComponent *child : layout_children) {
      if (child->desired[axis].strictness == 0.f) {
        child->computed[axis] += abs(indiv_refund);
        log_trace("Just gave back, time for trickle down");
        tax_refund(*child, axis, indiv_refund);
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

    // Build cached list of layout children (non-absolute, non-hidden) once
    // to avoid repeated to_cmp lookups and predicate checks
    std::vector<UIComponent *> layout_children;
    layout_children.reserve(widget.children.size());
    for (EntityID child_id : widget.children) {
      UIComponent &child_cmp = this->to_cmp(child_id);
      // Dont worry about any children that are absolutely positioned
      // Ignore anything that should be hidden
      if (child_cmp.absolute || child_cmp.should_hide)
        continue;
      layout_children.push_back(&child_cmp);
    }

    const size_t num_children = layout_children.size();
    if (num_children == 0)
      return;

    // me -> left -> right

    const auto _total_child = [&layout_children](Axis axis) {
      float sum = 0.f;
      for (UIComponent *child : layout_children) {
        sum += child->computed[axis];
      }
      return sum;
    };

    const auto _max_child = [&layout_children](Axis axis) {
      float max_val = 0.f;
      for (UIComponent *child : layout_children) {
        max_val = fmaxf(max_val, child->computed[axis]);
      }
      return max_val;
    };

    // Determine main vs cross axis for flex layout.
    // Main axis: children stack (sizes sum). Cross axis: children overlap
    // (only the largest matters). Using sum for the cross axis would
    // incorrectly shrink all children to fit a budget N times too small.
    bool is_col =
        static_cast<bool>(widget.flex_direction & FlexDirection::Column);
    bool is_rw =
        static_cast<bool>(widget.flex_direction & FlexDirection::Row);

    const auto _solve_error_optional = [&layout_children](Axis axis,
                                                          float *error) {
      int num_optional_children = 0;
      for (UIComponent *child : layout_children) {
        if (child->desired[axis].strictness == 0.f) {
          num_optional_children++;
        }
      }
      // were there any children who dont care about their size?
      if (num_optional_children != 0) {
        float approx_epc = *error / num_optional_children;
        for (UIComponent *child : layout_children) {
          Size exp = child->desired[axis];
          if (exp.strictness == 0.f) {
            float cur_size = child->computed[axis];
            child->computed[axis] = fmaxf(0, cur_size - approx_epc);
            if (cur_size > approx_epc)
              *error -= approx_epc;
          }
        }
      }
    };

    const auto fix_violating_children = [num_children,
                                         &layout_children](Axis axis,
                                                           float error) {
      VALIDATE(num_children != 0, "Should never have zero children");

      size_t num_strict_children = 0;
      for (UIComponent *child : layout_children) {
        if (child->desired[axis].strictness == 1.f) {
          num_strict_children++;
        }
      }

      // If there is any error left,
      // we have to take away from the allowed children;
      // Note: num_ignorable_children is 0 since layout_children already
      // excludes absolute and should_hide

      size_t num_resizeable_children = num_children - num_strict_children;

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
              "Cannot fit all children inside parent and unable to resize
      any of " "the children");
      }
      */

      float approx_epc =
          error / (1.f * std::max(1, (int)num_resizeable_children));
      for (UIComponent *child : layout_children) {
        Size exp = child->desired[axis];
        if (exp.strictness == 1.f) {
          continue;
        }
        float portion_of_error = (1.f - exp.strictness) * approx_epc;
        float cur_size = child->computed[axis];
        child->computed[axis] = fmaxf(0, cur_size - portion_of_error);
        // Reduce strictness every round
        // so that eventually we will get there
        exp.strictness = fmaxf(0.f, exp.strictness - 0.05f);
        child->desired[axis] = exp;
      }
    };

    const auto compute_error = [ACCEPTABLE_ERROR, &_total_child, &_max_child,
                                &_solve_error_optional, &fix_violating_children,
                                &widget](Axis axis, bool is_main_axis) -> float {
      // Use content area (computed minus padding) as the available space
      // for children. Padding reserves visual inset space and should not
      // be available for child layout. This matches compute_rect_bounds
      // which offsets children by padding_left/top.
      float my_size = fmaxf(0.f, widget.computed[axis] - widget.computed_padd[axis]);
      // Main axis: children stack, so their sizes sum up.
      // Cross axis: children overlap, so only the largest matters.
      float all_children = is_main_axis ? _total_child(axis) : _max_child(axis);
      float error = all_children - my_size;
      log_trace("starting error {} {}", axis, error);

      // Only run error correction on the main axis. Cross-axis children
      // overlap so shrinking them to fit a "sum" budget is incorrect.
      if (!is_main_axis) {
        return error;
      }

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

    // Main axis: Row→X, Column→Y. Cross axis uses max (overlap).
    float error_x = compute_error(Axis::X, is_rw);
    if (error_x < 0) {
      tax_refund(widget, Axis::X, error_x);
    }

    float error_y = compute_error(Axis::Y, is_col);
    if (error_y < 0) {
      tax_refund(widget, Axis::Y, error_y);
    }

    // Apply min/max constraints after size computation and error distribution
    apply_size_constraints(widget);

    // Solve for children - apply constraints first, then recurse
    for (UIComponent *child : layout_children) {
      apply_size_constraints(*child);
      solve_violations(*child);
    }
  }

  void compute_relative_positions(UIComponent &widget) {
    if (widget.parent == -1) {
      // This already happens by default, but lets be explicit about it
      widget.computed_rel[Axis::X] = 0.f;
      widget.computed_rel[Axis::Y] = 0.f;
    }

    if (enable_grid_snapping) {
      widget.computed[Axis::X] =
          snap_to_8pt_grid(widget.computed[Axis::X], Axis::X);
      widget.computed[Axis::Y] =
          snap_to_8pt_grid(widget.computed[Axis::Y], Axis::Y);
    }

    // Content area for child layout: subtract padding from the full box.
    // Children are later offset by padding_left/top in compute_rect_bounds,
    // so the layout space available to children is the content area only.
    // Clamp to 0 to handle cases where padding exceeds the widget size.
    float container_w = fmaxf(0.f, widget.computed[Axis::X] - widget.computed_padd[Axis::X]);
    float container_h = fmaxf(0.f, widget.computed[Axis::Y] - widget.computed_padd[Axis::Y]);

    // Wrap boundary = content area (children should wrap before exceeding it)
    float sx = container_w;
    float sy = container_h;

    // Grid snapping tolerance: when grid snapping is enabled, each child's
    // position is snapped to the grid, which can accumulate rounding errors
    // across multiple children. We use one grid unit as tolerance for the
    // actual wrap logic (prevents single-snap-induced wrapping), and a
    // larger accumulated tolerance for warning checks (proportional to
    // child count) for children()-sized containers.
    float grid_snap_tolerance_x = 0.f;
    float grid_snap_tolerance_y = 0.f;
    if (enable_grid_snapping) {
      constexpr float GST_GRID_UNIT_720P = 4.0f;
      grid_snap_tolerance_x =
          GST_GRID_UNIT_720P * (fetch_screen_value_(Axis::X) / 720.0f);
      grid_snap_tolerance_y =
          GST_GRID_UNIT_720P * (fetch_screen_value_(Axis::Y) / 720.0f);
    }

    // Determine layout direction
    bool is_column =
        static_cast<bool>(widget.flex_direction & FlexDirection::Column);
    bool is_row = static_cast<bool>(widget.flex_direction & FlexDirection::Row);

    // Count layout children and calculate total size along main axis
    size_t num_layout_children = 0;
    float total_main_size = 0.f;

    for (EntityID child_id : widget.children) {
      UIComponent &child = this->to_cmp(child_id);
      if (child.absolute || child.should_hide)
        continue;

      num_layout_children++;

      // Use the child's full size (computed + margin) for layout
      float cx = child.computed[Axis::X] + child.computed_margin[Axis::X];
      float cy = child.computed[Axis::Y] + child.computed_margin[Axis::Y];

      if (is_column) {
        total_main_size += cy;
      } else if (is_row) {
        total_main_size += cx;
      }
    }

    // Calculate justify_content parameters
    float main_axis_size = is_column ? container_h : container_w;
    float cross_axis_size = is_column ? container_w : container_h;
    float remaining_space = main_axis_size - total_main_size;

    float start_offset = 0.f;
    float gap = 0.f;

    if (remaining_space > 0.f && num_layout_children > 0) {
      switch (widget.justify_content) {
      case JustifyContent::FlexStart:
        break;
      case JustifyContent::FlexEnd:
        start_offset = remaining_space;
        break;
      case JustifyContent::Center:
        start_offset = remaining_space / 2.f;
        break;
      case JustifyContent::SpaceBetween:
        if (num_layout_children > 1) {
          gap = remaining_space / static_cast<float>(num_layout_children - 1);
        }
        break;
      case JustifyContent::SpaceAround:
        gap = remaining_space / static_cast<float>(num_layout_children);
        start_offset = gap / 2.f;
        break;
      }
    }

    // Accumulated grid snap tolerance for warning checks:
    // Each child's position snap can introduce up to half a grid unit of
    // rounding error. For children()-sized containers, this accumulates
    // across all children, so we scale the warning tolerance accordingly.
    float accumulated_snap_tolerance_x = grid_snap_tolerance_x;
    float accumulated_snap_tolerance_y = grid_snap_tolerance_y;
    if (enable_grid_snapping && num_layout_children > 2) {
      if (widget.desired[Axis::X].dim == Dim::Children) {
        accumulated_snap_tolerance_x = std::max(
            grid_snap_tolerance_x,
            num_layout_children * grid_snap_tolerance_x / 2.0f);
      }
      if (widget.desired[Axis::Y].dim == Dim::Children) {
        accumulated_snap_tolerance_y = std::max(
            grid_snap_tolerance_y,
            num_layout_children * grid_snap_tolerance_y / 2.0f);
      }
    }

    // Position children
    float offx = is_row ? start_offset : 0.f;
    float offy = is_column ? start_offset : 0.f;
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
        // Set computed_rel from the absolute position stored on UIComponent.
        // This is set during component init from with_absolute_position(x, y).
        child.computed_rel[Axis::X] = child.absolute_pos_x;
        child.computed_rel[Axis::Y] = child.absolute_pos_y;
        compute_relative_positions(child);
        continue;
      }

      // Ignore anything that should be hidden
      if (child.should_hide) {
        compute_relative_positions(child);
        continue;
      }

      // computed bounds - use margin for layout spacing
      float cx = child.computed[Axis::X] + child.computed_margin[Axis::X];
      float cy = child.computed[Axis::Y] + child.computed_margin[Axis::Y];

      bool will_hit_max_x = cx + offx > sx;
      bool will_hit_max_y = cy + offy > sy;

      bool no_flex = child.flex_direction == FlexDirection::None;
      // We cant flex and are going over the limit
      if (no_flex && (will_hit_max_x || will_hit_max_y)) {
        child.computed_rel[Axis::X] = sx;
        child.computed_rel[Axis::Y] = sy;
        continue;
      }

      // Helper to get child debug name
      auto get_child_debug_name = [this, child_id]() -> std::string {
        Entity &ent = this->to_ent(child_id);
        if (ent.has<UIComponentDebug>()) {
          return ent.get<UIComponentDebug>().name();
        }
        return fmt::format("entity_{}", child_id);
      };

      // Helper to get parent debug name
      auto get_parent_debug_name = [this, &widget]() -> std::string {
        Entity &ent = this->to_ent(widget.id);
        if (ent.has<UIComponentDebug>()) {
          return ent.get<UIComponentDebug>().name();
        }
        return fmt::format("entity_{}", widget.id);
      };

      // Wrap detection: use one-grid-unit tolerance to prevent wrapping
      // caused by grid snapping rounding errors (e.g., 3 buttons that fit
      // mathematically but wrap due to accumulated position snapping)
      bool will_wrap_column =
          is_column && cy + offy > sy + grid_snap_tolerance_y;
      bool will_wrap_row = is_row && cx + offx > sx + grid_snap_tolerance_x;

      // Warning checks use accumulated tolerance (scaled by child count
      // for children()-sized containers) to avoid false positives from
      // accumulated grid snapping across many children
      bool parent_is_scroll_view = to_ent(widget.id).has<HasScrollView>();
      constexpr float BASE_WRAP_TOLERANCE = 4.0f;
      bool should_warn_wrap_column =
          is_column && (cy + offy > sy + accumulated_snap_tolerance_y + BASE_WRAP_TOLERANCE);
      bool should_warn_wrap_row =
          is_row && (cx + offx > sx + accumulated_snap_tolerance_x + BASE_WRAP_TOLERANCE);
      if ((should_warn_wrap_column || should_warn_wrap_row) &&
          !parent_is_scroll_view) {
        bool should_warn = false;
        std::string warn_reason;

        // Condition 1: Parent has NoWrap set but would overflow
        if (widget.flex_wrap == FlexWrap::NoWrap) {
          should_warn = true;
          warn_reason = "NoWrap set but would overflow";
        }
        // Condition 2: Child FlexDirection matches parent
        // Only warn if the child itself has NoWrap, indicating the developer
        // did not intend wrapping. Without this, every naturally-wrapping
        // long page would trigger this warning.
        else if (child.flex_direction == widget.flex_direction &&
                 child.flex_wrap == FlexWrap::NoWrap) {
          should_warn = true;
          warn_reason = fmt::format(
              "Child FlexDirection matches parent ({}) and has NoWrap - may "
              "cause unexpected wrap",
              is_column ? "Column" : "Row");
        }
        // Condition 4: Debug flag enabled
        else if (child.debug_wrap) {
          should_warn = true;
          warn_reason = "debug_wrap enabled";
        }

        if (should_warn) {
          log_warn("Layout wrap: '{}' in parent '{}' - {} (child_size={:.1f}, "
                   "offset={:.1f}, container={:.1f})",
                   get_child_debug_name(), get_parent_debug_name(), warn_reason,
                   will_wrap_column ? cy : cx, will_wrap_column ? offy : offx,
                   will_wrap_column ? sy : sx);
        }
      }

      // If parent has NoWrap set, skip the wrap entirely
      if (widget.flex_wrap == FlexWrap::NoWrap) {
        // Don't wrap - items will overflow/clip
      } else {
        // Execute wrap logic
        if (will_wrap_column) {
          offy = 0;
          offx += col_w;
          col_w = cx;
        }

        if (will_wrap_row) {
          offx = 0;
          offy += col_h;
          col_h = cy;
        }
      }

      // Calculate cross-axis offset (self_align overrides parent's align_items)
      float cross_offset = 0.f;
      float child_cross = is_column ? cx : cy;
      float cross_remaining = cross_axis_size - child_cross;

      if (cross_remaining > 0.f) {
        // Use child's self_align if set, otherwise fall back to parent's align_items
        if (child.self_align != SelfAlign::Auto) {
          switch (child.self_align) {
          case SelfAlign::Auto:
            // Shouldn't reach here, but treat as FlexStart
            break;
          case SelfAlign::FlexStart:
            break;
          case SelfAlign::FlexEnd:
            cross_offset = cross_remaining;
            break;
          case SelfAlign::Center:
            cross_offset = cross_remaining / 2.f;
            break;
          }
        } else {
          // Use parent's align_items
          switch (widget.align_items) {
          case AlignItems::FlexStart:
            break;
          case AlignItems::FlexEnd:
            cross_offset = cross_remaining;
            break;
          case AlignItems::Center:
            cross_offset = cross_remaining / 2.f;
            break;
          case AlignItems::Stretch:
            // Stretch is handled in size calculation, not positioning
            break;
          }
        }
      }

      // Apply positions
      float final_x = is_column ? (offx + cross_offset) : offx;
      float final_y = is_column ? offy : (offy + cross_offset);

      if (enable_grid_snapping) {
        child.computed_rel[Axis::X] = snap_to_8pt_grid(final_x, Axis::X);
        child.computed_rel[Axis::Y] = snap_to_8pt_grid(final_y, Axis::Y);
      } else {
        child.computed_rel[Axis::X] = final_x;
        child.computed_rel[Axis::Y] = final_y;
      }

      // Condition 3: Check if child overflows parent bounds after positioning
      // Skip warning for scroll containers - overflow is expected behavior
      // Use accumulated tolerance to handle grid snapping rounding errors
      // Base tolerance of 2px catches floating-point rounding from
      // screen_pct and percentage calculations while still flagging real issues.
      // For wrapping layouts, skip the wrap-direction axis: wrapped children
      // legitimately exceed the single-column/row content area.
      constexpr float BASE_OVERFLOW_TOLERANCE = 4.0f;
      float child_end_x = child.computed_rel[Axis::X] + cx;
      float child_end_y = child.computed_rel[Axis::Y] + cy;
      bool wraps = widget.flex_wrap == FlexWrap::Wrap;
      // Column wraps horizontally → X overflow from wrapping is expected
      // Row wraps vertically → Y overflow from wrapping is expected
      bool suppress_x = wraps && is_column;
      bool suppress_y = wraps && is_row;
      bool overflows_x = !suppress_x && child_end_x > sx + accumulated_snap_tolerance_x + BASE_OVERFLOW_TOLERANCE;
      bool overflows_y = !suppress_y && child_end_y > sy + accumulated_snap_tolerance_y + BASE_OVERFLOW_TOLERANCE;
      if ((overflows_x || overflows_y) && !parent_is_scroll_view) {
        log_warn("Layout overflow: '{}' extends outside parent '{}' bounds "
                 "(child_rel=[{:.1f},{:.1f}], child_size=[{:.1f},{:.1f}], "
                 "child_end=[{:.1f},{:.1f}], parent_size=[{:.1f},{:.1f}], "
                 "gap={:.1f}, start_offset={:.1f})",
                 get_child_debug_name(), get_parent_debug_name(),
                 child.computed_rel[Axis::X], child.computed_rel[Axis::Y], cx,
                 cy, child_end_x, child_end_y, sx, sy, gap, start_offset);
      }

      constexpr float GRID_UNIT_720P = 4.0f;
      float grid_unit_y =
          GRID_UNIT_720P * (fetch_screen_value_(Axis::Y) / 720.0f);
      float grid_unit_x =
          GRID_UNIT_720P * (fetch_screen_value_(Axis::X) / 720.0f);

      // Setup for next child placement (include gap for justify)
      if (is_column) {
        float next_y = offy + cy + gap;
        // Note: We no longer add grid_unit_y here - it was causing
        // accumulated offset that breaks justify calculations.
        // Grid snapping now only affects final position, not child spacing.
        if (enable_grid_snapping) {
          next_y = snap_to_8pt_grid(next_y, Axis::Y);
        }
        offy = next_y;
      }
      if (is_row) {
        float next_x = offx + cx + gap;
        // Note: We no longer add grid_unit_x here - it was causing
        // accumulated offset that breaks justify calculations.
        // Grid snapping now only affects final position, not child spacing.
        if (enable_grid_snapping) {
          next_x = snap_to_8pt_grid(next_x, Axis::X);
        }
        offx = next_x;
      }

      update_max_size(cx, cy);
      compute_relative_positions(child);
    }

    // Fix for wrapped containers with children() sizing:
    // If this widget was sized by children() and wrapping occurred,
    // update the computed size to reflect the actual wrapped dimensions.
    // After positioning, offy + col_h represents the total wrapped height,
    // and offx + col_w represents the total wrapped width.
    // Add padding back since computed[axis] is the full box (content + padding).
    if (widget.desired[Axis::Y].dim == Dim::Children && is_row) {
      float wrapped_height = offy + col_h + widget.computed_padd[Axis::Y];
      if (wrapped_height > widget.computed[Axis::Y]) {
        widget.computed[Axis::Y] = wrapped_height;
      }
    }
    if (widget.desired[Axis::X].dim == Dim::Children && is_column) {
      float wrapped_width = offx + col_w + widget.computed_padd[Axis::X];
      if (wrapped_width > widget.computed[Axis::X]) {
        widget.computed[Axis::X] = wrapped_width;
      }
    }
  }

  void compute_rect_bounds(UIComponent &widget) {
    // log_trace("computing rect bounds for {}", widget);

    Vector2Type offset = Vector2Type{0.f, 0.f};
    if (widget.parent != -1) {
      UIComponent &parent = this->to_cmp(widget.parent);
      // Include parent's position, margin, AND padding to position child
      // within parent's content area (after padding)
      offset = Vector2Type{
          parent.computed_rel[Axis::X] + parent.computed_margin[Axis::left] +
              parent.computed_padd[Axis::left],
          parent.computed_rel[Axis::Y] + parent.computed_margin[Axis::top] +
              parent.computed_padd[Axis::top],
      };
      widget.computed_rel[Axis::X] += offset.x;
      widget.computed_rel[Axis::Y] += offset.y;
    }

    // Note: Widget's own padding affects its children, not its own position.
    // The padding is included in the offset calculation above when children
    // compute their bounds.

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
                         const std::map<EntityID, RefEntity> &map,
                         bool enable_grid_snapping = false,
                         float ui_scale = 1.0f) {
    AutoLayout al(resolution, map);
    al.set_grid_snapping(enable_grid_snapping);
    al.ui_scale = ui_scale;

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
    return ui::UICollectionHolder::getEntityForIDEnforce(id);
  }

  static UIComponent &to_cmp_static(EntityID id) {
    return to_ent_static(id).get<UIComponent>();
  }
};

} // namespace ui

} // namespace afterhours
