#pragma once

#include <cmath>
#include <string>

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
#include "ui/theme.h"
#include "window_manager.h"

namespace afterhours {

struct toast : developer::Plugin {

  enum class Level { Info, Success, Warning, Error, Custom };

  // Resolution-scaled sizes (designed for 720p, scales proportionally)
  static inline ui::Size WIDTH = ui::h720(380.0f);
  static inline ui::Size HEIGHT = ui::h720(50.0f);
  static inline ui::Size PADDING = ui::h720(16.0f);
  static inline ui::Size TOAST_GAP = ui::h720(8.0f);

  // Helper to resolve a Size to actual pixels given screen dimensions
  static float resolve_size(const ui::Size &size, int screen_w, int screen_h) {
    switch (size.dim) {
    case ui::Dim::Pixels:
      return size.value;
    case ui::Dim::ScreenPercent:
      // Use height for vertical scaling (h720 pattern)
      return size.value * static_cast<float>(screen_h);
    case ui::Dim::Percent:
      return size.value * static_cast<float>(screen_w);
    case ui::Dim::None:
    case ui::Dim::Text:
    case ui::Dim::Children:
    default:
      return size.value;
    }
  }

  // Root container for all toasts - singleton
  struct ToastRoot : BaseComponent {
    EntityID entity_id = -1;
  };

  // Component attached to individual toast entities
  struct Toast : BaseComponent {
    std::string message;
    Level level = Level::Info;
    Color custom_color = {100, 100, 100, 255};
    float duration = 3.0f;
    float elapsed = 0.0f;
    bool dismissed = false;

    Toast() = default;
    Toast(Level lvl, float dur, Color color = {100, 100, 100, 255})
        : level(lvl), custom_color(color), duration(dur) {}

    [[nodiscard]] float progress() const {
      if (duration <= 0.0f)
        return 0.0f;
      return 1.0f - std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    [[nodiscard]] bool is_expired() const {
      return dismissed || elapsed >= duration;
    }

    void dismiss() { dismissed = true; }
  };

  struct detail {
    [[nodiscard]] static float ease_out_expo(float t) {
      return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    }

    [[nodiscard]] static std::string icon_for_level(Level level) {
      switch (level) {
      case Level::Success:
        return "[OK]";
      case Level::Warning:
        return "[!]";
      case Level::Error:
        return "[X]";
      case Level::Custom:
        return "[*]";
      case Level::Info:
      default:
        return "[i]";
      }
    }

    static void init_singleton(Entity &singleton) {
      Entity &root_entity = EntityHelper::createEntity();
      root_entity.addComponent<ui::UIComponent>(root_entity.id);
      root_entity.get<ui::UIComponent>().make_absolute();

      singleton.addComponent<ToastRoot>();
      singleton.get<ToastRoot>().entity_id = root_entity.id;
      EntityHelper::registerSingleton<ToastRoot>(singleton);
    }

    static ui::imm::ElementResult
    create_simple_toast(ui::imm::HasUIContext auto &ctx, const std::string &msg,
                        Level level, float duration,
                        Color custom_color = {100, 100, 100, 255}) {
      ui::imm::ElementResult result =
          schedule(ctx, level, duration, custom_color);

      // Set the label with icon and message
      std::string icon = icon_for_level(level);
      result.ent().get<ui::HasLabel>().label = icon + " " + msg;

      return result;
    }
  };

  // Get the toast root entity (for custom toast composition)
  static Entity &get_root() {
    auto *root = EntityHelper::get_singleton_cmp<ToastRoot>();
    if (root && root->entity_id >= 0) {
      auto opt = EntityHelper::getEntityForID(root->entity_id);
      if (opt.has_value()) {
        return opt.asE();
      }
    }
    throw std::runtime_error("Toast root not initialized. Call "
                             "toast::add_singleton_components() first.");
  }

  // Create a toast as an independent entity (not part of IMM lifecycle)
  // Returns ElementResult where result is true when toast is visible
  // Note: Toasts are created once and managed by ToastUpdateSystem, they don't
  // follow the IMM pattern of being recreated every frame.
  static ui::imm::ElementResult schedule(ui::imm::HasUIContext auto &ctx,
                                         Level level = Level::Info,
                                         float duration = 3.0f,
                                         Color custom_color = {100, 100, 100,
                                                               255}) {
    using namespace ui;

    // Create independent entity (not through IMM mk())
    Entity &entity = EntityHelper::createEntity();

    // Determine background color
    Color bg_color = [&]() {
      if (level == Level::Custom) {
        return custom_color;
      }
      switch (level) {
      case Level::Success:
        return ctx.theme.secondary;
      case Level::Warning:
        return ctx.theme.accent;
      case Level::Error:
        return ctx.theme.error;
      case Level::Custom:
      case Level::Info:
      default:
        return ctx.theme.primary;
      }
    }();

    // Add UI component manually (not through IMM)
    entity.addComponent<UIComponent>(entity.id);
    auto &ui = entity.get<UIComponent>();
    ui.make_absolute();
    ui.flex_direction = FlexDirection::Row;

    // Get screen resolution to resolve sizes
    auto *res = EntityHelper::get_singleton_cmp<
        window_manager::ProvidesCurrentResolution>();
    if (res) {
      int screen_w = res->current_resolution.width;
      int screen_h = res->current_resolution.height;
      ui.computed[Axis::X] = resolve_size(WIDTH, screen_w, screen_h);
      ui.computed[Axis::Y] = resolve_size(HEIGHT, screen_w, screen_h);
    }

    entity.addComponent<HasColor>(bg_color);
    entity.addComponent<ui::HasRoundedCorners>()
        .set(std::bitset<4>().set())
        .set_roundness(0.15f);
    entity.addComponent<Toast>(Toast(level, duration, bg_color));
    entity.addComponent<ui::HasLabel>("");
    entity.addComponent<ui::UIComponentDebug>("toast");

    // Note: Don't queue for render here - ToastLayoutSystem handles rendering
    // after positioning to avoid flash at (0,0)

    // Toast is visible when not expired
    Toast &t = entity.get<Toast>();
    bool is_visible = !t.is_expired();

    return {is_visible, entity};
  }

  static ui::imm::ElementResult send_info(ui::imm::HasUIContext auto &ctx,
                                          const std::string &msg,
                                          float duration = 3.0f) {
    return detail::create_simple_toast(ctx, msg, Level::Info, duration);
  }

  static ui::imm::ElementResult send_success(ui::imm::HasUIContext auto &ctx,
                                             const std::string &msg,
                                             float duration = 3.0f) {
    return detail::create_simple_toast(ctx, msg, Level::Success, duration);
  }

  static ui::imm::ElementResult send_warning(ui::imm::HasUIContext auto &ctx,
                                             const std::string &msg,
                                             float duration = 5.0f) {
    return detail::create_simple_toast(ctx, msg, Level::Warning, duration);
  }

  static ui::imm::ElementResult send_error(ui::imm::HasUIContext auto &ctx,
                                           const std::string &msg,
                                           float duration = 7.0f) {
    return detail::create_simple_toast(ctx, msg, Level::Error, duration);
  }

  static ui::imm::ElementResult send_custom(ui::imm::HasUIContext auto &ctx,
                                            const std::string &msg, Color color,
                                            float duration = 3.0f) {
    return detail::create_simple_toast(ctx, msg, Level::Custom, duration,
                                       color);
  }

  struct ToastUpdateSystem : System<Toast> {
    void for_each_with(Entity &entity, Toast &t, float dt) override {
      t.elapsed += dt;
      if (t.is_expired()) {
        entity.cleanup = true;
      }
    }
  };

  template <typename InputAction>
  struct ToastLayoutSystem : System<> {
    void once(float) override {
      auto *res = EntityHelper::get_singleton_cmp<
          window_manager::ProvidesCurrentResolution>();
      if (!res)
        return;

      int screen_w = res->current_resolution.width;
      int screen_h = res->current_resolution.height;

      // Resolve sizes for current resolution
      float width_px = resolve_size(WIDTH, screen_w, screen_h);
      float height_px = resolve_size(HEIGHT, screen_w, screen_h);
      float padding_px = resolve_size(PADDING, screen_w, screen_h);
      float gap_px = resolve_size(TOAST_GAP, screen_w, screen_h);

      int index = 0;

      for (Entity &entity : EntityQuery()
                                .whereHasComponent<Toast>()
                                .whereHasComponent<ui::UIComponent>()
                                .gen()) {
        Toast &t = entity.get<Toast>();
        ui::UIComponent &ui = entity.get<ui::UIComponent>();

        // Ensure size is set for absolute elements
        if (ui.computed[ui::Axis::X] <= 0)
          ui.computed[ui::Axis::X] = width_px;
        if (ui.computed[ui::Axis::Y] <= 0)
          ui.computed[ui::Axis::Y] = height_px;

        float toast_height = ui.computed[ui::Axis::Y];

        float y_offset = static_cast<float>(index) * (toast_height + gap_px);
        float y_pos =
            static_cast<float>(screen_h) - padding_px - toast_height - y_offset;
        float x_pos = static_cast<float>(screen_w) - width_px - padding_px;

        float alpha_ease = detail::ease_out_expo(t.progress());
        float slide_offset = (1.0f - alpha_ease) * 50.0f;
        x_pos += slide_offset;

        ui.computed_rel[ui::Axis::X] = x_pos;
        ui.computed_rel[ui::Axis::Y] = y_pos;

        if (entity.has<ui::HasOpacity>()) {
          entity.get<ui::HasOpacity>().value = alpha_ease;
        }

        // Queue the toast for rendering each frame (skip expired/cleanup)
        if (!t.is_expired() && !entity.cleanup) {
          auto *ctx_ptr =
              EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
          if (ctx_ptr) {
            ctx_ptr->queue_render(ui::RenderInfo{entity.id, 100});
          }
        }

        index++;
      }
    }
  };

  static void add_singleton_components(Entity &singleton) {
    detail::init_singleton(singleton);
  }

  static void enforce_singletons(SystemManager &) {
    // Create singleton immediately if it doesn't exist
    auto *root = EntityHelper::get_singleton_cmp<ToastRoot>();
    if (!root) {
      Entity &singleton = EntityHelper::createEntity();
      detail::init_singleton(singleton);
    }
  }

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<ToastUpdateSystem>());
  }

  template <typename InputAction>
  static void register_layout_systems(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<ToastLayoutSystem<InputAction>>());
  }
};

// Compile-time verification that toast satisfies the PluginCore concept
static_assert(developer::PluginCore<toast>,
              "toast must implement the core plugin interface");

} // namespace afterhours
