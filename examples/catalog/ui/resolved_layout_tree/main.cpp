#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/e2e_testing/ui_commands.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static EntityID panel_id = -1;

struct BuildUI : System<DefaultUIContext> {
    void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
        auto panel = vstack(ctx, mk(entity),
                            ComponentConfig{}
                                .with_size(ComponentSize{pixels(300), children()})
                                .with_padding(Padding::all(pixels(10)))
                                .with_gap(pixels(8))
                                .with_debug_name("panel"));
        panel_id = panel.id();
        for (int i = 0; i < 3; ++i)
            div(ctx, mk(panel.ent(), i),
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(280), pixels(30)})
                    .with_debug_name(std::format("row{}", i)));
    }
};

struct PrintResolvedTree : System<> {
    void once(float) override {
        auto &panel = UICollectionHolder::getEntityForIDEnforce(panel_id);
        auto &cmp = panel.get<UIComponent>();
        float used = cmp.computed_padd[Axis::top] + cmp.computed_padd[Axis::bottom];
        for (EntityID id : cmp.children) {
            auto &child = UICollectionHolder::getEntityForIDEnforce(id);
            Rectangle r = child.get<UIComponent>().rect();
            log_info("{} at y={} h={}", child.get<UIComponentDebug>().name_value, r.y,
                     r.height);
            used += r.height;
        }
        used += cmp.gap * static_cast<float>(cmp.children.size() - 1);
        log_info("panel h={}, children + padding + gaps = {}", cmp.rect().height,
                 used);

        std::string xml;
        testing::ui_commands::dump_ui_node(xml, panel, 0);
        log_info("dump_ui:\n{}", xml);
    }
};

int main() {
    SystemManager systems;
    ui::setup_with_resolution<>(systems, {800, 600}, std::make_unique<BuildUI>());
    systems.register_update_system(std::make_unique<PrintResolvedTree>());
    systems.run(1.f / 60.f);
    return 0;
}
