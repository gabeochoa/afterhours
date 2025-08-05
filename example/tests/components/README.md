# Component AutoLayout Tests

This directory contains comprehensive tests for autolayout functionality with ComponentSize for each component in the ComponentType enum.

## Test Structure

Each component type has its own header file with multiple test cases covering:

### Button Tests (`button_tests.h`)
- **ButtonComponentSizeBasic**: Standard button sizes (140x40px)
- **ButtonComponentSizeVariants**: Small, medium, large, and wide buttons
- **ButtonComponentSizePrecision**: Fractional pixel testing

### ButtonGroup Tests (`button_group_tests.h`)
- **ButtonGroupComponentSizeBasic**: Standard button group (300x40px)
- **ButtonGroupComponentSizeVariants**: Compact, standard, large, and vertical groups
- **ButtonGroupComponentSizeToolbar**: Icon toolbar, full toolbar, FAB groups
- **ButtonGroupComponentSizePrecision**: Fractional pixel testing

### Div Tests (`div_tests.h`)
- **DivComponentSizeBasic**: Standard container div (400x300px)
- **DivComponentSizeContainerVariants**: Small, medium, large, fullscreen containers
- **DivComponentSizeLayoutVariants**: Sidebar, header, footer, modal layouts
- **DivComponentSizeGridAndFlex**: Grid cells, flex items, square divs
- **DivComponentSizePrecision**: Fractional pixel testing

### Slider Tests (`slider_tests.h`)
- **SliderComponentSizeBasic**: Standard horizontal slider (200x25px)
- **SliderComponentSizeHorizontalVariants**: Compact to full-width horizontal sliders
- **SliderComponentSizeVerticalVariants**: Compact to full-height vertical sliders
- **SliderComponentSizeSpecializedTypes**: Volume, progress, zoom, range sliders
- **SliderComponentSizePrecision**: Fractional pixel testing

### Checkbox Tests (`checkbox_tests.h`)
- **CheckboxComponentSizeBasic**: Standard checkbox with label (120x24px)
- **CheckboxComponentSizeVariants**: Small, standard, large, long label variants
- **CheckboxComponentSizeBoxOnly**: Checkbox box sizes without labels
- **CheckboxComponentSizeFormLayouts**: Inline, vertical, grid, settings layouts
- **CheckboxComponentSizePrecision**: Fractional pixel testing

### CheckboxNoLabel Tests (`checkbox_no_label_tests.h`)
- **CheckboxNoLabelComponentSizeBasic**: Standard checkbox box only (20x20px)
- **CheckboxNoLabelComponentSizeVariants**: Tiny to extra-large checkbox boxes
- **CheckboxNoLabelComponentSizeSpecialCases**: List, toolbar, menu, toggle variants
- **CheckboxNoLabelComponentSizeGridArray**: Table, card, thumbnail grid layouts
- **CheckboxNoLabelComponentSizePrecision**: Fractional pixel testing

### Dropdown Tests (`dropdown_tests.h`)
- **DropdownComponentSizeBasic**: Standard dropdown (180x32px)
- **DropdownComponentSizeVariants**: Compact to full-width dropdowns
- **DropdownComponentSizeSpecializedTypes**: Select, combobox, menu, filter variants
- **DropdownComponentSizeFormContexts**: Inline, vertical, grid, settings layouts
- **DropdownComponentSizeAccessibility**: Touch-friendly, high contrast, large text
- **DropdownComponentSizePrecision**: Fractional pixel testing

## Running Tests

### Run All Component Tests
```bash
make
```

### Run Tests by Component Type
```bash
./tests.exe "[Button]"
./tests.exe "[ButtonGroup]"
./tests.exe "[Div]"
./tests.exe "[Slider]"
./tests.exe "[Checkbox]"
./tests.exe "[CheckboxNoLabel]"
./tests.exe "[Dropdown]"
```

### Run Tests by Category
```bash
./tests.exe "[ComponentSize]"     # All ComponentSize tests
./tests.exe "[AutoLayout]"        # All AutoLayout tests
```

### Run Specific Test Cases
```bash
./tests.exe "ButtonComponentSizeBasic"
./tests.exe "DivComponentSizeLayoutVariants"
```

## Test Coverage

- **Total Component Tests**: 66 test cases
- **Total Assertions**: 230+ assertions
- **Components Covered**: 7 (all ComponentType enum values)
- **Test Categories**: Basic, Variants, Specialized, Precision

## What Each Test Validates

Each test validates that:
1. ComponentSize can be created with `pixels()` function
2. The ComponentSize values are correctly applied to UIComponent desired dimensions
3. The autolayout system correctly computes final sizes
4. The computed values exactly match the expected pixel values
5. Fractional pixel values work with appropriate precision

## Test Patterns

All tests follow a consistent pattern:
1. Create AutoLayout instance
2. Create UIComponent with unique ID
3. Create ComponentSize with `pixels()` 
4. Apply ComponentSize to UIComponent's desired sizes
5. Verify desired sizes are set correctly (dimension type and value)
6. Run `al.calculate_standalone()` 
7. Verify computed values match expected pixel values

This ensures comprehensive validation of the autolayout system's ComponentSize integration for all component types.