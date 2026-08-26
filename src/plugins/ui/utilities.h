#pragma once

#include <string>
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif

#include "../../core/text_cache.h"
#include "../../developer.h"
#include "../../ecs.h"
#include "../../logging.h"
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
#include "../e2e_testing/test_input.h"
#include "../e2e_testing/visible_text.h"
#endif
#include "../../font_helper.h"
#include "../autolayout.h"
#include "../window_manager.h"
#include "entity_management.h"
#include "components.h"
#include "context.h"
#include "ui_collection.h"

namespace afterhours {

namespace ui {

static inline void force_layout_and_print(
    Entity &root,
    window_manager::Resolution resolution = window_manager::Resolution()) {
    auto &ui_coll = UICollectionHolder::get().collection;
    ui_coll.merge_entity_arrays();
    auto comps = EntityQuery(ui_coll, {.ignore_temp_warning = true})
                     .whereHasComponent<ui::UIComponent>()
                     .gen();

    EntityID max_id = 0;
    for (Entity &entity : comps) {
        max_id = std::max(max_id, entity.id);
    }
    std::vector<Entity *> components(static_cast<size_t>(max_id) + 1, nullptr);
    for (Entity &entity : comps) {
        components[entity.id] = &entity;
    }

    if (resolution.width == 0 || resolution.height == 0) {
        Entity &e =
            EntityQuery()
                .whereHasComponent<window_manager::ProvidesCurrentResolution>()
                .gen_first_enforce();
        resolution = e.get<window_manager::ProvidesCurrentResolution>()
                         .current_resolution;
    }

    ui::AutoLayout::autolayout(root.get<ui::UIComponent>(), resolution,
                               components);
    print_debug_autolayout_tree(root, root.get<ui::UIComponent>());
}

enum struct InputValidationMode { None, LogOnly, Assert };

constexpr static InputValidationMode validation_mode =
#if defined(AFTER_HOURS_INPUT_VALIDATION_ASSERT)
    InputValidationMode::Assert;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_LOG_ONLY)
    InputValidationMode::LogOnly;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_NONE)
    InputValidationMode::None;
#else
    InputValidationMode::LogOnly;
#endif

// NOTE: i tried to write this as a constexpr function but
// the string joining wasnt working for me for some reason
#define validate_enum_has_value(enum_name, name, reason)                    \
    do {                                                                    \
        if constexpr (validation_mode == InputValidationMode::None) {       \
            /* Validation disabled - skip */                                \
        } else if constexpr (!magic_enum::enum_contains<enum_name>(name)) { \
            if constexpr (validation_mode == InputValidationMode::Assert) { \
                static_assert(false, "InputAction missing value '" name     \
                                     "'. Input used to " reason);           \
            } else if constexpr (validation_mode ==                         \
                                 InputValidationMode::LogOnly) {            \
                log_warn("InputAction missing value '" name                 \
                         "'. Input used to " reason);                       \
            }                                                               \
        }                                                                   \
    } while (0);

// Initialize the UI plugin. Creates the root UI entity and all singletons
// in the UI collection. Singletons are also registered in the default
// collection so external code (toast, modal, game code) can find them.
// Returns a reference to the root entity.
template<typename InputAction>
static Entity &init_ui_plugin() {
    auto &ui_coll = UICollectionHolder::get().collection;
    Entity &ui_root = ui_coll.createPermanentEntity();
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    // Grab the shared_ptr before anything else merges temp_entities
    auto root_shared = ui_coll.temp_entities.back();
#endif

    // UIContext
    ui_root.addComponent<UIContext<InputAction>>();
    ui_coll.registerSingleton<UIContext<InputAction>>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    EntityHelper::registerSingleton<UIContext<InputAction>>(ui_root);
#endif

    // FontManager
    ui_root.addComponent<ui::FontManager>()
        .load_font(UIComponent::DEFAULT_FONT, get_default_font())
        .load_font(UIComponent::SYMBOL_FONT, get_default_font())
        .load_font(UIComponent::UNSET_FONT, get_unset_font());
    ui_coll.registerSingleton<ui::FontManager>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    EntityHelper::registerSingleton<ui::FontManager>(ui_root);
#endif

    // TextMeasureCache
    auto &text_cache = ui_root.addComponent<ui::TextMeasureCache>();
    text_cache.set_measure_function([](std::string_view text,
                                       std::string_view font_name,
                                       float font_size, float spacing) {
        auto font_manager = EntityHelper::get_singleton_cmp<ui::FontManager>();
        if (!font_manager) {
            return Vector2Type{0.0f, 0.0f};
        }
        const std::string font_name_str(font_name);
        const std::string text_str(text);
        Font font = font_manager->get_font(font_name_str);
        return measure_text(font, text_str.c_str(), font_size, spacing);
    });
    ui_coll.registerSingleton<ui::TextMeasureCache>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    EntityHelper::registerSingleton<ui::TextMeasureCache>(ui_root);
#endif

    // UIEntityMappingCache
    ui_root.addComponent<ui::UIEntityMappingCache>();
    ui_coll.registerSingleton<ui::UIEntityMappingCache>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    EntityHelper::registerSingleton<ui::UIEntityMappingCache>(ui_root);
#endif

    // DragGroupState
    ui_root.addComponent<ui::DragGroupState>();
    ui_coll.registerSingleton<ui::DragGroupState>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    EntityHelper::registerSingleton<ui::DragGroupState>(ui_root);
#endif

    // Root UI component
    ui_root.addComponent<ui::AutoLayoutRoot>();
    ui_root.addComponent<ui::UIComponentDebug>("ui_root");
    ui_root.addComponent<ui::UIComponent>(ui_root.id)
        .set_desired_width(ui::screen_pct(1.f))
        .set_desired_height(ui::screen_pct(1.f))
        .enable_font(ui::UIComponent::DEFAULT_FONT,
                     afterhours::ui::pixels(75.f));

    // Validate InputAction enum
    validate_enum_has_value(InputAction, "None", "any unmapped input");
    validate_enum_has_value(InputAction, "WidgetMod",
                            "while held, press WidgetNext to do WidgetBack");
    validate_enum_has_value(InputAction, "WidgetNext",
                            "'tab' forward between ui elements");
    validate_enum_has_value(InputAction, "WidgetBack",
                            "'tab' back between ui elements");
    validate_enum_has_value(InputAction, "WidgetPress", "click on element");

    // In split-collection mode, also add root entity to default collection so
    // that screen systems (which iterate default collection entities via
    // for_each_with) can find it. In single-collection mode this is a no-op
    // since ui_coll IS the default collection.
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
    auto &default_coll = EntityHelper::get_default_collection();
    default_coll.temp_entities.push_back(root_shared);
    default_coll.permanant_ids.insert(ui_root.id);
#endif

    return ui_root;
}

// Helper: run a list of systems on UI collection entities
inline void run_systems_on_ui_entities(
    std::vector<std::unique_ptr<SystemBase>> &systems, float dt,
    bool is_render = false) {
    auto &ui_coll = UICollectionHolder::get().collection;
    ui_coll.merge_entity_arrays();

    for (auto &system : systems) {
        if (!system->should_run(dt)) continue;

        auto &entities = ui_coll.get_entities_for_mod();

        system->once(dt);
        system->on_iteration_begin(dt);
        for (auto &entity : entities) {
            if (!entity) continue;
            if (system->include_derived_children)
                system->for_each_derived(*entity, dt);
            else
                system->for_each(*entity, dt);
        }
        system->on_iteration_end(dt);
        system->after(dt);

        if (is_render) {
            const SystemBase &csys = *system;
            csys.once(dt);
            csys.on_iteration_begin(dt);
            for (auto &entity : entities) {
                if (!entity) continue;
                const Entity &e = *entity;
                if (csys.include_derived_children)
                    csys.for_each_derived(e, dt);
                else
                    csys.for_each(e, dt);
            }
            csys.on_iteration_end(dt);
            csys.after(dt);
        }

        ui_coll.merge_entity_arrays();
    }
}

// Bridge system: runs ClearUIComponentChildren + BeginUIContextManager
// on UI collection entities.
template<typename InputAction>
struct UIPluginPreUpdateBridge : System<> {
    std::vector<std::unique_ptr<SystemBase>> systems;

    UIPluginPreUpdateBridge() {
        systems.push_back(std::make_unique<ui::ClearUIComponentChildren>());
        systems.push_back(
            std::make_unique<ui::BeginUIContextManager<InputAction>>());
    }

    bool should_iterate() const override { return false; }

    virtual void once(float dt) override {
        run_systems_on_ui_entities(systems, dt);
    }
};

// Bridge system: runs all post-user-code UI update systems on UI collection
// entities.
template<typename InputAction>
struct UIPluginPostUpdateBridge : System<> {
    std::vector<std::unique_ptr<SystemBase>> systems;

    UIPluginPostUpdateBridge() {
        systems.push_back(
            std::make_unique<ui::UpdateDropdownOptions<InputAction>>());
        systems.push_back(std::make_unique<ui::ClearVisibity>());
        // Pre-layout drag handling: hide dragged entity + insert spacer before
        // BuildUIEntityMapping so the spacer is included in the mapping cache
        // and processed by RunAutoLayout.
        systems.push_back(
            std::make_unique<ui::HandleDragGroupsPreLayout<InputAction>>());
        systems.push_back(std::make_unique<ui::BuildUIEntityMapping>());
        systems.push_back(std::make_unique<ui::RunAutoLayout>());
        systems.push_back(std::make_unique<ui::MeasureScrollViews>());
        systems.push_back(std::make_unique<
                          ui::TrackIfComponentWillBeRendered<InputAction>>());
        systems.push_back(std::make_unique<ui::HandleTabbing<InputAction>>());
        systems.push_back(
            std::make_unique<ui::InputExclusivitySystem<InputAction>>());
        // After InputExclusivitySystem so the input gates it installs are in
        // place, and before every consumer of hot/active.
        systems.push_back(
            std::make_unique<ui::ResolveHitTarget<InputAction>>());
        // Before HandleClicks: a press on the bar must not also click the row
        // behind it.
        systems.push_back(
            std::make_unique<ui::HandleScrollbarDrag<InputAction>>());
        systems.push_back(std::make_unique<ui::HandleClicks<InputAction>>());
        systems.push_back(
            std::make_unique<ui::HandleTrayNavigation<InputAction>>());
        systems.push_back(
            std::make_unique<ui::HandleScrollInput<InputAction>>());
        // After it: the view that moved drives its sync_group.
        systems.push_back(std::make_unique<ui::SyncScrollViews>());
        systems.push_back(
            std::make_unique<ui::CloseDropdownOnClickOutside<InputAction>>());
        systems.push_back(std::make_unique<ui::HandleDrags<InputAction>>());
        // Post-layout drag handling: detect drag start, compute hover position,
        // create floating overlay.
        systems.push_back(
            std::make_unique<ui::HandleDragGroupsPostLayout<InputAction>>());
        systems.push_back(std::make_unique<ui::HandleLeftRight<InputAction>>());
        systems.push_back(
            std::make_unique<ui::HandleSelectOnFocus<InputAction>>());
        systems.push_back(
            std::make_unique<ui::ComputeVisualFocusId<InputAction>>());
        systems.push_back(
            std::make_unique<ui::EndUIContextManager<InputAction>>());
    }

    bool should_iterate() const override { return false; }

    virtual void once(float dt) override {
        run_systems_on_ui_entities(systems, dt);
        // Before cleanup(), which is what actually frees what this marks.
        imm::retire_unbuilt_ui_elements();
        UICollectionHolder::get().collection.cleanup();
    }
};

// Bridge system: runs UI render systems on UI collection entities.
template<typename InputAction>
struct UIPluginRenderBridge : System<> {
    std::vector<std::unique_ptr<SystemBase>> systems;

    UIPluginRenderBridge(InputAction toggle_debug, bool use_batched) {
        if (use_batched) {
            systems.push_back(
                std::make_unique<ui::RenderBatched<InputAction>>());
        } else {
            systems.push_back(std::make_unique<ui::RenderImm<InputAction>>());
        }
        // After the renderer (bar on top of content), before the debug overlay.
        systems.push_back(std::make_unique<ui::RenderScrollbars<InputAction>>());
        systems.push_back(
            std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
                toggle_debug));
    }

    bool should_iterate() const override { return false; }

    virtual void once(float dt) override {
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
        // Clear the visible text registry before each render pass so
        // expect_text only matches text drawn in the current frame.
        if (testing::test_input::detail::test_mode) {
            testing::VisibleTextRegistry::instance().clear();
        }
#endif
        run_systems_on_ui_entities(systems, dt, /*is_render=*/true);
    }
};

// --- Registration functions ---

template<typename InputAction>
static void enforce_singletons(SystemManager &) {
    // Compile-time validation only. UI singletons live in the UI collection
    // and are accessed via bridge systems, so EnforceSingleton systems
    // (which iterate default collection) are not needed.
    validate_enum_has_value(InputAction, "None", "any unmapped input");
    validate_enum_has_value(InputAction, "WidgetMod",
                            "while held, press WidgetNext to do WidgetBack");
    validate_enum_has_value(InputAction, "WidgetNext",
                            "'tab' forward between ui elements");
    validate_enum_has_value(InputAction, "WidgetBack",
                            "'tab' back between ui elements");
    validate_enum_has_value(InputAction, "WidgetPress", "click on element");
}

template<typename InputAction>
static void register_before_ui_updates(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<ui::UIPluginPreUpdateBridge<InputAction>>());
}

template<typename InputAction>
static void register_after_ui_updates(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<ui::UIPluginPostUpdateBridge<InputAction>>());
}

/// setup() with an explicit starting resolution.
template<typename InputAction = DefaultAction, typename... UserSystems>
static Entity &setup_with_resolution(SystemManager &sm,
                                     const window_manager::Resolution &rez,
                                     UserSystems &&...user_systems) {
    Entity &ui_root = init_ui_plugin<InputAction>();

    // RunAutoLayout needs this; init_ui_plugin does not create it. Skip if the
    // app already owns one.
    if (!EntityHelper::has_singleton<
            window_manager::ProvidesCurrentResolution>()) {
        window_manager::add_singleton_components(ui_root, rez, 60);
    }

    register_before_ui_updates<InputAction>(sm);
    // Fold: one register_update_system() call per system, in argument order.
    (sm.register_update_system(std::forward<UserSystems>(user_systems)), ...);
    register_after_ui_updates<InputAction>(sm);
    return ui_root;
}

/// Creates the UI root and registers the update systems in the only order that
/// works, with the caller's systems in between. Misordering these by hand
/// fails silently: the UI runs and never lays out.
///
///     ui::setup<>(systems, std::make_unique<MySystem>());
///
/// Defaults InputAction to DefaultAction. Returns the UI root. The pieces this
/// composes stay public for custom ordering.
template<typename InputAction = DefaultAction, typename... UserSystems>
static Entity &setup(SystemManager &sm, UserSystems &&...user_systems) {
    return setup_with_resolution<InputAction>(
        sm, window_manager::Resolution{1280, 720},
        std::forward<UserSystems>(user_systems)...);
}

template<typename InputAction>
static void register_render_systems(
    SystemManager &sm, InputAction toggle_debug = InputAction::None) {
    sm.register_render_system(
        std::make_unique<ui::UIPluginRenderBridge<InputAction>>(
            toggle_debug, /*use_batched=*/false));
}

template<typename InputAction>
static void register_batched_render_systems(
    SystemManager &sm, InputAction toggle_debug = InputAction::None) {
    sm.register_render_system(
        std::make_unique<ui::UIPluginRenderBridge<InputAction>>(
            toggle_debug, /*use_batched=*/true));
}

/// Conventional key bindings for the UI's own actions.
///
/// Entries are matched to the caller's enum *by name*, so an app with extra
/// actions of its own gets the widget/text bindings for free and keeps the
/// rest; names the enum does not have are skipped. `afterhours::keys` values
/// are GLFW/raylib codes, so this is backend-independent.
///
///     auto m = ui::default_keymap<MyAction>();
///     m[(int)MyAction::Jump] = {keys::SPACE};   // add your own on top
template<typename InputAction>
static input::ProvidesInputMapping::GameMapping default_keymap() {
    using KeyChord = input::KeyChord;
    input::ProvidesInputMapping::GameMapping mapping;

    auto bind = [&mapping](std::string_view name, input::ValidInputs inputs) {
        if (auto value = magic_enum::enum_cast<InputAction>(name))
            mapping[static_cast<int>(*value)] = std::move(inputs);
    };

    // Focus movement. WidgetMod is held, not tapped: holding it turns
    // WidgetNext into WidgetBack, which is how Shift+Tab reverses.
    bind("WidgetMod", {keys::LEFT_SHIFT, keys::RIGHT_SHIFT});
    bind("WidgetNext", {keys::TAB});
    bind("WidgetBack", {keys::BACKSPACE});
    bind("WidgetPress", {keys::ENTER, keys::SPACE});
    bind("WidgetUp", {keys::UP});
    bind("WidgetDown", {keys::DOWN});
    bind("WidgetLeft", {keys::LEFT});
    bind("WidgetRight", {keys::RIGHT});
    bind("MenuBack", {keys::ESCAPE});

    // Text editing. BACKSPACE is deliberately double-bound with WidgetBack
    // above: the text systems only consume it while a field has focus.
    constexpr uint8_t CMD = KeyChord::MOD_SUPER;
    constexpr uint8_t CTRL = KeyChord::MOD_CTRL;
    constexpr uint8_t SHIFT = KeyChord::MOD_SHIFT;
    // Every editing chord is bound for both macOS (cmd) and elsewhere (ctrl).
    auto bind_chord = [&bind](std::string_view name, int key) {
        bind(name, {KeyChord{key, CMD}, KeyChord{key, CTRL}});
    };
    bind("TextBackspace", {keys::BACKSPACE});
    bind("TextDelete", {keys::DELETE_KEY});
    bind("TextHome", {keys::HOME});
    bind("TextEnd", {keys::END});
    bind_chord("TextCopy", keys::C);
    bind_chord("TextCut", keys::X);
    bind_chord("TextPaste", keys::V);
    bind_chord("TextUndo", keys::Z);
    bind_chord("TextSelectAll", keys::A);
    bind_chord("TextDeleteWordBack", keys::BACKSPACE);
    bind_chord("TextDeleteWordForward", keys::DELETE_KEY);
    bind_chord("TextWordLeft", keys::LEFT);
    bind_chord("TextWordRight", keys::RIGHT);
    bind("TextRedo", {KeyChord{keys::Z, static_cast<uint8_t>(CMD | SHIFT)},
                      KeyChord{keys::Z, static_cast<uint8_t>(CTRL | SHIFT)}});
    bind("TextSelectLeft", {KeyChord{keys::LEFT, SHIFT}});
    bind("TextSelectRight", {KeyChord{keys::RIGHT, SHIFT}});

    return mapping;
}

// NOTE: raylib-only. The body uses raylib's render-texture API
// (render_texture.texture / draw_texture_pro); the Metal/sokol backend drives
// its own frame loop (see floatinghotel main.cpp), so run() is not built there.
#if defined(AFTER_HOURS_USE_RAYLIB)
/// Opens a window, wires input + UI, and runs the frame loop until the window
/// closes. Returns a main()-style exit code.
///
///     int main() { return ui::run<>({.title = "hello"},
///                                   std::make_unique<MySystem>()); }
///
/// This is the whole-app convenience path. Anything it decides for you --
/// keymap, resolution singleton, clear color, system order -- is reachable
/// piecemeal via graphics::init + default_keymap + setup + the loop below.
template<typename InputAction = DefaultAction, typename... UserSystems>
static int run(const graphics::Config &cfg, UserSystems &&...user_systems) {
    if (!graphics::init(cfg)) {
        log_error("graphics::init failed; cannot run");
        return 1;
    }

    // Skipped if the app already made one, same as the resolution singleton in
    // setup_with_resolution.
    if (!EntityHelper::has_singleton<input::InputCollector>()) {
        input::add_singleton_components(EntityHelper::createEntity(),
                                        default_keymap<InputAction>());
    }

    SystemManager systems;
    // Input must tick before the UI reads the collector this frame.
    input::register_update_systems(systems);
    setup_with_resolution<InputAction>(
        systems, window_manager::Resolution{cfg.width, cfg.height},
        std::forward<UserSystems>(user_systems)...);
    register_render_systems<InputAction>(systems);

    while (!graphics::window_should_close()) {
        graphics::begin_frame();
        // ThemeDefaults, not ctx.theme: BeginUIContextManager overwrites the
        // context's copy from it every frame, so this is the durable one.
        graphics::clear_background(imm::ThemeDefaults::get().theme.background);
        systems.run(graphics::get_frame_time());
        graphics::end_frame();

        // begin_frame/end_frame only bind and unbind an offscreen render
        // texture -- nothing reaches the window without this. Skipping it does
        // not just leave the window blank: raylib ticks its frame timer and
        // polls input inside end_drawing, so GetFrameTime stays 0, the fps cap
        // never applies, and no input arrives.
        graphics::begin_drawing();
        auto &render_texture = graphics::get_render_texture();
        // Negative source height flips it: render textures are bottom-up.
        draw_texture_pro(
            render_texture.texture,
            RectangleType{0, 0,
                          static_cast<float>(render_texture.texture.width),
                          -static_cast<float>(render_texture.texture.height)},
            RectangleType{0, 0,
                          static_cast<float>(graphics::get_screen_width()),
                          static_cast<float>(graphics::get_screen_height())},
            Vector2Type{0, 0}, 0.f, colors::UI_WHITE);
        graphics::end_drawing();
    }

    graphics::shutdown();
    return 0;
}
#endif

}  // namespace ui

}  // namespace afterhours
