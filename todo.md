# TODO List

> **📋 Regeneration Instructions for Cursor Agents:**
> 
> To update this file with current TODOs from the codebase, follow these steps:
> 
> 1. **Search for TODO comments:**
>    ```bash
>    grep -r --include="*.h" --include="*.cpp" --include="*.c" --include="*.hpp" --exclude-dir="vendor" "TODO\|FIXME\|HACK\|XXX\|NOTE" src/ example/ README.md
>    ```
> 
> 2. **For each TODO found:**
>    - Read the surrounding code context (±5-10 lines) to understand the issue
>    - Categorize as: Bug, Performance, Feature, Architecture, Documentation, etc.
>    - Assign priority: 🔴 High (bugs/safety), 🟡 Medium (performance/arch), 🟢 Low (features)
>    - Write clear explanation of what the TODO actually means and why it exists
> 
> 3. **Organize by:**
>    - Core System (entity, query, system, logging, etc.)
>    - Plugins (input, UI, texture, autolayout, etc.)  
>    - Examples and Documentation
>    - Technical Notes
> 
> 4. **Include summary section with:**
>    - Priority breakdown (High/Medium/Low)
>    - Statistics (total TODOs, critical issues, etc.)
>    - Quick reference for critical bugs vs features
> 
> 5. **Exclude vendor directory** - contains third-party code TODOs we don't control
> 
> 🔄 **Last Updated:** [Update this date when regenerating]

---

This document contains all TODO, FIXME, HACK, XXX, and NOTE comments found in the codebase, analyzed with context to understand the actual issues.

## Core System TODOs

### Entity Query System (`src/entity_query.h`)
- **Line 39-44**: **API Design Improvement** - Want cleaner query negation syntax like `.not(whereHasComponent<Example>())` instead of current verbose approach, but template type deduction makes this challenging
- **Line 118**: **Entity Conversion Support** - Missing functionality to convert Entities to other Entity types in the ordering system
- **Line 219-222**: **Performance - Memory Allocation** - Query system allocates/deallocates entire entity lists on every query. Should use indexer with filtering and copying at the end instead
- **Line 224-225**: **Entity Lifecycle Issue** - Newly created entities aren't available in queries until next system runs, which may cause timing issues (workaround: force_merge option)
- **Line 321**: **Performance - Unnecessary Sorting** - No need to sort when only one item exists in result set
- **Line 322**: **Performance - Partial Sort Optimization** - When using `gen_first()`, could use partial sort instead of full sort for better performance

### System Framework (`src/system.h`)
- **Line 49-55**: **Template Metaprogramming Challenge** - Want to support `Not<>` queries (e.g., `System<Health, Not<Dead>>`) to filter out unwanted components, but template metaprogramming complexity is blocking implementation
- **Line 159-161**: **Const System Registration Issue** - Systems that could be const but are registered as update systems fail because update only calls non-const `for_each_with` methods

### Entity Management (`src/entity.h`)
- **Line 235-238**: **Modern C++ Improvement** - Consider switching to functional monadic style using `optional.transform` to handle optional operations without exposing underlying value or existence checks

### Entity Helper (`src/entity_helper.h`)
- **Line 31**: **Code Quality - Typo** - Simple spelling error needs correction
- **Line 206**: **Technical Debt - shared_ptr Conversion** - Temporary conversion utility for things that currently need shared_ptr, suggests architectural inconsistency

### Base Component (`src/base_component.h`)
- **Line 24-28**: **Broken Component Limit Check** - Component ID overflow validation is commented out because "it doesn't work for some reason" - potential memory safety issue

### Developer Tools (`src/developer.h`)
- **Line 58**: **Code Organization** - Developer tools should be moved to dedicated file for better organization

### Logging System (`src/logging.h`)
- **Line 4**: **Missing Implementation** - Logging functions need to be actually implemented
- **Line 5**: **Code Organization** - Move logging to dedicated log.h file and include in other library parts

## Plugin TODOs

### Texture Manager (`src/plugins/texture_manager.h`)
- **Line 51**: **Code Reuse Issue** - Code was "stolen from transform" component, suggesting duplication that should be refactored
- **Line 87**: **Missing Feature - Text Alignment** - Need support for InnerLeft and InnerRight text alignment options

### Input System (`src/plugins/input_system.h`)
- **Line 114**: **Cross-Platform Controller Support** - Currently only using Xbox controller button names, need PlayStation and other controller support
- **Line 149**: **Missing Platform Support** - Need macOS-specific controller icons
- **Line 359**: **Key Mapping Bug** - Investigating why a key maps to the same value as KEY_R (potential input conflict)
- **Line 376**: **Complex Implementation** - Cryptic "good luck" comment suggests difficult/complex implementation ahead
- **Line 443**: **Configuration Missing** - Gamepad deadzone (0.25) is hardcoded, should be user-configurable
- **Line 514**: **Architecture Issue** - Need to replace with singleton query pattern
- **Line 523**: **Namespace Organization** - Input functionality should be moved out of input namespace for better organization

### UI System

#### Theme (`src/plugins/ui/theme.h`)
- **Line 56**: **UI Design - Color Choice** - Current error color needs improvement, better alternative needed

#### Rendering (`src/plugins/ui/rendering.h`)
- **Line 36**: **Performance - Missing Caching** - Rendering system needs caching implementation for better performance
- **Line 296**: **UI Design - Color Uncertainty** - Questioning if an additional color is needed for specific UI element
- **Line 338**: **Code Cleanup** - Note indicates TODO might not be needed (potential cleanup item)
- **Line 345-346**: **Filter Bug Investigation** - Adding certain components to filter causes query to return no entities (filtering logic bug)

#### UI Context (`src/plugins/ui/context.h`)
- **Line 34**: **Architecture - Code Organization** - UI context code should be moved to input system
- **Line 53**: **Circular Dependency Issue** - Can't add styling defaults due to circular dependency that needs resolution
- **Line 115**: **Incomplete Implementation** - Handling logic for certain UI use cases is incomplete

#### UI Systems (`src/plugins/ui/systems.h`)
- **Line 27**: **Architecture - Code Organization** - UI system code should live inside input_system
- **Line 176**: **Template System Limitation** - Likes current approach but wishes it worked better with Tags

#### UI Providers (`src/plugins/ui/providers.h`)
- **Line 88**: **Documentation Reference** - References message above for context (incomplete TODO)

#### Immediate Mode Components (`src/plugins/ui/immediate/imm_components.h`)
- **Line 128-129**: **UI Bug - Focus Ring** - Focus ring positioning incorrect because clickable area is checkbox_no_label element, not checkbox element
- **Line 153**: **UI Feature - Corner Merging** - When user customizes corner styles, need better way to merge them with defaults
- **Line 508-509**: **UI Feature - Dropdown Behavior** - Dropdown works well but needs way to close when user leaves without selecting

#### Component Config (`src/plugins/ui/immediate/component_config.h`)
- **Line 50**: **Configuration Design Question** - Questioning whether all component properties should be inheritable

### Auto Layout (`src/plugins/autolayout.h`)
- **Line 367**: **Configuration Values** - Uncertain about default spacing values (5,5 vs 10,10)
- **Line 996-997**: **Configuration Design** - Questioning if fallback behavior should be configurable setting
- **Line 1075**: **Layout Logic Uncertainty** - Unsure about applying certain logic to non-1.0 flex children
- **Line 1103**: **Missing Feature - Flexbox** - CSS-style flexbox layout support not yet implemented
- **Line 1119**: **Layout Feature** - Need to handle absolute positioned children properly (currently ignored)
- **Line 1182**: **Layout Constraint Issue** - Cannot enforce certain assertions when text wrapping is enabled

## Example Code TODOs

### UI Component Example (`example/ui_component/main.cpp`)
- **Line 33**: **Type Safety Investigation** - Need to investigate behavior when passing void type to component system

### Custom Queries Example (`example/custom_queries/main.cpp`)
- **Line 39**: **Numerical Precision Tuning** - Need to experiment with epsilon value for floating-point comparisons in custom queries

### Tests Example (`example/tests/main.cpp`)
- **Line 79**: **Incomplete Test** - Test case needs implementation
- **Line 84**: **Incomplete Test** - Test case needs implementation

### UI Layout Example (`example/ui_layout/main.cpp`)
- **Line 168**: **Update Mechanism Missing** - Need to determine how to properly update this UI component
- **Line 805**: **Layout Bounds Question** - Questioning whether child elements should extend beyond parent boundaries

## Documentation TODOs

### README.md
- **Line 17**: **Missing Example** - `for_each_with_derived` function needs usage example in documentation
- **Line 92**: **Incomplete Documentation** - Section needs more content/examples

## Notes and Technical Comments

### Performance & Implementation Notes

#### Bitwise Operations (`src/bitwise.h`)
- **Line 44**: **Implementation Choice** - Deliberately not using certain bitwise operation, using & for boolean operations instead

#### UI Utilities (`src/plugins/ui/utilities.h`)
- **Line 56**: **Compiler Limitation** - Attempted constexpr implementation failed (incomplete note)

#### Input System Implementation (`src/plugins/input_system.h`)
- **Line 438**: **Implementation Complexity** - Complex input checking logic requires validation of multiple conditions
- **Line 441**: **Deadzone Configuration** - Documents that 0.25 is the deadzone size (links to TODO about making it configurable)

#### Entity Helper Design (`src/entity_helper.h`)
- **Line 143**: **API Design Question** - Questioning whether certain functionality should be exposed to users

#### Auto Layout Implementation (`src/plugins/autolayout.h`)
- **Line 1019**: **Implementation Logic** - Explains why early return is avoided in empty cases

---

## Summary by Priority

### 🔴 High Priority (Bugs & Memory Safety)
- **Component ID overflow check broken** (`src/base_component.h:24-28`)
- **Filter bug causing empty results** (`src/plugins/ui/rendering.h:345-346`)
- **Focus ring positioning bug** (`src/plugins/ui/immediate/imm_components.h:128-129`)
- **Key mapping conflict** (`src/plugins/input_system.h:359`)

### 🟡 Medium Priority (Performance & Architecture)
- **Query system memory allocation inefficiency** (`src/entity_query.h:219-222`)
- **Missing rendering cache** (`src/plugins/ui/rendering.h:36`)
- **Circular dependency in UI styling** (`src/plugins/ui/context.h:53`)
- **Const system registration issue** (`src/system.h:159-161`)

### 🟢 Low Priority (Features & Improvements)
- **Flexbox layout support** (`src/plugins/autolayout.h:1103`)
- **Cross-platform controller support** (`src/plugins/input_system.h:114`)
- **Configurable gamepad deadzone** (`src/plugins/input_system.h:443`)
- **API design improvements** (`src/entity_query.h:39-44`)

**Total TODOs Found**: 45  
**Total Notes Found**: 6  
**Critical Issues**: 4  
**Performance Issues**: 6  
**Feature Requests**: 12  
**Last Updated**: December 2024