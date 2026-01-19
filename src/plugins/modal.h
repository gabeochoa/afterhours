#pragma once

#include <cmath>
#include <concepts>
#include <string>
#include <vector>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/entity_query.h"
#include "../core/system.h"
#include "../developer.h"
#include "../drawing_helpers.h"
#include "color.h"
#include "ui/element_result.h"
#include "ui/imm_components.h"
#include "ui/rendering.h"
#include "ui/systems.h"
#include "ui/theme.h"
#include "window_manager.h"

namespace afterhours {

// Concept for InputAction enum types
template <typename T>
concept InputActionLike = std::is_enum_v<T>;

// Dialog result enum
enum class DialogResult {
  Pending,   // Dialog still open
  Confirmed, // OK/Yes/Confirm clicked
  Cancelled, // Cancel/No clicked or Escape pressed
  Dismissed, // Backdrop clicked or X button
  Custom,    // Custom button clicked (for tertiary actions)
};

// ClosedBy modes (following HTML dialog spec)
enum class ClosedBy {
  Any,          // Close on escape OR click outside (light dismiss)
  CloseRequest, // Close on escape only (default for modal)
  None          // Manual close only
};

// Forward declaration
struct modal;

// TODO eventually it would be nice to use ComponentConfig
// Modal configuration
struct ModalConfig {
  ui::Size width = ui::h720(400);
  ui::Size height = ui::h720(200);
  std::string title;
  bool center_on_screen = true;
  ClosedBy closed_by = ClosedBy::CloseRequest;
  bool show_close_button = true;
  Color backdrop_color = {0, 0, 0, 128};
  int render_layer = 1000;

  ModalConfig &with_size(ui::Size w, ui::Size h) {
    width = w;
    height = h;
    return *this;
  }

  ModalConfig &with_title(const std::string &t) {
    title = t;
    return *this;
  }

  ModalConfig &with_closed_by(ClosedBy cb) {
    closed_by = cb;
    return *this;
  }

  ModalConfig &with_show_close_button(bool show) {
    show_close_button = show;
    return *this;
  }

  ModalConfig &with_backdrop_color(Color c) {
    backdrop_color = c;
    return *this;
  }

  ModalConfig &with_render_layer(int layer) {
    render_layer = layer;
    return *this;
  }
};

struct modal : developer::Plugin {

  // Root container for modal stack - singleton
  struct ModalRoot : BaseComponent {
    std::vector<EntityID> modal_stack; // For z-ordering, newest at back
    size_t modal_sequence = 0;         // For unique ordering
  };

  // Component attached to individual modal entities
  struct Modal : BaseComponent {
    bool was_open_last_frame = false;
    DialogResult result = DialogResult::Pending;
    std::string return_value;
    Color backdrop_color = {0, 0, 0, 128};
    ClosedBy closed_by = ClosedBy::CloseRequest;
    bool show_close_button = true;
    size_t open_order = 0;
    EntityID previously_focused_element = -1;
    int render_layer = 1000;
    std::string title;

    // Flag set by systems to signal the modal should close
    // The modal_impl will check this and set open = false
    bool pending_close = false;
    DialogResult pending_close_result = DialogResult::Pending;

    Modal() = default;

    // Initialize modal state when opening
    void open_with(const ModalConfig &config, EntityID focus_to_restore) {
      result = DialogResult::Pending;
      backdrop_color = config.backdrop_color;
      closed_by = config.closed_by;
      show_close_button = config.show_close_button;
      render_layer = config.render_layer;
      title = config.title;
      previously_focused_element = focus_to_restore;
      pending_close = false;
      pending_close_result = DialogResult::Pending;
    }

    // Request the modal to close (used by systems that don't have access to
    // open&)
    void request_close(DialogResult close_result = DialogResult::Dismissed) {
      pending_close = true;
      pending_close_result = close_result;
    }
  };

  // Result type for modals that includes DialogResult
  struct ModalResult {
    ui::imm::ElementResult element;
    DialogResult dialog_result = DialogResult::Pending;

    ModalResult(ui::imm::ElementResult el,
                DialogResult dr = DialogResult::Pending)
        : element(el), dialog_result(dr) {}

    // Convenience accessors
    operator bool() const { return element; }
    Entity &ent() const { return element.ent(); }
    EntityID id() const { return element.id(); }
    ui::UIComponent &cmp() const { return element.cmp(); }

    // Check dialog result
    bool confirmed() const { return dialog_result == DialogResult::Confirmed; }
    bool cancelled() const { return dialog_result == DialogResult::Cancelled; }
    bool dismissed() const { return dialog_result == DialogResult::Dismissed; }
    DialogResult result() const { return dialog_result; }
  };

  struct detail {
    static void init_singleton(Entity &singleton) {
      singleton.addComponent<ModalRoot>();
      EntityHelper::registerSingleton<ModalRoot>(singleton);
    }

    static ModalRoot &get_modal_root() {
      auto *root = EntityHelper::get_singleton_cmp<ModalRoot>();
      if (!root) {
        log_error("ModalRoot singleton not found");
        throw std::runtime_error("ModalRoot not initialized. Call "
                                 "modal::enforce_singletons() first.");
      }
      return *root;
    }

    static bool is_entity_in_tree(EntityID root_id, EntityID search_id) {
      if (root_id == search_id)
        return true;

      OptEntity opt = EntityHelper::getEntityForID(root_id);
      if (!opt.has_value())
        return false;

      Entity &entity = opt.asE();
      if (!entity.has<ui::UIComponent>())
        return false;

      const ui::UIComponent &cmp = entity.get<ui::UIComponent>();
      for (EntityID child_id : cmp.children) {
        if (is_entity_in_tree(child_id, search_id))
          return true;
      }
      return false;
    }

    static float resolve_size(const ui::Size &size, int screen_w,
                              int screen_h) {
      switch (size.dim) {
      case ui::Dim::Pixels:
        return size.value;
      case ui::Dim::ScreenPercent:
        return size.value * static_cast<float>(screen_h);
      case ui::Dim::Percent:
        return size.value * static_cast<float>(screen_w);
      default:
        return size.value;
      }
    }

    // Internal implementation for creating a modal
    static ui::imm::ElementResult modal_impl(ui::imm::HasUIContext auto &ctx,
                                             ui::imm::EntityParent ep_pair,
                                             bool &open, ModalConfig config) {
      using namespace ui;
      using namespace ui::imm;

      auto [entity, parent] = deref(ep_pair);

      // Initialize Modal component if needed
      Modal &m = entity.template addComponentIfMissing<Modal>();

      // Check if a system requested this modal to close
      if (m.pending_close) {
        m.result = m.pending_close_result;
        m.pending_close = false;
        m.pending_close_result = DialogResult::Pending;
        open = false; // This modifies the user's bool reference
      }

      // Handle state transitions
      bool was_open = m.was_open_last_frame;
      bool is_open = open;

      if (is_open && !was_open) {
        // Modal just opened
        m.open_with(config, ctx.focus_id);

        // Add to modal stack
        auto &root = get_modal_root();
        m.open_order = root.modal_sequence++;
        root.modal_stack.push_back(entity.id);
      } else if (!is_open && was_open) {
        // Modal just closed
        auto &root = get_modal_root();
        auto it = std::find(root.modal_stack.begin(), root.modal_stack.end(),
                            entity.id);
        if (it != root.modal_stack.end()) {
          root.modal_stack.erase(it);
        }

        // Restore focus
        if (m.previously_focused_element >= 0) {
          ctx.focus_id = m.previously_focused_element;
        }
      }

      m.was_open_last_frame = is_open;

      // Always get/create the backdrop entity so we can manage its visibility
      EntityID backdrop_id = entity.id + 1000000;
      auto backdrop_ep = mk(parent, backdrop_id);
      auto [backdrop_entity, backdrop_parent] = deref(backdrop_ep);

      if (!is_open) {
        // Add ShouldHide to modal and backdrop when not open
        entity.template addComponentIfMissing<ShouldHide>();
        backdrop_entity.template addComponentIfMissing<ShouldHide>();
        return {false, entity};
      }

      // Remove ShouldHide when open
      entity.template removeComponentIfExists<ShouldHide>();
      backdrop_entity.template removeComponentIfExists<ShouldHide>();

      // Get screen dimensions for centering
      auto *res = EntityHelper::get_singleton_cmp<
          window_manager::ProvidesCurrentResolution>();
      int screen_w = res ? res->current_resolution.width : 1280;
      int screen_h = res ? res->current_resolution.height : 720;

      float width_px = resolve_size(config.width, screen_w, screen_h);
      float height_px = resolve_size(config.height, screen_w, screen_h);

      float x_pos = (static_cast<float>(screen_w) - width_px) / 2.0f;
      float y_pos = (static_cast<float>(screen_h) - height_px) / 2.0f;

      // Create full-screen backdrop overlay as a SIBLING of the modal (child of
      // parent). This ensures it renders before the modal and blocks input.
      // We use a div (not button) to avoid hover state color changes.
      // Visual backdrop (div) - this provides the dimmed background appearance
      auto backdrop_visual =
          div(ctx, backdrop_ep,
              ComponentConfig{}
                  .with_size(ComponentSize{pixels(static_cast<float>(screen_w)),
                                           pixels(static_cast<float>(screen_h))})
                  .with_absolute_position()
                  .with_custom_background(config.backdrop_color)
                  .with_render_layer(config.render_layer - 1)
                  .with_debug_name("modal_backdrop"));
      backdrop_visual.cmp().computed_rel[Axis::X] = 0;
      backdrop_visual.cmp().computed_rel[Axis::Y] = 0;

      // Note: Backdrop clicks for light dismiss are handled by
      // ModalCloseWatcherSystem which checks if clicks are outside the modal
      // and sets pending_close. We check pending_close at the start of this
      // function.

      // Create modal container
      _init_component(
          ctx, ep_pair,
          // TODO add support for configuration
          ComponentConfig{}
              .with_size(ComponentSize{pixels(width_px), pixels(height_px)})
              .with_absolute_position()
              .with_flex_direction(FlexDirection::Column)
              .with_background(Theme::Usage::Surface)
              .with_roundness(0.05f)
              .with_padding(Spacing::md)
              .with_render_layer(config.render_layer)
              .with_debug_name("modal"),
          ComponentType::Div);

      // Set absolute position manually
      auto &ui = entity.get<UIComponent>();
      ui.computed_rel[Axis::X] = x_pos;
      ui.computed_rel[Axis::Y] = y_pos;

      // Add title if specified
      if (!config.title.empty()) {
        auto header =
            div(ctx, mk(entity, 0),
                ComponentConfig{}
                    .with_size(ComponentSize{percent(1.0f), h720(36)})
                    .with_flex_direction(FlexDirection::Row)
                    .with_justify_content(JustifyContent::SpaceBetween)
                    .with_align_items(AlignItems::Center)
                    .with_margin(Margin{.bottom = DefaultSpacing::small()})
                    .with_render_layer(config.render_layer + 1)
                    .with_debug_name("modal_header"));

        div(ctx, mk(header.ent(), 0),
            ComponentConfig{}
                .with_label(config.title)
                .with_size(ComponentSize{children(), percent(1.0f)})
                .with_font(UIComponent::DEFAULT_FONT, 18.0f)
                .with_auto_text_color(true)
                .with_render_layer(config.render_layer + 1)
                .with_debug_name("modal_title"));

        if (config.show_close_button) {
          if (button(ctx, mk(header.ent(), 1),
                     ComponentConfig{}
                         .with_label("X")
                         .with_size(ComponentSize{h720(28), h720(28)})
                         .with_render_layer(config.render_layer + 1)
                         .with_debug_name("modal_close"))) {
            m.result = DialogResult::Dismissed;
            open = false;
          }
        }
      }

      // Queue for render at high layer
      ctx.queue_render(RenderInfo{entity.id, config.render_layer});

      return {true, entity};
    }

    // Helper to create info dialog content
    static ui::imm::ElementResult
    create_info_content(ui::imm::HasUIContext auto &ctx,
                        ui::imm::EntityParent ep_pair, bool &open,
                        const std::string &title, const std::string &message,
                        const std::string &button_label) {
      using namespace ui;
      using namespace ui::imm;

      constexpr int CONTENT_LAYER = 1001;

      auto result = modal_impl(ctx, ep_pair, open,
                               ModalConfig{}
                                   .with_size(h720(350), h720(150))
                                   .with_title(title)
                                   .with_show_close_button(false));

      if (result) {
        div(ctx, mk(result.ent(), 0),
            ComponentConfig{}
                .with_label(message)
                .with_size(ComponentSize{percent(1.0f), children()})
                .with_padding(Spacing::md)
                .with_render_layer(CONTENT_LAYER));

        auto button_row =
            div(ctx, mk(result.ent(), 1),
                ComponentConfig{}
                    .with_size(ComponentSize{percent(1.0f), h720(44)})
                    .with_flex_direction(FlexDirection::Row)
                    .with_justify_content(JustifyContent::Center)
                    .with_align_items(AlignItems::Center)
                    .with_flex_wrap(FlexWrap::NoWrap)
                    .with_render_layer(CONTENT_LAYER));

        if (button(ctx, mk(button_row.ent(), 0),
                   ComponentConfig{}
                       .with_label(button_label)
                       .with_size(ComponentSize{h720(100), h720(36)})
                       .with_render_layer(CONTENT_LAYER))) {
          open = false;
        }
      }

      return result;
    }

    // Helper to create confirm dialog content
    static ModalResult create_confirm_content(
        ui::imm::HasUIContext auto &ctx, ui::imm::EntityParent ep_pair,
        bool &open, const std::string &title, const std::string &message,
        const std::string &confirm_label, const std::string &cancel_label) {
      using namespace ui;
      using namespace ui::imm;

      constexpr int CONTENT_LAYER = 1001;
      DialogResult dialog_result = DialogResult::Pending;

      auto result = modal_impl(ctx, ep_pair, open,
                               ModalConfig{}
                                   .with_size(h720(400), h720(180))
                                   .with_title(title)
                                   .with_show_close_button(false));

      if (result) {
        div(ctx, mk(result.ent(), 0),
            ComponentConfig{}
                .with_label(message)
                .with_size(ComponentSize{percent(1.0f), children()})
                .with_padding(Spacing::md)
                .with_render_layer(CONTENT_LAYER));

        auto button_row =
            div(ctx, mk(result.ent(), 1),
                ComponentConfig{}
                    .with_size(ComponentSize{percent(1.0f), h720(44)})
                    .with_flex_direction(FlexDirection::Row)
                    .with_justify_content(JustifyContent::Center)
                    .with_align_items(AlignItems::Center)
                    .with_flex_wrap(FlexWrap::NoWrap)
                    .with_render_layer(CONTENT_LAYER));

        if (button(ctx, mk(button_row.ent(), 0),
                   ComponentConfig{}
                       .with_label(confirm_label)
                       .with_size(ComponentSize{h720(100), h720(36)})
                       .with_background(Theme::Usage::Primary)
                       .with_margin(
                           Margin{.left = DefaultSpacing::small(),
                                  .right = DefaultSpacing::small()})
                       .with_render_layer(CONTENT_LAYER))) {
          dialog_result = DialogResult::Confirmed;
          open = false;
        }

        if (button(ctx, mk(button_row.ent(), 1),
                   ComponentConfig{}
                       .with_label(cancel_label)
                       .with_size(ComponentSize{h720(100), h720(36)})
                       .with_margin(
                           Margin{.left = DefaultSpacing::small(),
                                  .right = DefaultSpacing::small()})
                       .with_render_layer(CONTENT_LAYER))) {
          dialog_result = DialogResult::Cancelled;
          open = false;
        }
      }

      // Check if modal was closed by escape/backdrop
      if (!open && result.ent().template has<Modal>()) {
        Modal &m = result.ent().template get<Modal>();
        if (m.result != DialogResult::Pending) {
          dialog_result = m.result;
        }
      }

      return {result, dialog_result};
    }

    // Helper to create fyi dialog content
    static ModalResult create_fyi_content(ui::imm::HasUIContext auto &ctx,
                                          ui::imm::EntityParent ep_pair,
                                          bool &open, const std::string &title,
                                          const std::string &message,
                                          const std::string &primary_label,
                                          const std::string &dismiss_label,
                                          const std::string &tertiary_label) {
      using namespace ui;
      using namespace ui::imm;

      constexpr int CONTENT_LAYER = 1001;
      DialogResult dialog_result = DialogResult::Pending;

      auto result = modal_impl(ctx, ep_pair, open,
                               ModalConfig{}
                                   .with_size(h720(420), h720(200))
                                   .with_title(title)
                                   .with_show_close_button(false));

      if (result) {
        div(ctx, mk(result.ent(), 0),
            ComponentConfig{}
                .with_label(message)
                .with_size(ComponentSize{percent(1.0f), children()})
                .with_padding(Spacing::md)
                .with_render_layer(CONTENT_LAYER));

        auto button_row =
            div(ctx, mk(result.ent(), 1),
                ComponentConfig{}
                    .with_size(ComponentSize{percent(1.0f), h720(44)})
                    .with_flex_direction(FlexDirection::Row)
                    .with_justify_content(JustifyContent::Center)
                    .with_align_items(AlignItems::Center)
                    .with_flex_wrap(FlexWrap::NoWrap)
                    .with_render_layer(CONTENT_LAYER));

        if (button(ctx, mk(button_row.ent(), 0),
                   ComponentConfig{}
                       .with_label(primary_label)
                       .with_size(ComponentSize{h720(110), h720(36)})
                       .with_background(Theme::Usage::Primary)
                       .with_margin(
                           Margin{.left = DefaultSpacing::small(),
                                  .right = DefaultSpacing::small()})
                       .with_render_layer(CONTENT_LAYER))) {
          dialog_result = DialogResult::Confirmed;
          open = false;
        }

        if (button(ctx, mk(button_row.ent(), 1),
                   ComponentConfig{}
                       .with_label(dismiss_label)
                       .with_size(ComponentSize{h720(100), h720(36)})
                       .with_margin(
                           Margin{.left = DefaultSpacing::small(),
                                  .right = DefaultSpacing::small()})
                       .with_render_layer(CONTENT_LAYER))) {
          dialog_result = DialogResult::Cancelled;
          open = false;
        }

        if (!tertiary_label.empty()) {
          if (button(ctx, mk(button_row.ent(), 2),
                     ComponentConfig{}
                         .with_label(tertiary_label)
                         .with_size(ComponentSize{h720(100), h720(36)})
                         .with_margin(
                             Margin{.left = DefaultSpacing::small(),
                                    .right = DefaultSpacing::small()})
                         .with_render_layer(CONTENT_LAYER))) {
            dialog_result = DialogResult::Custom;
            open = false;
          }
        }
      }

      // Check if modal was closed by escape/backdrop
      if (!open && result.ent().template has<Modal>()) {
        Modal &m = result.ent().template get<Modal>();
        if (m.result != DialogResult::Pending) {
          dialog_result = m.result;
        }
      }

      return {result, dialog_result};
    }
  }; // struct detail

  // Check if any modal is currently active
  static bool is_active() {
    auto &root = detail::get_modal_root();
    return !root.modal_stack.empty();
  }

  // Get the topmost modal entity ID
  // Returns -1 if no modals are open
  static EntityID top_modal() {
    auto &root = detail::get_modal_root();
    if (root.modal_stack.empty()) {
      return -1;
    }
    return root.modal_stack.back();
  }

  // Check if an entity should receive input (is in topmost modal or no modal
  // active)
  static bool should_receive_input(EntityID entity_id) {
    auto &root = detail::get_modal_root();
    if (root.modal_stack.empty())
      return true;

    EntityID top = root.modal_stack.back();
    return detail::is_entity_in_tree(top, entity_id);
  }

  // Close a modal programmatically
  // This sets a pending_close flag that modal_impl will check
  static void close(EntityID modal_id,
                    DialogResult result = DialogResult::Dismissed,
                    const std::string &return_value = "") {
    OptEntity opt = EntityHelper::getEntityForID(modal_id);
    if (!opt.has_value())
      return;

    Entity &entity = opt.asE();
    if (!entity.has<Modal>())
      return;

    Modal &m = entity.get<Modal>();
    m.return_value = return_value;
    m.request_close(result);
    // Note: modal_impl will handle stack removal when it processes the close
  }

  // System to handle escape key and backdrop clicks for modal closing
  template <InputActionLike InputAction>
  struct ModalCloseWatcherSystem : ui::SystemWithUIContext<Modal> {
    ui::UIContext<InputAction> *context = nullptr;
    input::MousePosition press_pos{};
    bool modal_active_on_press = false;

    virtual void once(float) override {
      context = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
      if (!context)
        return;

      auto &root = detail::get_modal_root();
      if (root.modal_stack.empty())
        return;

      // Track mouse press state for backdrop click detection
      if (context->mouse.just_pressed) {
        press_pos = context->mouse.pos;
        modal_active_on_press = !root.modal_stack.empty();
      }

      // Handle escape key for topmost modal
      EntityID top_id = root.modal_stack.back();
      OptEntity top_opt = EntityHelper::getEntityForID(top_id);
      if (!top_opt.has_value())
        return;

      Entity &top_entity = top_opt.asE();
      if (!top_entity.has<Modal>())
        return;

      Modal &top_modal = top_entity.get<Modal>();

      // Check for escape key (via InputAction if available)
      if constexpr (magic_enum::enum_contains<InputAction>("MenuBack")) {
        if (context->pressed(InputAction::MenuBack)) {
          if (top_modal.closed_by == ClosedBy::CloseRequest ||
              top_modal.closed_by == ClosedBy::Any) {
            modal::close(top_id, DialogResult::Cancelled);
          }
        }
      }

      // Handle backdrop click for light dismiss
      if (top_modal.closed_by == ClosedBy::Any && modal_active_on_press &&
          context->mouse.just_released) {
        // Check if click was outside the modal
        if (!ui::is_point_inside_entity_tree(top_id, press_pos) &&
            !ui::is_point_inside_entity_tree(top_id, context->mouse.pos)) {
          modal::close(top_id, DialogResult::Dismissed);
        }
      }
    }

    virtual void for_each_with(Entity &, ui::UIComponent &, Modal &,
                               float) override {
      // All work done in once()
    }
  };

  // System to block input to elements outside the topmost modal
  template <InputActionLike InputAction>
  struct ModalInputBlockSystem : ui::SystemWithUIContext<> {
    ui::UIContext<InputAction> *context = nullptr;
    static constexpr const char *GATE_NAME = "modal";

    virtual void once(float) override {
      context = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
      if (!context)
        return;

      auto &root = detail::get_modal_root();

      // If no modal is active, remove the input gate
      if (root.modal_stack.empty()) {
        context->remove_input_gate(GATE_NAME);
        return;
      }

      EntityID top_id = root.modal_stack.back();

      // Add/update input gate to block input to elements outside the topmost
      // modal. This is checked in active_if_mouse_inside() before setting
      // hot/active.
      context->add_input_gate(GATE_NAME, [top_id](EntityID id) {
        // Always allow input to the ROOT
        if (id == -1)
          return true;
        // Check if this entity is inside the modal tree
        return detail::is_entity_in_tree(top_id, id);
      });

      // Also clear any existing hot/active that's outside the modal
      if (context->hot_id != context->ROOT &&
          !detail::is_entity_in_tree(top_id, context->hot_id)) {
        context->hot_id = context->ROOT;
      }

      if (context->active_id != context->ROOT &&
          context->active_id != context->FAKE &&
          !detail::is_entity_in_tree(top_id, context->active_id)) {
        context->active_id = context->ROOT;
      }
    }

    virtual void for_each_with(Entity &, ui::UIComponent &, float) override {
      // All work done in once()
    }
  };

  // System to render backdrop for modals
  template <InputActionLike InputAction>
  struct ModalBackdropRenderSystem : System<> {
    virtual void once(float) override {
      auto &root = detail::get_modal_root();
      if (root.modal_stack.empty())
        return;

      auto *res = EntityHelper::get_singleton_cmp<
          window_manager::ProvidesCurrentResolution>();
      if (!res)
        return;

      int screen_w = res->current_resolution.width;
      int screen_h = res->current_resolution.height;

      // Draw backdrop for each modal in stack
      for (EntityID modal_id : root.modal_stack) {
        OptEntity opt = EntityHelper::getEntityForID(modal_id);
        if (!opt.has_value())
          continue;

        Entity &entity = opt.asE();
        if (!entity.has<Modal>())
          continue;

        Modal &m = entity.get<Modal>();

        // Draw full-screen backdrop
        draw_rectangle(
            {0, 0, static_cast<float>(screen_w), static_cast<float>(screen_h)},
            m.backdrop_color);
      }
    }
  };

  // System to trap focus within the topmost modal
  template <InputActionLike InputAction>
  struct ModalFocusTrapSystem : ui::SystemWithUIContext<> {
    ui::UIContext<InputAction> *context = nullptr;

    virtual void once(float) override {
      context = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
      if (!context)
        return;

      auto &root = detail::get_modal_root();
      if (root.modal_stack.empty())
        return;

      EntityID top_id = root.modal_stack.back();

      // If focus is outside the modal, try to move it inside
      if (context->focus_id != context->ROOT &&
          !detail::is_entity_in_tree(top_id, context->focus_id)) {
        // Reset focus to ROOT so the modal can grab it
        context->focus_id = context->ROOT;
      }
    }

    virtual void for_each_with(Entity &, ui::UIComponent &, float) override {
      // All work done in once()
    }
  };

  // ============================================================
  // Registration (Public)
  // ============================================================

  static void enforce_singletons(SystemManager &) {
    // Check directly without going through get_modal_root() which throws
    auto *existing = EntityHelper::get_singleton_cmp<ModalRoot>();
    if (!existing) {
      Entity &singleton = EntityHelper::createEntity();
      detail::init_singleton(singleton);
    }
  }

  template <InputActionLike InputAction>
  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<ModalCloseWatcherSystem<InputAction>>());
    sm.register_update_system(
        std::make_unique<ModalInputBlockSystem<InputAction>>());
    sm.register_update_system(
        std::make_unique<ModalFocusTrapSystem<InputAction>>());
  }

  template <InputActionLike InputAction>
  static void register_render_systems(SystemManager &sm) {
    sm.register_render_system(
        std::make_unique<ModalBackdropRenderSystem<InputAction>>());
  }

  // ============================================================
  // Convenience Helpers (Public)
  // ============================================================

  // modal::info - Single button acknowledgment
  static ui::imm::ElementResult info(ui::imm::HasUIContext auto &ctx,
                                     ui::imm::EntityParent ep_pair, bool &open,
                                     const std::string &title,
                                     const std::string &message,
                                     const std::string &button_label = "OK") {
    return detail::create_info_content(ctx, ep_pair, open, title, message,
                                       button_label);
  }

  // modal::confirm - Two button confirmation
  // Returns ModalResult with dialog_result set to Confirmed/Cancelled
  static ModalResult confirm(ui::imm::HasUIContext auto &ctx,
                             ui::imm::EntityParent ep_pair, bool &open,
                             const std::string &title,
                             const std::string &message,
                             const std::string &confirm_label = "OK",
                             const std::string &cancel_label = "Cancel") {
    return detail::create_confirm_content(ctx, ep_pair, open, title, message,
                                          confirm_label, cancel_label);
  }

  // modal::fyi - Three button with tertiary option
  // Returns ModalResult with dialog_result set to Confirmed/Cancelled/Custom
  static ModalResult fyi(ui::imm::HasUIContext auto &ctx,
                         ui::imm::EntityParent ep_pair, bool &open,
                         const std::string &title, const std::string &message,
                         const std::string &primary_label,
                         const std::string &dismiss_label,
                         const std::string &tertiary_label = "") {
    return detail::create_fyi_content(ctx, ep_pair, open, title, message,
                                      primary_label, dismiss_label,
                                      tertiary_label);
  }
};

// Free function for cleaner API: modal(ctx, ep, open, config)
inline ui::imm::ElementResult modal(ui::imm::HasUIContext auto &ctx,
                                    ui::imm::EntityParent ep_pair, bool &open,
                                    ModalConfig config = ModalConfig()) {
  return modal::detail::modal_impl(ctx, ep_pair, open, config);
}

// Overload that returns ModalResult with DialogResult included
inline modal::ModalResult
modal_with_result(ui::imm::HasUIContext auto &ctx,
                  ui::imm::EntityParent ep_pair, bool &open,
                  ModalConfig config = ModalConfig()) {
  auto result = modal::detail::modal_impl(ctx, ep_pair, open, config);

  DialogResult dialog_result = DialogResult::Pending;
  if (!open && result.ent().template has<modal::Modal>()) {
    modal::Modal &m = result.ent().template get<modal::Modal>();
    if (m.result != DialogResult::Pending) {
      dialog_result = m.result;
    }
  }

  return {result, dialog_result};
}

} // namespace afterhours
