
#pragma once

#include "../base_component.h"
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

using FontID = int;

struct AutoLayoutRoot : BaseComponent {};

struct UIComponent : BaseComponent {
  EntityID id;
  explicit UIComponent(EntityID id_) : id(id_) {}

  std::array<Size, 2> desired;

  bool absolute = false;
  std::array<float, 2> computed;
  std::array<float, 2> computed_rel;

  EntityID parent = -1;
  std::vector<EntityID> children;

  FontID fontID = -1;

  Rectangle rect() const {
    return Rectangle{
        .x = computed_rel[0],
        .y = computed_rel[1],
        .width = computed[0],
        .height = computed[1],
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

  auto &set_parent(EntityID id_) {
    parent = id_;
    return *this;
  }

  auto &set_desired_x(Size s) {
    desired[0] = s;
    return *this;
  }

  auto &set_desired_y(Size s) {
    desired[1] = s;
    return *this;
  }
};

struct AutoLayout {
  static Entity &to_ent(EntityID id) {
    return EntityQuery().whereID(id).gen_first_enforce();
  }

  static UIComponent &to_cmp(EntityID id) {
    return EntityQuery().whereID(id).gen_first_enforce().get<UIComponent>();
  }

  static float compute_size_for_standalone_expectation(UIComponent &widget,
                                                       size_t exp_index) {
    Size exp = widget.desired[exp_index];
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
      return widget.computed[exp_index];
    }
  }

  static void calculate_standalone(UIComponent &widget) {
    auto size_x = compute_size_for_standalone_expectation(widget, 0);
    auto size_y = compute_size_for_standalone_expectation(widget, 1);

    widget.computed[0] = size_x;
    widget.computed[1] = size_y;

    for (EntityID child_id : widget.children) {
      calculate_standalone(to_cmp(child_id));
    }
  }

  static float compute_size_for_parent_expectation(UIComponent &widget,
                                                   size_t exp_index) {
    if (widget.absolute && widget.desired[exp_index].dim == Dim::Percent) {
      VALIDATE(false, "Absolute widgets should not use Percent");
    }

    float no_change = widget.computed[exp_index];
    if (widget.parent == -1)
      return no_change;

    float parent_size = to_cmp(widget.parent).computed[exp_index];
    Size exp = widget.desired[exp_index];
    float new_size = exp.value * parent_size;
    switch (exp.dim) {
    case Dim::Percent:
      return parent_size == -1 ? no_change : new_size;
    default:
      return no_change;
    }
  }

  static void calculate_those_with_parents(UIComponent &widget) {
    auto size_x = compute_size_for_parent_expectation(widget, 0);
    auto size_y = compute_size_for_parent_expectation(widget, 1);

    widget.computed[0] = size_x;
    widget.computed[1] = size_y;

    for (EntityID child_id : widget.children) {
      calculate_those_with_parents(to_cmp(child_id));
    }
  }

  static float _sum_children_axis_for_child_exp(UIComponent &widget,
                                                size_t exp_index) {
    float total_child_size = 0.f;
    for (EntityID entityID : widget.children) {
      UIComponent &child = to_cmp(entityID);
      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;

      float cs = child.computed[exp_index];
      //  no computed value yet
      if (cs == -1) {
        if ( //
            child.desired[exp_index].dim == Dim::Percent &&
            widget.desired[exp_index].dim == Dim::Children
            //
        ) {
          VALIDATE(false, "Parents sized with mode 'children' cannot have "
                          "children sized with mode 'percent'.");
        }
        // Instead of silently ignoring this, check the cases above
        VALIDATE(false, "expect that all children have been solved by now");
      }
      total_child_size += cs;
    }
    return total_child_size;
  }

  static float compute_size_for_child_expectation(UIComponent &widget,
                                                  size_t exp_index) {
    float no_change = widget.computed[exp_index];
    if (widget.children.empty())
      return no_change;

    Size exp = widget.desired[exp_index];
    if (exp.dim != Dim::Children)
      return no_change;

    float total_child_size =
        _sum_children_axis_for_child_exp(widget, exp_index);

    return total_child_size;
  }

  static void calculate_those_with_children(UIComponent &widget) {
    for (EntityID child : widget.children) {
      calculate_those_with_children(to_cmp(child));
    }

    auto size_x = compute_size_for_child_expectation(widget, 0);
    auto size_y = compute_size_for_child_expectation(widget, 1);

    widget.computed[0] = size_x;
    widget.computed[1] = size_y;
  }

  static void tax_refund(UIComponent &widget, size_t exp_index, float error) {
    int num_eligible_children = 0;
    for (EntityID child_id : widget.children) {
      UIComponent &child = to_cmp(child_id);
      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;

      Size exp = child.desired[exp_index];
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
      UIComponent &child = to_cmp(child_id);

      // Dont worry about any children that are absolutely positioned
      if (child.absolute)
        continue;

      Size exp = child.desired[exp_index];
      if (exp.strictness == 0.f) {
        child.computed[exp_index] += abs(indiv_refund);
        log_trace("Just gave back, time for trickle down");
        tax_refund(child, exp_index, indiv_refund);
      }
      // TODO idk if we should do this for all non 1.f children?
      // if (exp.strictness == 1.f || child->ignore_size) {
      // continue;
      // }
    }
  }

  static void solve_violations(UIComponent &widget) {
    // we dont care if its less than a pixel
    const float ACCEPTABLE_ERROR = 1.f;

    size_t num_children = 0;
    for (EntityID child : widget.children) {
      if (to_cmp(child).absolute)
        continue;
      num_children++;
    }
    if (num_children == 0)
      return;

    // me -> left -> right

    const auto _total_child = [&widget](size_t exp_index) {
      float sum = 0.f;
      // TODO support growing
      for (EntityID child : widget.children) {
        // Dont worry about any children that are absolutely positioned
        if (to_cmp(child).absolute)
          continue;
        sum += to_cmp(child).computed[exp_index];
      }
      return sum;
    };

    const auto _solve_error_optional = [&widget](size_t exp_index,
                                                 float *error) {
      int num_optional_children = 0;
      for (EntityID child : widget.children) {
        // TODO Dont worry about absolute positioned children
        if (to_cmp(child).absolute)
          continue;

        Size exp = to_cmp(child).desired[exp_index];
        if (exp.strictness == 0.f) {
          num_optional_children++;
        }
      }
      // were there any children who dont care about their size?
      if (num_optional_children != 0) {
        float approx_epc = *error / num_optional_children;
        for (EntityID child : widget.children) {
          UIComponent &child_cmp = to_cmp(child);
          // Dont worry about absolute positioned children
          if (child_cmp.absolute)
            continue;

          Size exp = child_cmp.desired[exp_index];
          if (exp.strictness == 0.f) {
            float cur_size = child_cmp.computed[exp_index];
            child_cmp.computed[exp_index] = fmaxf(0, cur_size - approx_epc);
            if (cur_size > approx_epc)
              *error -= approx_epc;
          }
        }
      }
    };

    const auto fix_violating_children = [num_children, &widget](
                                            size_t exp_index, float error) {
      VALIDATE(num_children != 0, "Should never have zero children");

      size_t num_strict_children = 0;
      size_t num_ignorable_children = 0;

      for (EntityID child : widget.children) {
        Size exp = to_cmp(child).desired[exp_index];
        if (exp.strictness == 1.f) {
          num_strict_children++;
        }
        if (to_cmp(child).absolute) {
          num_ignorable_children++;
        }
      }

      // If there is any error left,
      // we have to take away from the allowed children;

      size_t num_resizeable_children =
          num_children - num_strict_children - num_ignorable_children;

      /* support grow flags
      // TODO we cannot enforce the assert below in the case of wrapping
      // because the positioning happens after error correction
      if (error > ACCEPTABLE_ERROR && num_resizeable_children == 0 &&
          //
          !((widget->growflags & GrowFlags::Column) ||
            (widget->growflags & GrowFlags::Row))
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
        UIComponent &child_cmp = to_cmp(child);
        Size exp = child_cmp.desired[exp_index];
        if (exp.strictness == 1.f || child_cmp.absolute) {
          continue;
        }
        float portion_of_error = (1.f - exp.strictness) * approx_epc;
        float cur_size = child_cmp.computed[exp_index];
        child_cmp.computed[exp_index] = fmaxf(0, cur_size - portion_of_error);
        // Reduce strictness every round
        // so that eventually we will get there
        exp.strictness = fmaxf(0.f, exp.strictness - 0.05f);
        child_cmp.desired[exp_index] = exp;
      }
    };

    const auto compute_error = [ACCEPTABLE_ERROR, _total_child,
                                _solve_error_optional, fix_violating_children,
                                &widget](size_t exp_index) -> float {
      float my_size = widget.computed[exp_index];
      float all_children = _total_child(exp_index);
      float error = all_children - my_size;
      // std::cout << "starting error " << exp_index << " " << error << "\n";

      int i_x = 0;
      while (error > ACCEPTABLE_ERROR) {
        _solve_error_optional(0, &error);
        i_x++;

        fix_violating_children(0, error);
        all_children = _total_child(0);
        error = all_children - my_size;
        if (i_x > 100) {
          log_warn("Hit X-axis iteration limit trying to solve violations");
          // VALIDATE(false, "hit x iteration limit trying to solve
          // violations");
          break;
        }
      }
      return error;
    };

    float error_x = compute_error(0);
    if (error_x < 0) {
      tax_refund(widget, 0, error_x);
    }

    float error_y = compute_error(1);
    if (error_y < 1) {
      tax_refund(widget, 0, error_y);
    }

    // Solve for children
    for (EntityID child : widget.children) {
      solve_violations(to_cmp(child));
    }
  }

  static void compute_relative_positions(UIComponent &widget) {
    if (widget.parent == -1) {
      // This already happens by default, but lets be explicit about it
      widget.computed_rel[0] = 0.f;
      widget.computed_rel[1] = 0.f;
    }

    // Assuming we dont care about things smaller than 1 pixel
    widget.computed[0] = round(widget.computed[0]);
    widget.computed[1] = round(widget.computed[1]);
    float sx = widget.computed[0];
    float sy = widget.computed[1];

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
      UIComponent &child = to_cmp(child_id);

      // Dont worry about any children that are absolutely positioned
      if (child.absolute) {
        compute_relative_positions(child);
        continue;
      }

      float cx = child.computed[0];
      float cy = child.computed[1];

      bool will_hit_max_x = false;
      bool will_hit_max_y = false;

      will_hit_max_x = cx + offx > sx;
      will_hit_max_y = cy + offy > sy;

      // We cant grow and are going over the limit
      if ((will_hit_max_x || will_hit_max_y)) {
        child.computed_rel[0] = sx;
        child.computed_rel[1] = sy;
        continue;
      }

      // We can grow vertically and current child will push us over height
      // lets wrap
      if (cy + offy > sy) {
        offy = 0;
        offx += col_w;
        col_w = cx;
      }

      // We can grow horizontally and current child will push us over
      // width lets wrap
      // TODO support row layout
      if (false && cx + offx > sx) {
        offx = 0;
        offy += col_h;
        col_h = cy;
      }

      child.computed_rel[0] = offx;
      child.computed_rel[1] = offy;

      // Setup for next child placement
      // if (widget->growflags & GrowFlags::Column)
      { offy += cy; }
      // TODO support row layout
      // if (widget->growflags & GrowFlags::Row)
      // {
      // offx += cx;
      // }

      update_max_size(cx, cy);
      compute_relative_positions(child);
    }
  }

  static void compute_rect_bounds(UIComponent &widget) {
    // log_trace("computing rect bounds for {}", widget);

    Vector2Type offset = Vector2Type{0.f, 0.f};
    if (widget.parent != -1) {
      UIComponent &parent = to_cmp(widget.parent);
      offset = offset + Vector2Type{parent.rect().x, parent.rect().y};
    }

    widget.computed_rel[0] += offset.x;
    widget.computed_rel[1] += offset.y;

    for (EntityID child : widget.children) {
      compute_rect_bounds(to_cmp(child));
    }
  }

  static void autolayout(UIComponent &widget) {
    // - (any) compute solos (doesnt rely on parent/child / other widgets)
    calculate_standalone(widget);
    // - (pre) parent sizes
    calculate_those_with_parents(widget);
    // - (post) children
    calculate_those_with_children(widget);
    // - (pre) solve violations
    solve_violations(widget);
    // - (pre) compute relative positions
    compute_relative_positions(widget);
    // - (pre) compute rect bounds
    compute_rect_bounds(widget);
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
      print_tree(to_cmp(child_id), tab + 1);
    }
  }
};

} // namespace ui

} // namespace afterhours
