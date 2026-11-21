# RFC: Fix Auto Layout Issues with Percentage-Sized Elements and Children-Sized Parents

## Problem Statement

The current auto layout system has a fundamental limitation: **parents sized with `Dim::Children` cannot have children sized with `Dim::Percent`**. This creates a circular dependency that prevents proper layout computation.

### The Circular Dependency Problem

1. **Parent** needs to know its children's sizes to determine its own size (`Dim::Children`)
2. **Children** need to know their parent's size to calculate percentage-based dimensions (`Dim::Percent`)
3. Neither can be computed without the other, creating a deadlock

The error is explicitly caught in the code:

```cpp
// From src/plugins/autolayout.h lines 952-984
log_error("Parents sized with mode 'children' cannot have "
          "children sized with mode 'percent'. Failed when checking "
          "children for {} axis {}",
          widget.id, axis);

VALIDATE(false, "Parents sized with mode 'children' cannot have "
                "children sized with mode 'percent'.");
```

## Current System Analysis

### Layout Execution Order

The system processes layouts in this sequence:
1. `calculate_standalone()` - Elements that don't depend on others
2. `calculate_those_with_parents()` - Elements that depend on parent sizes
3. `calculate_those_with_children()` - Elements that depend on child sizes
4. `solve_violations()` - Resolve size conflicts
5. `compute_relative_positions()` - Position elements
6. `compute_rect_bounds()` - Final positioning

### Dimension Types

The system supports these dimension types:
- `Dim::None` - No specific sizing
- `Dim::Pixels` - Fixed pixel size
- `Dim::Text` - Size based on text content
- `Dim::Percent` - Percentage of parent size
- `Dim::Children` - Size based on children
- `Dim::ScreenPercent` - Percentage of screen size

### Current Validation Logic

The system validates this constraint in two places:
1. `_sum_children_axis_for_child_exp()` - When summing child sizes
2. `_max_child_size()` - When finding maximum child size

Both functions throw errors when they encounter the forbidden combination.

## Example Use Cases That Fail

### Test Case: Grandparent Setup with Percentage Padding

```cpp
// From example/ui_layout/main.cpp lines 182-250
std::array<RefEntity, 3> grandparent_setup_padding(
    Entity &sophie, Axis axis, Size padding_size,
    ApplicationLocation location = ApplicationLocation::Grandparent) {

  auto &div = EntityHelper::createEntity();
  {
    auto &c = make_component(div)
                  .set_desired_width(pixels(100.f))
                  .set_desired_height(percent(0.5f))  // ← Percentage height
                  .set_parent(sophie);

    if (location == ApplicationLocation::Grandparent)
      c.set_desired_padding(padding_size, axis);
  }

  auto &parent = EntityHelper::createEntity();
  {
    auto &c = make_component(parent)
                  .set_desired_width(percent(1.f))    // ← Percentage width
                  .set_desired_height(percent(0.5f))  // ← Percentage height
                  .set_parent(div);

    if (location == ApplicationLocation::Parent)
      c.set_desired_padding(padding_size, axis);
  }

  auto &child = EntityHelper::createEntity();
  {
    auto &c = make_component(child)
                  .set_desired_width(percent(1.f))    // ← Percentage width
                  .set_desired_height(percent(0.5f))  // ← Percentage height
                  .set_parent(parent);

    if (location == ApplicationLocation::Child)
      c.set_desired_padding(padding_size, axis);
  }

  run_(sophie);
  return {{div, parent, child}};
}
```

This creates a hierarchy where:
- Root (`sophie`) has fixed dimensions
- `div` has percentage height (50% of parent)
- `parent` has percentage dimensions (100% width, 50% height)
- `child` has percentage dimensions (100% width, 50% height)

The issue arises when any parent in the chain needs to be sized by its children while having percentage-sized children.

## Proposed Solutions

### Solution 1: Multi-Pass Layout Resolution

Implement an iterative approach that resolves circular dependencies through multiple passes:

```cpp
void resolve_circular_dependencies(UIComponent &widget) {
    const int MAX_PASSES = 10;
    const float TOLERANCE = 1.0f;
    
    for (int pass = 0; pass < MAX_PASSES; pass++) {
        float total_change = 0.0f;
        
        // First pass: estimate sizes for percentage-based children
        if (pass == 0) {
            estimate_percentage_sizes(widget);
        }
        
        // Calculate sizes in dependency order
        calculate_standalone(widget);
        calculate_those_with_parents(widget);
        calculate_those_with_children(widget);
        
        // Check if we've converged
        total_change = measure_layout_changes(widget);
        if (total_change < TOLERANCE) {
            break;
        }
    }
}

void estimate_percentage_sizes(UIComponent &widget) {
    // For percentage-sized children of children-sized parents,
    // provide reasonable initial estimates
    for (EntityID child_id : widget.children) {
        UIComponent &child = to_cmp(child_id);
        if (child.desired[Axis::X].dim == Dim::Percent && 
            widget.desired[Axis::X].dim == Dim::Children) {
            // Estimate based on available space or default values
            child.computed[Axis::X] = estimate_percentage_size(child, Axis::X);
        }
        if (child.desired[Axis::Y].dim == Dim::Percent && 
            widget.desired[Axis::Y].dim == Dim::Children) {
            child.computed[Axis::Y] = estimate_percentage_size(child, Axis::Y);
        }
        
        estimate_percentage_sizes(child);
    }
}
```

**Pros:**
- Maintains existing API
- Handles circular dependencies gracefully
- Converges to stable solution

**Cons:**
- Multiple layout passes required
- May not always converge
- Performance impact on complex layouts

### Solution 2: Constraint-Based Layout System

Replace the current dependency-based system with a constraint solver:

```cpp
struct LayoutConstraint {
    enum Type {
        FixedSize,
        PercentageOfParent,
        SizeToChildren,
        MinSize,
        MaxSize,
        AspectRatio
    };
    
    Type type;
    float value;
    EntityID target;
    Axis axis;
    float priority;  // Higher priority constraints resolved first
};

class ConstraintSolver {
    std::vector<LayoutConstraint> constraints;
    
public:
    void add_constraint(const LayoutConstraint& constraint);
    bool solve();
    void resolve_circular_constraints();
    
private:
    bool detect_circular_constraints();
    void break_circular_constraints();
    float solve_constraint(const LayoutConstraint& constraint);
};
```

**Pros:**
- More flexible and powerful
- Can handle complex constraint relationships
- Better separation of concerns

**Cons:**
- Major architectural change
- More complex to implement
- Different API for users

### Solution 3: Hybrid Approach with Smart Fallbacks

Enhance the current system with intelligent fallbacks for circular dependencies:

```cpp
float compute_size_with_fallback(UIComponent &widget, Axis axis) {
    const Size &exp = widget.desired[axis];
    
    // Try normal computation first
    if (exp.dim == Dim::Percent) {
        float result = compute_size_for_parent_expectation(widget, axis);
        if (result != -1) return result;
        
        // Fallback: use a reasonable default based on siblings
        return compute_fallback_size(widget, axis);
    }
    
    if (exp.dim == Dim::Children) {
        float result = compute_size_for_child_expectation(widget, axis);
        if (result != -1) return result;
        
        // Fallback: use minimum size or content size
        return compute_minimum_size(widget, axis);
    }
    
    return widget.computed[axis];
}

float compute_fallback_size(UIComponent &widget, Axis axis) {
    // Strategy 1: Use sibling sizes as reference
    if (widget.parent != -1) {
        UIComponent &parent = to_cmp(widget.parent);
        float total_sibling_size = 0;
        int sibling_count = 0;
        
        for (EntityID sibling_id : parent.children) {
            if (sibling_id != widget.id) {
                UIComponent &sibling = to_cmp(sibling_id);
                if (sibling.computed[axis] != -1) {
                    total_sibling_size += sibling.computed[axis];
                    sibling_count++;
                }
            }
        }
        
        if (sibling_count > 0) {
            float avg_sibling_size = total_sibling_size / sibling_count;
            return avg_sibling_size * widget.desired[axis].value;
        }
    }
    
    // Strategy 2: Use default size based on content type
    return get_default_size_for_content(widget, axis);
}
```

**Pros:**
- Minimal API changes
- Graceful degradation
- Maintains backward compatibility

**Cons:**
- May produce unexpected results
- Fallback logic can be complex
- Less predictable behavior

### Solution 4: Layout Templates and Presets

Create predefined layout patterns that avoid circular dependencies:

```cpp
enum class LayoutTemplate {
    VerticalStack,      // Parent: fixed height, children: percentages
    HorizontalFlow,     // Parent: fixed width, children: percentages  
    Grid,               // Parent: fixed dimensions, children: percentages
    FlexContainer,      // Parent: children-based, children: fixed/flex
    ResponsivePanel     // Parent: screen-based, children: percentages
};

void apply_layout_template(UIComponent &widget, LayoutTemplate template) {
    switch (template) {
        case LayoutTemplate::VerticalStack:
            widget.set_desired_height(pixels(100.f));
            // Children can use percentages safely
            break;
            
        case LayoutTemplate::FlexContainer:
            widget.set_desired_width(children());
            widget.set_desired_height(children());
            // Children should use fixed sizes or flex
            break;
            
        case LayoutTemplate::Grid:
            widget.set_desired_width(pixels(300.f));
            widget.set_desired_height(pixels(200.f));
            // Children can use percentages safely
            break;
    }
}

// Usage example
auto &container = make_component(entity)
    .apply_layout_template(LayoutTemplate::VerticalStack);

auto &child1 = make_component(child1_entity)
    .set_desired_height(percent(0.3f))  // Safe: parent has fixed height
    .set_parent(container);
```

**Pros:**
- Prevents circular dependencies by design
- Easy to use correctly
- Clear intent and documentation

**Cons:**
- Less flexible than free-form layout
- May not cover all use cases
- Additional API surface

## Recommended Approach

I recommend implementing **Solution 3 (Hybrid Approach with Smart Fallbacks)** as the primary solution, with **Solution 4 (Layout Templates)** as a complementary feature.

### Implementation Plan

#### Phase 1: Smart Fallbacks
1. Implement `compute_size_with_fallback()` function
2. Add fallback logic for percentage-sized children
3. Add fallback logic for children-sized parents
4. Update validation to use fallbacks instead of errors

#### Phase 2: Layout Templates
1. Define common layout patterns
2. Implement template application logic
3. Add validation to ensure templates don't create circular dependencies
4. Document best practices

#### Phase 3: Enhanced Error Handling
1. Replace hard errors with warnings
2. Add logging for fallback usage
3. Provide suggestions for fixing problematic layouts
4. Add layout validation tools

### Backward Compatibility

The proposed changes maintain full backward compatibility:
- Existing layouts continue to work
- New fallback behavior is transparent
- Layout templates are optional
- Error messages become warnings with suggestions

### Testing Strategy

1. **Unit Tests**: Test fallback logic with various constraint combinations
2. **Integration Tests**: Verify that problematic layouts now work
3. **Performance Tests**: Ensure fallbacks don't significantly impact performance
4. **Regression Tests**: Verify existing layouts still work correctly

## Conclusion

The circular dependency issue between percentage-sized elements and children-sized parents is a fundamental limitation of the current auto layout system. By implementing smart fallbacks and layout templates, we can provide users with a more robust and flexible layout system while maintaining backward compatibility.

The hybrid approach offers the best balance of functionality, performance, and ease of implementation. It allows users to create complex layouts that would previously fail, while providing clear guidance on how to structure layouts to avoid issues.