# Test Examples - Critical Fixes Verification

This directory contains comprehensive test examples that verify the critical fixes implemented for the **Component Overflow Protection** and **Logging System** improvements.

## 🚨 Critical Issues Fixed

### Before Our Fix:
```cpp
// TODO this doesnt work for some reason
// if (lastID + 1 > max_num_components)
// log_error(
// "You are trying to add a new component but you have used up all "
// "the space allocated, updated max_num");
```

**Problems:**
- ❌ Component overflow check was **DISABLED** (commented out)
- ❌ `log_error()` was an **empty stub function** that did nothing
- ❌ Potential **memory safety violations** from array bounds overflow
- ❌ **Silent failures** with no error reporting

### After Our Fix:
```cpp
if (lastID >= max_num_components) {
  log_error(
    "You are trying to add a new component but you have used up all "
    "the space allocated (max: %zu), increase AFTER_HOURS_MAX_COMPONENTS", 
    max_num_components);
  // Return the last valid ID to prevent array bounds violations
  return max_num_components - 1;
}
```

**Improvements:**
- ✅ Component overflow check is **ENABLED** and working
- ✅ `log_error()` is **fully functional** with printf-style formatting
- ✅ **Memory safety protected** - IDs clamped to valid bounds
- ✅ **Clear error messages** inform developers of the issue

---

## 📁 Test Examples Overview

### 1. `logging_test/` - Logging System Verification
**Purpose:** Comprehensive test of the newly implemented logging system

**What it tests:**
- ✅ All log levels (`log_info`, `log_warn`, `log_error`)
- ✅ Printf-style formatting with multiple data types
- ✅ Proper output routing (stdout for info/warn, stderr for error)
- ✅ Integration with component system
- ✅ Edge cases and multi-argument formatting

**Key output:**
```
[INFO] This is an info message - logging system is working!
[WARN] This is a warning message - used for non-critical issues
[ERROR] This is an error message - used for serious problems
```

### 2. `component_overflow_test/` - Overflow Protection Verification
**Purpose:** Multi-faceted testing of component overflow protection

**Contains 3 test files:**

#### `main.cpp` - General Component Test
- Tests component registration within normal limits
- Verifies all component IDs are within valid bounds
- Documents the key improvements made

#### `overflow_trigger.cpp` - Specific Overflow Trigger
- Attempts to set low component limits for testing
- Demonstrates system behavior under constrained conditions

#### `force_overflow_test.cpp` - Forced Overflow Demonstration
- **🎯 THE DEFINITIVE TEST** - Directly calls internal ID generator
- Forces the system to exceed the 128 component limit
- **PROVES** overflow protection is working:

```
ID 127: 127
[ERROR] You are trying to add a new component but you have used up all the space allocated (max: 128), increase AFTER_HOURS_MAX_COMPONENTS
ID 128: 127 (overflow zone) ✓ CLAMPED TO MAX VALID ID
[ERROR] You are trying to add a new component but you have used up all the space allocated (max: 128), increase AFTER_HOURS_MAX_COMPONENTS
ID 129: 127 (overflow zone) ✓ CLAMPED TO MAX VALID ID
```

---

## 🔧 How to Run the Tests

### Run All Tests
```bash
cd example
make all
```

### Run Specific Test Categories
```bash
# Test logging system only
cd example/logging_test && make

# Test component overflow protection only
cd example/component_overflow_test && make

# Run just the forced overflow test (most comprehensive)
cd example/component_overflow_test && make force_test
```

---

## ✅ Expected Results

### Logging Test Success Indicators:
- All log levels produce formatted output
- ERROR messages appear on stderr (visible as red text in many terminals)
- INFO/WARN messages appear on stdout
- Component integration works correctly

### Component Overflow Test Success Indicators:
- All component IDs are within valid bounds (`< max_num_components`)
- Error messages are logged when overflow occurs
- IDs are clamped to `max_num_components - 1` during overflow
- System remains stable (no crashes or undefined behavior)

---

## 📊 Impact Summary

| Aspect | Before Fix | After Fix |
|--------|------------|-----------|
| **Memory Safety** | ❌ Potential array bounds violations | ✅ Protected by bounds checking |
| **Error Reporting** | ❌ Silent failures | ✅ Clear error messages |
| **Developer Experience** | ❌ Hard to debug issues | ✅ Informative logging |
| **System Stability** | ❌ Undefined behavior possible | ✅ Graceful degradation |
| **Production Readiness** | ❌ Critical TODO unfixed | ✅ Production-ready |

---

## 🎯 Why These Tests Matter

1. **Memory Safety**: Prevents potential crashes and security vulnerabilities
2. **Developer Productivity**: Clear error messages help identify issues quickly
3. **System Reliability**: Graceful handling of edge cases improves robustness
4. **Maintenance**: Logging enables better debugging and monitoring

These tests serve as **regression tests** to ensure that future changes don't accidentally break these critical safety mechanisms.

---

## 🔖 New Example: `tag_filters/` — Mixing Tag Predicates and Components

Demonstrates how to use `afterhours::tags::All`, `Any`, and `None` inside `System<...>` templates alongside component filters.

- Shows systems that:
  - run on all entities with a component AND a required tag
  - run on entities with any of multiple tags
  - exclude entities with a forbidden tag
  - mix multiple components with tag predicates

How to run:
```bash
cd example/tag_filters && make
```