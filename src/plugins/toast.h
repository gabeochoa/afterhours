#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/entity_query.h"
#include "../core/system.h"
#include "../developer.h"
#include "../drawing_helpers.h"
#include "animation/instant.h"
#include "color.h"
#include "ui/element_result.h"
#include "ui/imm_components.h"
#include "ui/rendering.h"
#include "ui/text_measure.h"
#include "ui/theme.h"
#include "window_manager.h"

namespace afterhours {

struct toast : developer::Plugin {
    enum class Level { Info, Success, Warning, Error, Custom };
    enum class Position { TopLeft, TopRight, BottomLeft, BottomRight, BottomCenter };
    static inline Position position = Position::BottomCenter;

    // Resolution-scaled sizes (designed for 720p, scales proportionally)
    static inline ui::Size WIDTH = ui::h720(340.0f);
    static inline ui::Size HEIGHT = ui::h720(48.0f);
    static inline ui::Size PADDING = ui::h720(14.0f);
    static inline ui::Size TOAST_GAP = ui::h720(8.0f);
    // TODO: let apps configure MAX_VISIBLE like WIDTH/HEIGHT/position
    static inline int MAX_VISIBLE = 3;

    // Helper to resolve a Size to actual pixels given screen dimensions
    static float resolve_size(const ui::Size &size, int screen_w,
                              int screen_h) {
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
            case ui::Dim::Expand:
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
        bool hovered = false;

        Toast() = default;
        Toast(Level lvl, float dur, Color color = {100, 100, 100, 255})
            : level(lvl), custom_color(color), duration(dur) {}

        [[nodiscard]] float progress() const {
            if (duration <= 0.0f) return 0.0f;
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

        // Only warnings/errors get a text prefix; info/success stay clean.
        [[nodiscard]] static std::string icon_for_level(Level level) {
            switch (level) {
                case Level::Warning:
                    return "[!]";
                case Level::Error:
                    return "[X]";
                case Level::Success:
                case Level::Custom:
                case Level::Info:
                default:
                    return "";
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

        static float wrapped_height(const std::string &text, float width_px,
                                    float min_height, const std::string &font,
                                    float font_px, float inset_x,
                                    float inset_y) {
            if (text.empty() || width_px <= 2.f * inset_x || font_px <= 0.f)
                return min_height;
            const auto m = ui::measure_text_wrapped(
                text, width_px - 2.f * inset_x, font, font_px);
            if (m.line_count <= 1) return min_height;
            return std::max(min_height,
                            m.height + 2.f * inset_y + inset_x);
        }

        static ui::imm::ElementResult create_simple_toast(
            ui::imm::HasUIContext auto &ctx, const std::string &msg,
            Level level, float duration,
            Color custom_color = {100, 100, 100, 255}) {
            // Set the label with icon and message (no prefix for info/success).
            std::string icon = icon_for_level(level);
            const std::string label =
                icon.empty() ? msg : icon + " " + msg;

            for (Entity &entity : EntityQuery()
                                      .whereHasComponent<Toast>()
                                      .gen()) {
                Toast &existing = entity.get<Toast>();
                if (!entity.cleanup && !existing.is_expired() &&
                    existing.level == level && existing.message == label) {
                    existing.elapsed = 0.f;
                    existing.duration = duration;
                    return {true, entity};
                }
            }

            ui::imm::ElementResult result =
                schedule(ctx, level, duration, custom_color);
            result.ent().get<ui::HasLabel>().label = label;
            result.ent().get<Toast>().message = label;
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
        throw std::runtime_error(
            "Toast root not initialized. Call "
            "toast::add_singleton_components() first.");
    }

    // Create a toast as an independent entity (not part of IMM lifecycle)
    // Returns ElementResult where result is true when toast is visible
    // Note: Toasts are created once and managed by ToastUpdateSystem, they
    // don't follow the IMM pattern of being recreated every frame.
    static ui::imm::ElementResult schedule(
        ui::imm::HasUIContext auto &ctx, Level level = Level::Info,
        float duration = 3.0f, Color custom_color = {100, 100, 100, 255}) {
        using namespace ui;

        {
            RefEntities live = EntityQuery()
                                   .whereHasComponent<Toast>()
                                   .whereLambda([](const Entity &e) {
                                       return !e.cleanup &&
                                              !e.get<Toast>().is_expired();
                                   })
                                   .gen();
            std::sort(live.begin(), live.end(),
                      [](Entity &a, Entity &b) {
                          return a.get<Toast>().elapsed >
                                 b.get<Toast>().elapsed;
                      });
            while (static_cast<int>(live.size()) >= MAX_VISIBLE) {
                live.front().get().cleanup = true;
                live.erase(live.begin());
            }
        }

        // Create independent entity (not through IMM mk())
        Entity &entity = EntityHelper::createEntity();

        // Determine background color
        Color bg_color = [&]() {
            if (level == Level::Custom) {
                return custom_color;
            }
            switch (level) {
                case Level::Success:
                    return ctx.theme.success;
                case Level::Warning:
                    return ctx.theme.warning;
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
        ui.set_desired_width(WIDTH).set_desired_height(HEIGHT);
        ui.flex_direction = FlexDirection::Row;
        const auto &defaults = imm::UIStylingDefaults::get();
        const auto &font_name = defaults.resolved_font_name();
        ui.enable_font(font_name, defaults.default_font_size, true);
        ui.resolved_scaling_mode = ctx.scaling_mode.value_or(defaults.scaling_mode);

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
        entity.addComponent<ui::HasOpacity>(1.0f);
        entity.addComponent<ui::HasRoundedCorners>()
            .set(ctx.theme.rounded_corners)
            .set_roundness(ctx.theme.roundness)
            .set_radius_px(ctx.theme.corner_radius);
        entity.addComponent<Toast>(Toast(level, duration, bg_color));
        auto &label = entity.addComponent<ui::HasLabel>("");
        label.text_overflow = ui::TextOverflow::Wrap;
        label.text_inset = Vector2Type{12.f, 8.f};
        label.explicit_text_color =
            colors::auto_text_color(bg_color, ctx.theme.font, ctx.theme.darkfont);
        entity.addComponent<ui::UIComponentDebug>("toast");

        // Note: Don't queue for render here - ToastLayoutSystem handles
        // rendering after positioning to avoid flash at (0,0)

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
                                              const std::string &msg,
                                              Color color,
                                              float duration = 3.0f) {
        return detail::create_simple_toast(ctx, msg, Level::Custom, duration,
                                           color);
    }

    struct ToastUpdateSystem : System<Toast> {
        void for_each_with(Entity &entity, Toast &t, float dt) override {
            if (!t.hovered) t.elapsed += dt;
            if (t.is_expired()) {
                entity.cleanup = true;
            }
        }
    };

    template<typename InputAction>
    struct ToastLayoutSystem : System<> {
        bool should_iterate() const override { return false; }
        void once(float) override {
            auto *res = EntityHelper::get_singleton_cmp<
                window_manager::ProvidesCurrentResolution>();
            if (!res) return;

            int screen_w = res->current_resolution.width;
            int screen_h = res->current_resolution.height;

            // Resolve sizes for current resolution
            float width_px = resolve_size(WIDTH, screen_w, screen_h);
            float height_px = resolve_size(HEIGHT, screen_w, screen_h);
            float padding_px = resolve_size(PADDING, screen_w, screen_h);
            float gap_px = resolve_size(TOAST_GAP, screen_w, screen_h);

            auto *ctx_ptr = EntityHelper::get_singleton_cmp<
                ui::UIContext<InputAction>>();
            float y_offset = 0.f;
            int index = 0;

            for (Entity &entity : EntityQuery()
                                      .whereHasComponent<Toast>()
                                      .whereHasComponent<ui::UIComponent>()
                                      .gen()) {
                Toast &t = entity.get<Toast>();
                ui::UIComponent &ui = entity.get<ui::UIComponent>();

                if (ui.desired[ui::Axis::X].dim != ui::Dim::None)
                    ui.computed[ui::Axis::X] = resolve_size(ui.desired[ui::Axis::X], screen_w, screen_h);
                else if (ui.computed[ui::Axis::X] <= 0)
                    ui.computed[ui::Axis::X] = width_px;
                float base_height = height_px;
                if (ui.desired[ui::Axis::Y].dim != ui::Dim::None)
                    base_height = resolve_size(ui.desired[ui::Axis::Y], screen_w, screen_h);

                float toast_height = base_height;
                if (entity.has<ui::HasLabel>()) {
                    const auto &label = entity.get<ui::HasLabel>();
                    const Vector2Type inset =
                        label.text_inset.value_or(Vector2Type{12.f, 8.f});
                    toast_height = detail::wrapped_height(
                        label.label, ui.computed[ui::Axis::X], base_height,
                        ui.resolved_font_name(),
                        resolve_size(ui.font_size, screen_w, screen_h),
                        inset.x, inset.y);
                    toast_height = std::min(
                        toast_height,
                        std::max(base_height,
                                 static_cast<float>(screen_h) - 2.f * padding_px));
                }
                ui.computed[ui::Axis::Y] = toast_height;
                float y_pos = static_cast<float>(screen_h) - padding_px -
                              toast_height - y_offset;
                const bool at_top = position == Position::TopLeft || position == Position::TopRight;
                if (at_top) y_pos = padding_px + y_offset;
                const float toast_width = ui.computed[ui::Axis::X];
                float x_pos = (static_cast<float>(screen_w) - toast_width) / 2.0f;
                if (position == Position::TopLeft || position == Position::BottomLeft)
                    x_pos = padding_px;
                if (position == Position::TopRight || position == Position::BottomRight)
                    x_pos = static_cast<float>(screen_w) - padding_px - toast_width;

                const bool instant = motion::is_instant();
                float alpha_ease = instant ? 1.0f : detail::ease_out_expo(t.progress());
                float slide_offset = instant ? 0.0f : (1.0f - alpha_ease) * 24.0f;
                y_pos += at_top ? -slide_offset : slide_offset;

                ui.computed_rel[ui::Axis::X] = x_pos;
                ui.computed_rel[ui::Axis::Y] = y_pos;
                if (!t.is_expired()) y_offset += toast_height + gap_px;

                if (entity.has<ui::HasOpacity>()) {
                    entity.get<ui::HasOpacity>().value = alpha_ease;
                }

                t.hovered = false;
                if (ctx_ptr) {
                    const RectangleType rect{x_pos, y_pos, toast_width,
                                             toast_height};
                    t.hovered = ui::is_mouse_inside(ctx_ptr->mouse.pos, rect);
                    if (t.hovered && ctx_ptr->mouse.just_pressed) t.dismiss();
                }

                if (!t.is_expired() && !entity.cleanup) {
                    if (ctx_ptr) {
                        ctx_ptr->queue_render(ui::RenderInfo{entity.id, 100});
                    }
                    index++;
                }
            }
        }
    };

    static void add_singleton_components(Entity &singleton) {
        detail::init_singleton(singleton);
    }

    static void enforce_singletons(SystemManager &) {
        // Create singleton immediately if it doesn't exist
        if (EntityHelper::has_singleton<ToastRoot>()) return;
        Entity &singleton = EntityHelper::createEntity();
        detail::init_singleton(singleton);
    }

    static void register_update_systems(SystemManager &sm) {
        sm.register_update_system(std::make_unique<ToastUpdateSystem>());
    }

    template<typename InputAction>
    static void register_layout_systems(SystemManager &sm) {
        sm.register_render_system(
            std::make_unique<ToastLayoutSystem<InputAction>>());
    }
};

template <typename InputAction> struct ToastPlugin {
    void install(PluginContext &ctx) const {
        auto &root = ctx.ensure_singleton<toast::ToastRoot>();
        if (root.entity_id < 0) {
            Entity &root_entity = ctx.collection.createEntity();
            root_entity.addComponent<ui::UIComponent>(root_entity.id);
            root_entity.get<ui::UIComponent>().make_absolute();
            root.entity_id = root_entity.id;
            ctx.collection.merge_entity_arrays();
        }
        ctx.add_system(Stage::Update,
                       std::make_unique<toast::ToastUpdateSystem>());
        ctx.add_system(Stage::PreRender,
                       std::make_unique<toast::ToastLayoutSystem<InputAction>>());
    }
};

// Compile-time verification that toast satisfies the PluginCore concept
static_assert(developer::PluginCore<toast>,
              "toast must implement the core plugin interface");

}  // namespace afterhours
