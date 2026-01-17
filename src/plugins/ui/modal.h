// Modal Dialog Components and Systems for Afterhours
// Gap 09: Modal Dialogs
//
// Usage:
//   // Create modal entity
//   Entity& modal = EntityHelper::createEntity();
//   modal.add<IsModal>({
//     .close_on_backdrop_click = true,
//     .close_on_escape = true
//   });
//   modal.add<DialogState>();
//   
//   // Open modal
//   modal_open(modal);
//   
//   // In render loop, check result:
//   if (dialog_state.result != DialogResult::Pending) {
//     // Handle result
//   }

#pragma once

#include <string>
#include <vector>
#include <functional>

#include "../../ecs.h"
#include "../color.h"

namespace afterhours {
namespace ui {

//=============================================================================
// ENUMS
//=============================================================================

/// Result of a modal dialog interaction
enum class DialogResult {
  Pending,    // Dialog is still open, no decision made
  Confirmed,  // User clicked OK/Yes/Confirm
  Cancelled,  // User clicked Cancel/No
  Dismissed,  // User closed dialog (backdrop click, escape, X button)
  Custom      // Custom result - check custom_result field
};

/// Modal positioning mode
enum class ModalPosition {
  Center,     // Center of screen
  TopCenter,  // Top center with margin
  BottomCenter,
  Custom      // Use custom_x, custom_y
};

/// Modal size mode
enum class ModalSize {
  Auto,       // Size to content
  Small,      // ~300px wide
  Medium,     // ~500px wide
  Large,      // ~700px wide
  FullWidth,  // Full width with margins
  Custom      // Use custom_width, custom_height
};

//=============================================================================
// COMPONENTS
//=============================================================================

/// Tag component indicating this entity is a modal dialog
struct IsModal : BaseComponent {
  // Behavior options
  bool close_on_backdrop_click = true;
  bool close_on_escape = true;
  bool show_close_button = true;
  bool draggable = false;
  
  // Appearance
  uint32_t backdrop_color = 0x00000080;  // Semi-transparent black
  float corner_radius = 4.f;
  
  // Position
  ModalPosition position = ModalPosition::Center;
  float custom_x = 0.f;
  float custom_y = 0.f;
  float margin = 40.f;  // Margin from screen edges
  
  // Size
  ModalSize size = ModalSize::Auto;
  float custom_width = 0.f;
  float custom_height = 0.f;
  float min_width = 200.f;
  float min_height = 100.f;
  float max_width = 0.f;  // 0 = no max
  float max_height = 0.f;
  
  // Internal state
  bool is_dragging = false;
  float drag_offset_x = 0.f;
  float drag_offset_y = 0.f;
};

/// State for dialog interactions
struct DialogState : BaseComponent {
  DialogResult result = DialogResult::Pending;
  int custom_result = 0;  // For DialogResult::Custom
  std::string input_value;  // For input dialogs
  int selected_index = -1;  // For list/choice dialogs
  
  // Reset state for reuse
  void reset() {
    result = DialogResult::Pending;
    custom_result = 0;
    input_value.clear();
    selected_index = -1;
  }
  
  bool is_open() const { return result == DialogResult::Pending; }
  bool is_confirmed() const { return result == DialogResult::Confirmed; }
  bool is_cancelled() const { return result == DialogResult::Cancelled; }
  bool is_dismissed() const { return result == DialogResult::Dismissed; }
};

/// Modal content (title and message)
struct HasModalContent : BaseComponent {
  std::string title;
  std::string message;
  std::string ok_text = "OK";
  std::string cancel_text = "Cancel";
  bool show_cancel = false;
  
  HasModalContent() = default;
  HasModalContent(const std::string& t, const std::string& m)
    : title(t), message(m) {}
  HasModalContent(const std::string& t, const std::string& m, bool cancel)
    : title(t), message(m), show_cancel(cancel) {}
};

/// Modal stack tracking (singleton component)
struct ModalStackState : BaseComponent {
  std::vector<EntityID> modal_stack;  // Stack of open modals (top = last)
  int next_modal_sequence = 1;  // For z-ordering
  
  bool has_modals() const { return !modal_stack.empty(); }
  
  EntityID top_modal() const {
    return modal_stack.empty() ? -1 : modal_stack.back();
  }
  
  void push_modal(EntityID id) {
    modal_stack.push_back(id);
  }
  
  bool pop_modal(EntityID id) {
    for (auto it = modal_stack.begin(); it != modal_stack.end(); ++it) {
      if (*it == id) {
        modal_stack.erase(it);
        return true;
      }
    }
    return false;
  }
  
  bool is_top(EntityID id) const {
    return !modal_stack.empty() && modal_stack.back() == id;
  }
  
  int get_sequence() { return next_modal_sequence++; }
};

/// Modal sequence number for z-ordering
struct HasModalSequence : BaseComponent {
  int sequence = 0;
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

namespace modal {

/// Open a modal dialog
inline void open(Entity& modal_entity) {
  if (!modal_entity.has<IsModal>()) {
    modal_entity.add<IsModal>({});
  }
  if (!modal_entity.has<DialogState>()) {
    modal_entity.add<DialogState>({});
  }
  
  // Reset state
  modal_entity.get<DialogState>().reset();
  
  // Add to modal stack
  auto* stack = EntityHelper::get_singleton_cmp_for_mod<ModalStackState>();
  if (stack) {
    stack->push_modal(modal_entity.id());
    modal_entity.addOrReplace<HasModalSequence>({stack->get_sequence()});
  }
}

/// Close a modal dialog with result
inline void close(Entity& modal_entity, DialogResult result = DialogResult::Dismissed) {
  if (!modal_entity.has<DialogState>()) return;
  
  modal_entity.get<DialogState>().result = result;
  
  // Remove from modal stack
  auto* stack = EntityHelper::get_singleton_cmp_for_mod<ModalStackState>();
  if (stack) {
    stack->pop_modal(modal_entity.id());
  }
}

/// Close with custom result
inline void close_custom(Entity& modal_entity, int custom_result) {
  if (!modal_entity.has<DialogState>()) return;
  
  auto& state = modal_entity.get<DialogState>();
  state.result = DialogResult::Custom;
  state.custom_result = custom_result;
  
  auto* stack = EntityHelper::get_singleton_cmp_for_mod<ModalStackState>();
  if (stack) {
    stack->pop_modal(modal_entity.id());
  }
}

/// Confirm the modal
inline void confirm(Entity& modal_entity) {
  close(modal_entity, DialogResult::Confirmed);
}

/// Cancel the modal
inline void cancel(Entity& modal_entity) {
  close(modal_entity, DialogResult::Cancelled);
}

/// Check if any modal is active
inline bool is_modal_active() {
  auto* stack = EntityHelper::get_singleton_cmp<ModalStackState>();
  return stack && stack->has_modals();
}

/// Get the top modal entity
inline OptEntity get_top_modal() {
  auto* stack = EntityHelper::get_singleton_cmp<ModalStackState>();
  if (!stack || !stack->has_modals()) return {};
  return EntityHelper::getEntityForID(stack->top_modal());
}

/// Check if entity is the top modal
inline bool is_top_modal(EntityID id) {
  auto* stack = EntityHelper::get_singleton_cmp<ModalStackState>();
  return stack && stack->is_top(id);
}

} // namespace modal

//=============================================================================
// CONVENIENCE FUNCTIONS
//=============================================================================

/// Create a simple message box modal
inline Entity& message_box(const std::string& title, const std::string& message) {
  Entity& modal = EntityHelper::createEntity();
  modal.add<IsModal>({
    .close_on_backdrop_click = false,
    .close_on_escape = true,
    .show_close_button = false
  });
  modal.add<DialogState>({});
  modal.add<HasModalContent>({title, message, false});
  modal::open(modal);
  return modal;
}

/// Create a confirmation dialog
inline Entity& confirm_dialog(const std::string& title, const std::string& message,
                              const std::string& ok_text = "OK",
                              const std::string& cancel_text = "Cancel") {
  Entity& modal = EntityHelper::createEntity();
  modal.add<IsModal>({
    .close_on_backdrop_click = false,
    .close_on_escape = true,
    .show_close_button = false
  });
  modal.add<DialogState>({});
  HasModalContent content{title, message, true};
  content.ok_text = ok_text;
  content.cancel_text = cancel_text;
  modal.add<HasModalContent>(std::move(content));
  modal::open(modal);
  return modal;
}

/// Create an input dialog
inline Entity& input_dialog(const std::string& title, const std::string& prompt,
                            const std::string& default_value = "") {
  Entity& modal = EntityHelper::createEntity();
  modal.add<IsModal>({
    .close_on_backdrop_click = false,
    .close_on_escape = true,
    .show_close_button = true
  });
  DialogState state{};
  state.input_value = default_value;
  modal.add<DialogState>(std::move(state));
  modal.add<HasModalContent>({title, prompt, true});
  modal::open(modal);
  return modal;
}

} // namespace ui
} // namespace afterhours
