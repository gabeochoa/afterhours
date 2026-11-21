#pragma once

#include <optional>
#include <string>

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/entity_query.h"
#include "animation.h"
#include "ui/element_result.h"
#include "ui/imm_components.h"

namespace afterhours {

// Shared present/dismiss path for full-screen screens (menu, pause, loading):
// modal.h owns the same lifecycle for dialog panels, but it forces a backdrop,
// a centered panel and the modal stack. A screen that draws its own content
// (imm children or hand-drawn passes under the returned container) needs only
// the lifecycle: presence fade, an input gate for everything outside the
// screen, and focus restore on dismiss. That slice lives here; screens keep
// their own layout and drawing.
struct presentation {
    struct Config {
        std::optional<motion::Mode> enter_motion;
        std::optional<motion::Mode> exit_motion;
        Config &with_motion(motion::Mode enter, motion::Mode exit) {
            enter_motion = std::move(enter);
            exit_motion = std::move(exit);
            return *this;
        }
    };

    struct Presentation : BaseComponent {
        enum struct Phase { Hidden, Entering, Visible, Exiting };
        Phase phase = Phase::Hidden;
        bool was_open_last_frame = false;
        std::optional<motion::Mode> exit_motion;
        EntityID previously_focused_element = -1;
        bool presenting() const { return phase != Phase::Hidden; }
        bool exiting() const { return phase == Phase::Exiting; }
    };

    static constexpr size_t kTrack = 900;

    static bool in_tree(EntityID root_id, EntityID search_id) {
        for (int depth = 0; depth < 64 && search_id != -1; ++depth) {
            if (root_id == search_id) return true;
            auto opt = ui::UICollectionHolder::getEntityForID(search_id);
            if (!opt.valid() || !opt->has<ui::UIComponent>()) return false;
            search_id = opt->get<ui::UIComponent>().parent;
        }
        return false;
    }

    static std::string gate_name(EntityID id) {
        return "presentation:" + std::to_string(id);
    }

    // Presence in [0,1] for hand-drawn content under a presented screen.
    static float presence_of(Entity &entity) {
        if (!entity.has<motion::HasTracks>()) return 1.f;
        auto &track = entity.get<motion::HasTracks>().track<float>(kTrack);
        return track.started() ? track.value() : 1.f;
    }

    // Call every frame with the screen's `open` flag. Returns the screen
    // container while presenting (including while exiting); add children to
    // it, or gate hand-drawn passes on the result and presence_of(ent).
    // Keep calling after `open` flips false until this returns false once,
    // or the input gate and saved focus are never released.
    static ui::imm::ElementResult host(ui::imm::HasUIContext auto &ctx,
                                       ui::imm::EntityParent ep_pair,
                                       bool &open, Config config = {}) {
        using namespace ui;
        using namespace ui::imm;
        auto [entity, parent] = deref(ep_pair);
        Presentation &p = entity.template addComponentIfMissing<Presentation>();
        auto &presence = entity.template addComponentIfMissing<motion::HasTracks>()
                             .template track<float>(kTrack);

        const auto finish_close = [&] {
            ctx.remove_input_gate(gate_name(entity.id));
            if (p.previously_focused_element >= 0 &&
                (ctx.focus_id == ctx.ROOT || in_tree(entity.id, ctx.focus_id)))
                ctx.set_focus(p.previously_focused_element);
            p.phase = Presentation::Phase::Hidden;
        };
        const auto sync_gate = [&] {
            const bool exiting = p.exiting();
            const EntityID self = entity.id;
            ctx.add_input_gate(gate_name(self), [self, exiting](EntityID id) {
                return id == -1 || (!exiting && in_tree(self, id));
            });
            if (!in_tree(self, ctx.focus_id)) ctx.focus_id = ctx.ROOT;
            if (!in_tree(self, ctx.hot_id)) ctx.hot_id = ctx.ROOT;
            if (!in_tree(self, ctx.active_id)) ctx.active_id = ctx.ROOT;
        };

        if (open && !p.was_open_last_frame) {
            if (!p.exiting()) {
                p.previously_focused_element = ctx.focus_id;
                p.exit_motion = config.exit_motion;
                if (config.enter_motion.has_value() && !motion::is_instant())
                    presence.from(0.f).to(1.f, *config.enter_motion);
                else
                    presence.from(1.f);
            } else {
                presence.to(1.f, config.enter_motion.value_or(
                                     motion::Spring::smooth()));
            }
            p.phase = Presentation::Phase::Entering;
            sync_gate();
        } else if (!open && p.was_open_last_frame) {
            if (p.exit_motion.has_value() && !motion::is_instant()) {
                p.phase = Presentation::Phase::Exiting;
                presence.to(0.f, *p.exit_motion);
                sync_gate();
            } else {
                finish_close();
            }
        }
        if (p.phase == Presentation::Phase::Entering && !presence.active())
            p.phase = Presentation::Phase::Visible;
        if (p.exiting() && !presence.active()) finish_close();
        p.was_open_last_frame = open;

        if (!p.presenting()) {
            entity.template addComponentIfMissing<ShouldHide>();
            return {false, entity};
        }
        entity.template removeComponentIfExists<ShouldHide>();
        auto result = div(ctx, ep_pair,
                          ComponentConfig{}
                              .with_size(ComponentSize{percent(1.0f), percent(1.0f)})
                              .with_debug_name("presentation"));
        entity.template addComponentIfMissing<HasOpacity>().value *= presence_of(entity);
        return result;
    }
};

}  // namespace afterhours
