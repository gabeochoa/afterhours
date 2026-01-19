#pragma once

#include <cmath>
#include <string>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../drawing_helpers.h"
#include "color.h"
#include "ui/rendering.h"
#include "ui/theme.h"
#include "window_manager.h"

namespace afterhours {

struct toast : developer::Plugin {

  enum class Level { Info, Success, Warning, Error, Custom };

  static constexpr float WIDTH = 380.0f;
  static constexpr float HEIGHT = 50.0f;
  static constexpr float PADDING = 16.0f;

  // Component attached to toast entities
  struct Toast : BaseComponent {
    std::string message;
    Level level = Level::Info;
    Color custom_color = {100, 100, 100, 255};
    float duration = 3.0f;
    float elapsed = 0.0f;

    Toast() = default;
    Toast(std::string msg, Level lvl, float dur, Color color = {100, 100, 100, 255})
        : message(std::move(msg)), level(lvl), custom_color(color), duration(dur) {}

    [[nodiscard]] float progress() const {
      if (duration <= 0.0f)
        return 0.0f;
      return 1.0f - std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    [[nodiscard]] bool is_expired() const { return elapsed >= duration; }
  };

  // Update system - advances time and marks expired toasts for deletion
  struct ToastUpdateSystem : System<Toast> {
    void for_each_with(Entity &entity, Toast &t, float dt) override {
      t.elapsed += dt;
      if (t.is_expired()) {
        entity.cleanup = true;
      }
    }
  };

  [[nodiscard]] static Color color_for_level(const Toast &t,
                                             const ui::Theme &theme) {
    switch (t.level) {
    case Level::Success:
      return theme.secondary;
    case Level::Warning:
      return theme.accent;
    case Level::Error:
      return theme.error;
    case Level::Custom:
      return t.custom_color;
    case Level::Info:
    default:
      return theme.primary;
    }
  }

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

  // Render system - renders all active toasts
  // Uses System<> (no template) so it works as a render system
  template <typename InputAction>
  struct ToastRenderSystem : System<> {
    static constexpr float ICON_WIDTH = 40.0f;
    static constexpr float PROGRESS_BAR_HEIGHT = 4.0f;
    static constexpr float ROUNDNESS = 0.15f;
    static constexpr int SEGMENTS = 8;
    static constexpr float FONT_SIZE = 18.0f;
    static constexpr float FONT_SPACING = 1.0f;

    void once(float) const override {
      auto *ctx = EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
      auto *res = EntityHelper::get_singleton_cmp<window_manager::ProvidesCurrentResolution>();
      auto *font_mgr = EntityHelper::get_singleton_cmp<ui::FontManager>();
      
      if (!ctx || !res || !font_mgr) {
        return;
      }

      const ui::Theme &theme = ctx->theme;
      int screen_w = res->current_resolution.width;
      int screen_h = res->current_resolution.height;
      Font font = font_mgr->get_active_font();

      float start_y = static_cast<float>(screen_h) - PADDING - HEIGHT;

      int render_index = 0;
      
      // Iterate over all entities with Toast component
      for (const auto &entity_ptr : EntityHelper::get_entities()) {
        if (!entity_ptr || !entity_ptr->has<Toast>()) continue;
        Toast &t = entity_ptr->get<Toast>();
        
        float prog = t.progress();
        float alpha_ease = ease_out_expo(prog);

        float y_offset = static_cast<float>(render_index) * (HEIGHT + 8.0f);
        float y_pos = start_y - y_offset;
        float x_pos = static_cast<float>(screen_w) - WIDTH - PADDING;

        float slide_offset = (1.0f - alpha_ease) * 50.0f;
        x_pos += slide_offset;

        // Get background color and apply alpha
        Color bg = color_for_level(t, theme);
        bg = colors::opacity_pct(bg, alpha_ease);

        // Draw toast background with rounded corners using internal API
        RectangleType toast_rect = {x_pos, y_pos, WIDTH, HEIGHT};
        std::bitset<4> all_corners;
        all_corners.set(); // All corners rounded
        draw_rectangle_rounded(toast_rect, ROUNDNESS, SEGMENTS, bg, all_corners);

        // Calculate text color with auto-contrast and alpha
        Color text_color = colors::auto_text_color(bg, theme.font, theme.darkfont);
        text_color = colors::opacity_pct(text_color, alpha_ease);

        // Calculate vertical center for text (same for both icon and message)
        float content_height = HEIGHT - PROGRESS_BAR_HEIGHT;
        float text_y = y_pos + (content_height - FONT_SIZE) / 2.0f;

        // Draw icon with fixed font size
        std::string icon = icon_for_level(t.level);
        Vector2Type icon_size = measure_text(font, icon.c_str(), FONT_SIZE, FONT_SPACING);
        float icon_x = x_pos + 8.0f + (ICON_WIDTH - icon_size.x) / 2.0f; // Center icon horizontally
        draw_text_ex(font, icon.c_str(), {icon_x, text_y}, FONT_SIZE, FONT_SPACING, text_color);

        // Draw message with same fixed font size, aligned left
        float msg_x = x_pos + ICON_WIDTH + 4.0f;
        draw_text_ex(font, t.message.c_str(), {msg_x, text_y}, FONT_SIZE, FONT_SPACING, text_color);

        // Draw progress bar at bottom
        float progress_width = WIDTH * prog;
        Color progress_color = colors::opacity_pct({255, 255, 255, 100}, alpha_ease);
        RectangleType progress_rect = {x_pos, y_pos + HEIGHT - PROGRESS_BAR_HEIGHT, 
                                       progress_width, PROGRESS_BAR_HEIGHT};
        draw_rectangle(progress_rect, progress_color);

        render_index++;
      }
    }
  };

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<ToastUpdateSystem>());
  }

  template <typename InputAction>
  static void register_render_systems(SystemManager &sm) {
    sm.register_render_system(
        std::make_unique<ToastRenderSystem<InputAction>>());
  }

  // Helper to create a toast entity
  static Entity &make_toast(const std::string &msg, Level level,
                            float duration = 3.0f,
                            Color custom_color = {100, 100, 100, 255}) {
    Entity &e = EntityHelper::createEntity();
    e.addComponent<Toast>(Toast(msg, level, duration, custom_color));
    return e;
  }

  static Entity &send_info(const std::string &msg, float duration = 3.0f) {
    return make_toast(msg, Level::Info, duration);
  }

  static Entity &send_success(const std::string &msg, float duration = 3.0f) {
    return make_toast(msg, Level::Success, duration);
  }

  static Entity &send_warning(const std::string &msg, float duration = 5.0f) {
    return make_toast(msg, Level::Warning, duration);
  }

  static Entity &send_error(const std::string &msg, float duration = 7.0f) {
    return make_toast(msg, Level::Error, duration);
  }

  static Entity &send_custom(const std::string &msg, Color color,
                             float duration = 3.0f) {
    return make_toast(msg, Level::Custom, duration, color);
  }

};

} // namespace afterhours
