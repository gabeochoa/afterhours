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

    static constexpr size_t TRACK = 900;

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

    static float presence_of(Entity &entity) {
        if (!entity.has<motion::HasTracks>()) return 1.f;
        auto &track = entity.get<motion::HasTracks>().track<float>(TRACK);
        return track.started() ? track.value() : 1.f;
    }

    static ui::imm::ElementResult host(ui::imm::HasUIContext auto &ctx,
                                       ui::imm::EntityParent ep_pair,
                                       bool &open, Config config = {}) {
        using namespace ui;
        using namespace ui::imm;
        auto [entity, parent] = deref(ep_pair);
        Presentation &p = entity.template addComponentIfMissing<Presentation>();
        auto &presence = entity.template addComponentIfMissing<motion::HasTracks>()
                             .template track<float>(TRACK);

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

        entity.template toggleComponent<ShouldHide>(!p.presenting());
        if (!p.presenting()) return {false, entity};
        auto result = div(ctx, ep_pair,
                          ComponentConfig{}
                              .with_size(ComponentSize{percent(1.0f), percent(1.0f)})
                              .with_debug_name("presentation"));
        entity.template addComponentIfMissing<HasOpacity>().value *= presence_of(entity);
        return result;
    }
};

}
