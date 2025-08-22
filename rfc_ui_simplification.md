# RFC: UI System Simplification

## Overview

This RFC proposes a series of simplifications to the AfterHours UI system to reduce complexity, improve maintainability, and enhance developer experience while preserving all existing functionality.

## Current Architecture Analysis

The UI system currently consists of several complex layers:

### 1. Entity-Component System
- Custom ECS implementation with components like `UIComponent`, `HasClickListener`, `HasColor`, etc.
- Maximum of 128 components per entity (configurable via `AFTER_HOURS_MAX_COMPONENTS`)
- Complex component management with type IDs and bit sets

### 2. Immediate Mode UI
- Builder-style functions: `div()`, `button()`, `checkbox()`, `image()`, etc.
- Complex `ComponentConfig` system with inheritance and chaining
- Entity creation and management through `EntityParent` pairs

### 3. Auto-layout System
- Flexbox-like layout with 6 dimension types:
  - `Dim::None`, `Dim::Pixels`, `Dim::Text`, `Dim::Percent`, `Dim::Children`, `Dim::ScreenPercent`
- Complex sizing calculations with strictness values
- Parent-child relationship management

### 4. Rendering System
- Separate rendering logic from state management
- Focus management with visual focus IDs
- Opacity and transformation handling
- Complex texture and image rendering

### 5. Input Handling
- Multiple specialized systems for different input types
- Focus tracking, hot states, and active states
- Complex tabbing and navigation logic

## Identified Simplification Opportunities

### 1. Component Consolidation

#### Current State
Many small, single-purpose components that could be logically grouped:

```cpp
// Input handling components
struct HasClickListener : BaseComponent { /* ... */ };
struct HasDragListener : BaseComponent { /* ... */ };
struct HasLeftRightListener : BaseComponent { /* ... */ };

// Visual styling components  
struct HasColor : BaseComponent { /* ... */ };
struct HasOpacity : BaseComponent { /* ... */ };
struct HasRoundedCorners : BaseComponent { /* ... */ };
struct HasUIModifiers : BaseComponent { /* ... */ };

// State management components
struct HasCheckboxState : BaseComponent { /* ... */ };
struct HasSliderState : BaseComponent { /* ... */ };
struct HasDropdownState : ui::HasCheckboxState { /* ... */ };
```

#### Proposed Consolidation
```cpp
// Unified input handling
struct HasInputHandlers : BaseComponent {
    std::function<void(Entity&)> onClick;
    std::function<void(Entity&)> onDrag;
    std::function<void(Entity&, int)> onLeftRight;
    std::function<void(Entity&)> onHover;
    std::function<void(Entity&)> onFocus;
    // ... other handlers
};

// Unified visual styling
struct HasVisualStyle : BaseComponent {
    Color color;
    float opacity = 1.0f;
    std::bitset<4> roundedCorners;
    float scale = 1.0f;
    Vector2Type translate = {0.f, 0.f};
    // ... other visual properties
};

// Unified state management
struct HasState : BaseComponent {
    std::variant<bool, float, size_t, std::string> value;
    std::function<void(const HasState&)> onChange;
    bool changed_since = false;
    // ... other state properties
};
```

### 2. Immediate Mode API Simplification

#### Current State
Complex configuration objects with inheritance and verbose setup:

```cpp
auto elem = div(ctx, mk(entity), 
    ComponentConfig::inherit_from(config)
        .with_size(ComponentSize{icon_width, icon_height})
        .with_margin(config.margin)
        .with_padding(config.padding)
        .with_flex_direction(FlexDirection::Row)
        .with_debug_name(fmt::format("icon_row_item_{}", i)));
```

#### Proposed Simplification
```cpp
auto elem = div(ctx, mk(entity))
    .size(icon_width, icon_height)
    .margin(config.margin)
    .padding(config.padding)
    .flex_direction(FlexDirection::Row)
    .debug_name("icon_row_item_" + std::to_string(i));
```

### 3. Layout System Simplification

#### Current State
6 different dimension types with complex calculations:

```cpp
enum struct Dim {
    None, Pixels, Text, Percent, Children, ScreenPercent
};

struct Size {
    Dim dim = Dim::None;
    float value = -1;
    float strictness = 1.f;
};
```

#### Proposed Simplification
```cpp
enum struct Dim {
    Fixed,      // Pixels, absolute values
    Relative,   // Percent, screen percent
    Content     // Text, children, auto-sizing
};

struct Size {
    Dim dim = Dim::Fixed;
    float value = 0.f;
    float flex = 1.f;  // Replaces strictness, more intuitive
};
```

### 4. System Consolidation

#### Current State
Multiple specialized systems for different input types:

```cpp
struct HandleClicks : SystemWithUIContext<ui::HasClickListener> { /* ... */ };
struct HandleDrags : SystemWithUIContext<ui::HasDragListener> { /* ... */ };
struct HandleLeftRight : SystemWithUIContext<ui::HasLeftRightListener> { /* ... */ };
struct HandleTabbing : SystemWithUIContext<> { /* ... */ };
```

#### Proposed Consolidation
```cpp
struct HandleInput : SystemWithUIContext<ui::HasInputHandlers> {
    // Unified input handling for all input types
    // Single system iteration instead of multiple
};
```

### 5. Entity Lifecycle Simplification

#### Current State
Multiple manual steps to create UI elements:

```cpp
auto &entity = EntityHelper::createEntity();
entity.addComponent<UIComponent>(entity.id);
entity.addComponent<HasClickListener>(callback);
entity.addComponent<HasColor>(color);
entity.addComponent<HasOpacity>(0.8f);
```

#### Proposed Simplification
```cpp
auto entity = ui::create_element()
    .on_click(callback)
    .color(color)
    .opacity(0.8f);
```

## Implementation Plan

### Phase 1: Component Consolidation (Breaking Changes)
- [ ] Create new consolidated components
- [ ] Implement migration helpers for existing code
- [ ] Mark old components as deprecated
- [ ] Update all systems to use new components
- [ ] Remove deprecated components

### Phase 2: API Simplification (Non-breaking)
- [ ] Implement method chaining for ComponentConfig
- [ ] Create higher-level widget constructors
- [ ] Simplify dimension system
- [ ] Update examples and documentation

### Phase 3: System Consolidation (Breaking Changes)
- [ ] Merge input handling systems
- [ ] Unify rendering logic
- [ ] Simplify focus management
- [ ] Update system registration

### Phase 4: Entity Lifecycle Simplification (Non-breaking)
- [ ] Create factory functions for common patterns
- [ ] Implement automatic cleanup
- [ ] Add lifecycle management
- [ ] Update examples

## Benefits

### 1. Reduced Complexity
- Fewer components to understand and maintain
- Simplified system architecture
- Cleaner, more focused code

### 2. Better Performance
- Fewer component lookups
- Reduced system iterations
- More efficient memory usage

### 3. Improved Developer Experience
- Intuitive API design
- Less boilerplate code
- Easier debugging and testing

### 4. Enhanced Maintainability
- Centralized logic for related functionality
- Easier to add new features
- Reduced code duplication

## Backwards Compatibility

### Breaking Changes
- Component consolidation will require migration
- System consolidation will change system registration
- Some internal APIs may change

### Migration Strategy
- Provide migration helpers and examples
- Maintain deprecated components during transition period
- Clear documentation for breaking changes
- Automated migration tools where possible

### Non-breaking Changes
- API simplifications maintain existing interfaces
- Entity lifecycle improvements are additive
- Performance optimizations are transparent

## Risks and Mitigation

### Risk 1: Breaking Existing Code
- **Mitigation**: Comprehensive migration guide and helpers
- **Mitigation**: Gradual rollout with deprecation warnings

### Risk 2: Performance Regression
- **Mitigation**: Benchmarking before and after changes
- **Mitigation**: Performance testing in CI/CD pipeline

### Risk 3: Feature Loss
- **Mitigation**: Thorough testing of all existing functionality
- **Mitigation**: Feature parity validation

## Testing Strategy

### Unit Tests
- Test all consolidated components
- Verify migration helpers work correctly
- Ensure API compatibility

### Integration Tests
- Test complete UI workflows
- Verify system interactions
- Performance benchmarking

### Migration Tests
- Test migration from old to new components
- Verify no functionality is lost
- Performance comparison

## Timeline

- **Week 1-2**: Component consolidation design and implementation
- **Week 3-4**: API simplification and testing
- **Week 5-6**: System consolidation and validation
- **Week 7-8**: Entity lifecycle improvements and final testing
- **Week 9**: Documentation updates and release preparation

## Conclusion

This UI simplification effort will significantly improve the maintainability and developer experience of the AfterHours UI system while preserving all existing functionality. The phased approach ensures minimal disruption to existing code while providing clear migration paths.

The proposed changes represent a significant architectural improvement that will make the codebase easier to work with, more performant, and more maintainable for future development.