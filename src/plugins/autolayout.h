
#pragma once

#include "../base_component.h"
#include "../bitwise.h"
#include "../developer.h"
#include "../entity.h"
#include "../entity_query.h"

namespace afterhours {
using Rectangle = RectangleType;

namespace ui {

enum struct Dim {
  None,
  Pixels,
  Text,
  Percent,
  Children,
};

struct Size {
  Dim dim = Dim::None;
  float value = -1;
  float strictness = 1.f;
};

using ComponentSize = std::pair<Size, Size>;

enum struct FlexDirection {
  None = 1 << 0,
  Row = 1 << 1,
  Column = 1 << 2,
};

enum struct Axis { X, Y };
std::ostream &operator<<(std::ostream &os, const Axis &axis) {
  os << (axis == Axis::X ? "X-Axis" : "Y-Axis");
  return os;
}

using FontID = int;

struct AutoLayoutRoot : BaseComponent {};

struct UIComponent : BaseComponent {
  EntityID id;
  explicit UIComponent(EntityID id_) : id(id_) {}

  template <typename T> struct AxisArray {
    std::array<T, 2> data;
    T &operator[](Axis axis) { return data[static_cast<size_t>(axis)]; }

    const T &operator[](Axis axis) const {
      return data[static_cast<size_t>(axis)];
    }
  };

  AxisArray<Size> desired;
  FlexDirection flex_direction = FlexDirection::Column;

  bool is_visible = false;
  bool absolute = false;
  AxisArray<float> computed;
  AxisArray<float> computed_rel;

  EntityID parent = -1;
  std::vector<EntityID> children;

  FontID fontID = -1;

  Rectangle rect() const {
    return Rectangle{
        .x = computed_rel[Axis::X],
        .y = computed_rel[Axis::Y],
        .width = computed[Axis::X],
        .height = computed[Axis::Y],
    };
  };

  float x() const { return rect().x; }
  float y() const { return rect().y; }
  float width() const { return rect().width; }
  float height() const { return rect().height; }

  Rectangle focus_rect(int rw = 2) const {
    return Rectangle{x() - (float)rw, y() - (float)rw,
                     width() + (2.f * (float)rw), height() + (2.f * (float)rw)};
  }

  auto &add_child(EntityID id_) {
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

  auto &set_desired_width(Size s) {
    desired[Axis::X] = s;
    return *this;
  }

  auto &set_desired_height(Size s) {
    desired[Axis::Y] = s;
    return *this;
  }

  auto &set_flex_direction(FlexDirection flex) {
    flex_direction = flex;
    return *this;
  }

  void reset_computed_values() {
    computed[Axis::X] = 0.f;
    computed[Axis::Y] = 0.f;
    computed_rel[Axis::X] = 0.f;
    computed_rel[Axis::Y] = 0.f;
  }
};

struct AutoLayout {

  std::map<EntityID, RefEntity> mapping;
  AutoLayout(const std::map<EntityID, RefEntity> &mapping_ = {})
      : mapping(mapping_) {}

  Entity &to_ent(EntityID id) { return mapping.at(id); }

  virtual UIComponent &to_cmp(EntityID id) {
    return to_ent(id).get<UIComponent>();
  }

  float compute_size_for_standalone_expectation(UIComponent &widget,
                                                Axis axis) {
    Size exp = widget.desired[axis];
    switch (exp.dim) {
    case Dim::Pixels:
      return exp.value;
    case Dim::Text:
      // TODO figure this out
      // So we can use MeasureTextEx but
      // we need to know the font and spacing
      return 100.f;
    default:
      // This is not a standalone widget,
      // so just keep moving along
      return widget.computed[axis];
    }
  }

  void calculate_standalone(UIComponent &widget) {
    auto size_x = compute_size_for_standalone_expectation(widget, Axis::X);
    auto size_y = compute_size_for_standalone_expectation(widget, Axis::Y);

    widget.computed[Axis::X] = size_x;
    widget.computed[Axis::Y] = size_y;

    for (EntityID child_id : widget.children) {
      calculate_standalone(this->to_cmp(child_id));
    }
  }

  float compute_size_for_parent_expectation(const UIComponent &widget,
                                            Axis axis) {
    if (widget.absolute && widget.desired[axis].dim == Dim::Percent) {
      VALIDATE(false, "Absolute widgets should not use Percent");
    }

    float no_change = widget.computed[axis];
    if (widget.parent == -1)
      return no_change;

    float parent_size = this->to_cmp(widget.parent).computed[axis];
    Size exp = widget.desired[axis];
    switch (exp.dim) {
    case Dim::Percent:
      return parent_size == -1 ? no_change : exp.value * parent_size;
    default:
      return no_change;
    }
  }

  void calculate_those_with_parents(UIComponent &widget) {
    auto size_x = compute_size_for_parent_expectation(widget, Axis::X);
    auto size_y = compute_size_for_parent_expectation(widget, Axis::Y);

    widget.computed[Axis::X] = size_x;
    widget.computed[Axis::Y] = size_y;

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
        log_error("expect that all children have been solved by now");
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
    if (widget.children.empty()) {
      // if the component has no children, but the expected size was set,
      // use that instead
      // TODO does this need to be a setting? is this generally a decent choice
      if (widget.desired[axis].value > 0) {
        return widget.desired[axis].value;
      }
      return no_change;
    }

    float expectation = _sum_children_axis_for_child_exp(widget, axis);

    if ((widget.flex_direction & FlexDirection::Column) && axis == Axis::X) {
      expectation = _max_child_size(widget, axis);
    }

    Size exp = widget.desired[axis];
    if (exp.dim != Dim::Children)
      return no_change;

    return expectation;
  }

  void calculate_those_with_children(UIComponent &widget) {
    if (widget.children.size() == 0)
      return;

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
        if (exp.strictness == 1.f || child_cmp.absolute) {
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
    float sx = widget.computed[Axis::X];
    float sy = widget.computed[Axis::Y];

    float offx = 0.f;
    float offy = 0.f;

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

      float cx = child.computed[Axis::X];
      float cy = child.computed[Axis::Y];

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
      offset = offset + Vector2Type{parent.rect().x, parent.rect().y};
    }

    widget.computed_rel[Axis::X] += offset.x;
    widget.computed_rel[Axis::Y] += offset.y;

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
                         const std::map<EntityID, RefEntity> &map) {
    AutoLayout al(map);

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

  static void print_tree(UIComponent &cmp, size_t tab = 0) {

    for (size_t i = 0; i < tab; i++)
      std::cout << "  ";

    std::cout << cmp.id << " : ";
    std::cout << cmp.rect().x << ",";
    std::cout << cmp.rect().y << ",";
    std::cout << cmp.rect().width << ",";
    std::cout << cmp.rect().height << std::endl;

    for (EntityID child_id : cmp.children) {
      print_tree(to_cmp_static(child_id), tab + 1);
    }
  }
};

} // namespace ui

} // namespace afterhours
