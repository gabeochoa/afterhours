#pragma once

#include <algorithm>
#include <format>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../entity.h"
#include "../../entity_helper.h"
#include "../../entity_query.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../input_system.h"
#include "../texture_manager.h"
#include "components.h"
#include "context.h"
#include "systems.h"
#include "theme.h"

namespace afterhours {

namespace ui {

static inline RectangleType position_text(const ui::FontManager &fm,
                                          const std::string &text,
                                          RectangleType container,
                                          TextAlignment alignment,
                                          Vector2Type margin_px) {
  Font font = fm.get_active_font();

  // Calculate the maximum text size based on the container size and margins
  Vector2Type max_text_size = Vector2Type{
      .x = container.width - 2 * margin_px.x,
      .y = container.height - 2 * margin_px.y,
  };

  // TODO add some caching here?

  // Find the largest font size that fits within the maximum text size
  float font_size = 1.f;
  Vector2Type text_size;
  while (true) {
    text_size = measure_text(font, text.c_str(), font_size, 1.f);
    if (text_size.x > max_text_size.x || text_size.y > max_text_size.y) {
      break;
    }
    font_size++;
  }
  font_size--; // Decrease font size by 1 to ensure it fits

  // Calculate the text position based on the alignment and margins
  Vector2Type position;
  switch (alignment) {
  case TextAlignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x +
             (container.width - 2 * margin_px.x - text_size.x) / 2,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x - text_size.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return RectangleType{
      .x = position.x,
      .y = position.y,
      .width = font_size,
      .height = font_size,
  };
}

static inline void draw_text_in_rect(const ui::FontManager &fm,
                                     const std::string &text,
                                     RectangleType rect,
                                     TextAlignment alignment, Color color) {
  RectangleType sizing =
      position_text(fm, text, rect, alignment, Vector2Type{5.f, 5.f});
  draw_text_ex(fm.get_active_font(), text.c_str(),
               Vector2Type{sizing.x, sizing.y}, sizing.height, 1.f, color);
}

static inline Vector2Type
position_texture(texture_manager::Texture, Vector2Type size,
                 RectangleType container,
                 texture_manager::HasTexture::Alignment alignment,
                 Vector2Type margin_px = {0.f, 0.f}) {

  // Calculate the text position based on the alignment and margins
  Vector2Type position;

  switch (alignment) {
  case texture_manager::HasTexture::Alignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y + size.x,
    };
    break;
  case texture_manager::HasTexture::Alignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x + (container.width / 2) + (size.x / 2),
        .y = container.y + margin_px.y + (container.height / 2) + (size.y / 2),
    };
    break;
  case texture_manager::HasTexture::Alignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x + size.x,
        .y = container.y + margin_px.y + size.y,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return Vector2Type{
      .x = position.x,
      .y = position.y,
  };
}

static inline void
draw_texture_in_rect(texture_manager::Texture texture, RectangleType rect,
                     texture_manager::HasTexture::Alignment alignment) {

  float scale = texture.height / rect.height;
  Vector2Type size = {
      texture.width / scale,
      texture.height / scale,
  };

  Vector2Type location = position_texture(texture, size, rect, alignment);

  texture_manager::draw_texture_pro(texture,
                                    RectangleType{
                                        0,
                                        0,
                                        texture.width,
                                        texture.height,
                                    },
                                    RectangleType{
                                        .x = location.x,
                                        .y = location.y,
                                        .width = size.x,
                                        .height = size.y,
                                    },
                                    size, 0.f, colors::UI_WHITE);
}

template <typename InputAction>
struct RenderDebugAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {

  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;

  mutable int level = 0;
  mutable int indent = 0;

  float fontSize = 20;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp)
      : toggle_action(toggle_kp) {}

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector<InputAction> inpc =
          input::get_input_collector<InputAction>();
      for (auto &actions_done : inpc.inputs()) {
        if (actions_done.action == toggle_action) {
          enabled = !enabled;
          break;
        }
      }
    }
    return enabled;
  }

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    this->include_derived_children = true;

    draw_text(std::format("mouse({}, {})", this->context->mouse_pos.x,
                          this->context->mouse_pos.y)
                  .c_str(),
              0, 0, fontSize,
              this->context->theme.from_usage(Theme::Usage::Font));

    // starting at 1 to avoid the mouse text
    this->level = 1;
    this->indent = 0;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    float x = 10 * indent;
    float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      component_name = cmpdebug.name();
    }

    std::string widget_str = std::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    float text_width = measure_text_internal(widget_str.c_str(), fontSize);
    Rectangle debug_label_location = Rectangle{x, y, text_width, fontSize};

    if (is_mouse_inside(this->context->mouse_pos, debug_label_location)) {
      draw_rectangle_outline(
          cmp.rect(), this->context->theme.from_usage(Theme::Usage::Error));
      draw_rectangle_outline(cmp.bounds(), colors::UI_BLACK);
      draw_rectangle(debug_label_location, colors::UI_BLUE);
    } else {
      draw_rectangle(debug_label_location, colors::UI_BLACK);
    }

    draw_text(widget_str.c_str(), x, y, fontSize,
              this->context->is_hot(entity.id)
                  ? this->context->theme.from_usage(Theme::Usage::Error)
                  : this->context->theme.from_usage(Theme::Usage::Font));
  }

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.was_rendered_to_screen) {
      render_me(entity);
      level++;
    }

    indent++;
    for (EntityID child : cmp.children) {
      render(AutoLayout::to_ent_static(child));
    }
    indent--;
  }

  virtual void for_each_with_derived(const Entity &entity, const UIComponent &,
                                     const AutoLayoutRoot &, float) const {
    render(entity);
    level += 2;
    indent = 0;
  }
};

template <typename InputAction> struct RenderImm : System<> {

  RenderImm() : System<>() { this->include_derived_children = true; }

  void render_me(const UIContext<InputAction> &context,
                 const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id)) {
        col = context.theme.from_usage(Theme::Usage::Background);
      }

      auto corner_settings = entity.has<HasRoundedCorners>()
                                 ? entity.get<HasRoundedCorners>().get()
                                 : std::bitset<4>().reset();

      // TODO do we need another color for this?
      if (context.has_focus(entity.id)) {
        draw_rectangle_rounded(cmp.focus_rect(),
                               0.5f, // roundness
                               8,    // segments
                               context.theme.from_usage(Theme::Usage::Accent),
                               corner_settings);
      }
      draw_rectangle_rounded(cmp.rect(),
                             0.5f, // roundness
                             8,    // segments
                             col, corner_settings);
    }

    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      draw_text_in_rect(
          font_manager, hasLabel.label.c_str(), cmp.rect(), hasLabel.alignment,
          context.theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled));
    }

    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      draw_texture_in_rect(texture.texture, cmp.rect(), texture.alignment);
    }
  }

  void render(const UIContext<InputAction> &context,
              const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT)
      const_cast<FontManager &>(font_manager).set_active(cmp.font_name);

    if (entity.has<HasColor>() || entity.has<HasLabel>()) {
      render_me(context, font_manager, entity);
    }

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with(const Entity &entity,
                             // TODO why if we add these to the filter,
                             // this doesnt return any entities?
                             //
                             // const UIContext<InputAction> &context,
                             // const FontManager &font_manager,
                             float) const override {

    if (entity.is_missing<UIContext<InputAction>>())
      return;
    if (entity.is_missing<FontManager>())
      return;

    const UIContext<InputAction> &context =
        entity.get<UIContext<InputAction>>();
    const FontManager &font_manager = entity.get<FontManager>();

    std::ranges::sort(context.render_cmds, [](RenderInfo a, RenderInfo b) {
      if (a.layer == b.layer)
        return a.id < b.id;
      return a.layer < b.layer;
    });

    for (auto &cmd : context.render_cmds) {
      auto id = cmd.id;
      auto &ent = EntityHelper::getEntityForIDEnforce(id);
      render(context, font_manager, ent);
    }
    context.render_cmds.clear();
  }
};

} // namespace ui

} // namespace afterhours