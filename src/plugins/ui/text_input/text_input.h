#pragma once

/// Text Input Plugin
///
/// Provides text input components and utilities for building text editing UIs.
///
/// Organization:
/// - storage.h          - TextStorage concept and StringStorage implementation
/// - state.h            - HasTextInputState and related component types
/// - utils.h            - UTF-8 utilities, cursor manipulation, word navigation
/// - selection.h        - TextSelection for multi-line/selection support
/// - line_index.h       - LineIndex for offset/row/column mapping
/// - text_layout.h      - TextLayoutCache for word wrap and visual line mapping
/// - systems.h          - ECS systems for text editing (blink, line index, selection)
/// - component.h        - The text_input() immediate-mode widget
/// - text_area_state.h  - HasTextAreaState for multiline text editing
/// - text_area.h        - The text_area() immediate-mode widget (multiline)
///
/// Usage:
/// ```cpp
/// #include <afterhours/src/plugins/ui/text_input/text_input.h>
///
/// // Single-line text input
/// std::string username;
/// if (afterhours::text_input::text_input(ctx, mk(parent), username,
///     ComponentConfig{}.with_label("Username"))) {
///   // Text changed
/// }
///
/// // Multiline text area
/// std::string message;
/// if (afterhours::text_input::text_area(ctx, mk(parent), message,
///     ComponentConfig{}
///         .with_size(ComponentSize{pixels(300), pixels(100)})
///         .with_line_height(pixels(18))
///         .with_max_lines(5))) {
///   // Text changed
/// }
/// ```
///
/// Or use the backward-compatible ui::imm::text_input() / ui::imm::text_area().

#include "storage.h"
#include "state.h"
#include "utils.h"
#include "selection.h"
#include "line_index.h"
#include "text_layout.h"
#include "systems.h"
#include "component.h"
#include "text_area_state.h"
#include "text_area.h"

