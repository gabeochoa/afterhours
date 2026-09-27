#pragma once

// ============================================================================
// Public API — Immediate-mode UI components
//
// Custom component authors should compose using these primitives and utilities:
//
// Primitives:
//   div, hstack, vstack, spacer, separator, button, image, sprite,
//   image_button, circular_progress
//
// Composites (built from primitives):
//   icon_row, button_group, checkbox, checkbox_group, radio_group,
//   toggle_switch, slider, stepper, pagination, dropdown, navigation_bar,
//   tab_container, progress_bar, decorative_frame
//
// Initialization (component_init.h):
//   init_component()  — sets up UIComponent, applies config, layout, visuals
//   init_state()      — creates or retrieves per-entity state component
//
// Everything in the detail:: namespace is internal and subject to change.
// ============================================================================

// Split into per-family headers; this file remains the umbrella
// that includes all of them, so existing includes keep working.
#include "imm_primitives.h"
#include "imm_layout.h"
#include "imm_virtual_list.h"
#include "imm_controls.h"
#include "imm_value.h"
#include "imm_containers.h"
