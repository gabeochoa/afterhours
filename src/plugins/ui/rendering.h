#pragma once

#include "fmt/format.h"
#include <algorithm>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../animation.h"
#include "../input_system.h"
#include "../texture_manager.h"
#include "animation_keys.h"
#include "components.h"
#include "context.h"
#include "systems.h"
#include "text_utils.h"
#include "theme.h"

namespace afterhours {

namespace ui {

static inline float _compute_effective_opacity(const Entity &entity) {
  float result = 1.0f;
  const Entity *cur = &entity;
  int guard = 0;
  while (cur) {
    if (cur->has<HasOpacity>()) {
      result *= std::clamp(cur->get<HasOpacity>().value, 0.0f, 1.0f);
    }
    if (!cur->has<UIComponent>())
      break;
    EntityID pid = cur->get<UIComponent>().parent;
    if (pid < 0 || pid == cur->id)
      break;
    cur = &EntityHelper::getEntityForIDEnforce(pid);
    if (++guard > 64)
      break;
  }
  return std::clamp(result, 0.0f, 1.0f);
}
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

  // Check if text contains CJK characters and use appropriate rendering
  if (afterhours::ui::text_utils::contains_cjk(text)) {
    // For CJK text, we need to handle UTF-8 properly
    // Use the font's active font and render with proper spacing
    Font font = fm.get_active_font();
    float fontSize = sizing.height;
    float spacing = 1.0f;

    // Calculate starting position based on alignment
    Vector2Type startPos = {sizing.x, sizing.y};
    if (alignment == TextAlignment::Center) {
      // Center the text within the rectangle
      Vector2Type textSize =
          measure_text_utf8(font, text.c_str(), fontSize, spacing);
      startPos.x = rect.x + (rect.width - textSize.x) / 2.0f;
      startPos.y = rect.y + (rect.height - textSize.y) / 2.0f;
    }

    // Draw the text using draw_text_ex which handles UTF-8 properly
    draw_text_ex(font, text.c_str(), startPos, fontSize, spacing, color);
  } else {
    // For non-CJK text, use the existing method
    draw_text_ex(fm.get_active_font(), text.c_str(),
                 Vector2Type{sizing.x, sizing.y}, sizing.height, 1.f, color);
  }
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

  float scale = (float)texture.height / rect.height;
  Vector2Type size = {
      (float)texture.width / scale,
      (float)texture.height / scale,
  };

  Vector2Type location = position_texture(texture, size, rect, alignment);

  texture_manager::draw_texture_pro(texture,
                                    RectangleType{
                                        0.0f,
                                        0.0f,
                                        (float)texture.width,
                                        (float)texture.height,
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

  mutable UIContext<InputAction> *context;

  mutable int level = 0;
  mutable int indent = 0;
  mutable EntityID isolated_id = -1;
  mutable bool isolate_enabled = false;
  enum struct IsolationMode { NodeOnly, NodeAndDescendants };
  mutable IsolationMode isolation_mode = IsolationMode::NodeOnly;

  float fontSize = 20.0f;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector inpc = input::get_input_collector();
      for (auto &actions_done : inpc.inputs()) {
        if (static_cast<InputAction>(actions_done.action) == toggle_action) {
          enabled = !enabled;
          break;
        }
      }
    }
    return enabled;
  }

  virtual void once(float) const override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

    draw_text(fmt::format("mouse({}, {})", this->context->mouse_pos.x,
                          this->context->mouse_pos.y)
                  .c_str(),
              0.0f, 0.0f, fontSize,
              this->context->theme.from_usage(Theme::Usage::Font));

    // starting at 1 to avoid the mouse text
    this->level = 1;
    this->indent = 0;
  }

  bool is_descendant_of_isolated(const Entity &entity) const {
    if (!isolate_enabled)
      return false;
    if (entity.id == isolated_id)
      return false;
    const Entity *cur = &entity;
    int guard = 0;
    while (cur->has<UIComponent>()) {
      const UIComponent &cur_cmp = cur->get<UIComponent>();
      if (cur_cmp.parent < 0 || ++guard > 64)
        break;
      if (cur_cmp.parent == isolated_id)
        return true;
      cur = &EntityHelper::getEntityForIDEnforce(cur_cmp.parent);
    }
    return false;
  }

  void render_me(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();

    const float x = 10 * indent;
    const float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      component_name = cmpdebug.name();
    }

    const std::string widget_str = fmt::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    const float text_width =
        measure_text_internal(widget_str.c_str(), fontSize);
    const Rectangle debug_label_location =
        Rectangle{x, y, text_width, fontSize};

    const bool is_hovered =
        is_mouse_inside(this->context->mouse_pos, debug_label_location);
    bool show = true;
    if (isolate_enabled) {
      if (entity.id == isolated_id) {
        show = true;
      } else if (isolation_mode == IsolationMode::NodeAndDescendants) {
        show = is_descendant_of_isolated(entity);
      } else {
        show = false;
      }
    }
    const bool hidden = !show;

    const auto color_or_hidden = [hidden](Color c) {
      return hidden ? colors::opacity_pct(c, 0.f) : c;
    };

    if (is_hovered) {
      draw_rectangle_outline(cmp.rect(),
                             color_or_hidden(this->context->theme.from_usage(
                                 Theme::Usage::Error)));
      draw_rectangle_outline(cmp.bounds(), color_or_hidden(colors::UI_BLACK));
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLUE));
    } else {
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLACK));
    }

    Color baseText = this->context->is_hot(entity.id)
                         ? this->context->theme.from_usage(Theme::Usage::Error)
                         : this->context->theme.from_usage(Theme::Usage::Font);
    draw_text(widget_str.c_str(), x, y, fontSize, color_or_hidden(baseText));

    const bool left_released = input::is_mouse_button_released(0);
    const bool right_released = input::is_mouse_button_released(1);
    if (is_hovered && (left_released || right_released)) {
      IsolationMode new_mode = left_released ? IsolationMode::NodeAndDescendants
                                             : IsolationMode::NodeOnly;
      if (isolate_enabled && isolated_id == entity.id &&
          isolation_mode == new_mode) {
        isolate_enabled = false;
        isolated_id = -1;
      } else {
        isolate_enabled = true;
        isolated_id = entity.id;
        isolation_mode = new_mode;
      }
    }
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

template <typename InputAction>
struct RenderImm : System<UIContext<InputAction>, FontManager> {

  RenderImm() : System<UIContext<InputAction>, FontManager>() {
    this->include_derived_children = true;
  }

  void render_me(const UIContext<InputAction> &context,
                 const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    const float effective_opacity = _compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    if (entity.has<HasUIModifiers>()) {
      draw_rect = entity.get<HasUIModifiers>().apply_modifier(draw_rect);
    }

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();

    if (context.visual_focus_id == entity.id) {
      Color focus_col = context.theme.from_usage(Theme::Usage::Accent);
      float effective_focus_opacity = _compute_effective_opacity(entity);
      if (effective_focus_opacity < 1.0f) {
        focus_col = colors::opacity_pct(focus_col, effective_focus_opacity);
      }
      RectangleType focus_rect = cmp.focus_rect();
      if (entity.has<HasUIModifiers>()) {
        focus_rect = entity.get<HasUIModifiers>().apply_modifier(focus_rect);
      }
      if (corner_settings.any()) {
        draw_rectangle_rounded(focus_rect,
                               0.5f, // roundness
                               8,    // segments
                               focus_col, corner_settings);
      } else {
        draw_rectangle_outline(focus_rect, focus_col);
      }
    }

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id)) {
        col = context.theme.from_usage(Theme::Usage::Background);
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      draw_rectangle_rounded(draw_rect,
                             0.5f, // roundness
                             8,    // segments
                             col, corner_settings);
    }

    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      Color font_col =
          context.theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled);
      if (effective_opacity < 1.0f) {
        font_col = colors::opacity_pct(font_col, effective_opacity);
      }
      draw_text_in_rect(font_manager, hasLabel.label.c_str(), draw_rect,
                        hasLabel.alignment, font_col);
    }

    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      // draw textured rect with opacity via color tint
      // NOTE: draw_texture_in_rect path lacks tint, so opacity will apply to
      // images below reuse existing helper (no tint support), so fallback to
      // image path below
      draw_texture_in_rect(texture.texture, draw_rect, texture.alignment);
    } else if (entity.has<ui::HasImage>()) {
      const ui::HasImage &img = entity.get<ui::HasImage>();
      texture_manager::Rectangle src =
          img.source_rect.value_or(texture_manager::Rectangle{
              0.0f, 0.0f, (float)img.texture.width, (float)img.texture.height});

      // Scale to fit height of rect
      float scale = src.height / draw_rect.height;
      Vector2Type size = {src.width / scale, src.height / scale};
      Vector2Type location =
          position_texture(img.texture, size, draw_rect, img.alignment);

      Color img_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        img_col = colors::opacity_pct(img_col, effective_opacity);
      }
      texture_manager::draw_texture_pro(img.texture, src,
                                        RectangleType{
                                            .x = location.x,
                                            .y = location.y,
                                            .width = size.x,
                                            .height = size.y,
                                        },
                                        size, 0.f, img_col);
    }
  }

  void render(const UIContext<InputAction> &context,
              const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT) {
      const_cast<FontManager &>(font_manager).set_active(cmp.font_name);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>()) {
      render_me(context, font_manager, entity);
    }

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with_derived(const Entity &entity,
                                     const UIContext<InputAction> &context,
                                     const FontManager &font_manager,
                                     float) const override {
#if __WIN32
    // Note we have to do bubble sort here because mingw doesnt support
    // std::ranges::sort
    for (size_t i = 0; i < context.render_cmds.size(); ++i) {
      for (size_t j = i + 1; j < context.render_cmds.size(); ++j) {
        if ((context.render_cmds[i].layer > context.render_cmds[j].layer) ||
            (context.render_cmds[i].layer == context.render_cmds[j].layer &&
             context.render_cmds[i].id > context.render_cmds[j].id)) {
          std::swap(context.render_cmds[i], context.render_cmds[j]);
        }
      }
    }
#else
    std::ranges::sort(context.render_cmds, [](RenderInfo a, RenderInfo b) {
      if (a.layer == b.layer)
        return a.id < b.id;
      return a.layer < b.layer;
    });
#endif

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
