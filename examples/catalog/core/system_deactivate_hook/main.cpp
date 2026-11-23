#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"

using namespace afterhours;

struct PanelSearch : System<> {
    bool panel_open = false;
    std::vector<std::string> corpus;
    int runs = 0;

    bool should_run(const float) const override { return panel_open; }

    void on_activate(const float) override {
        corpus = {"transcript one", "transcript two", "transcript three"};
        std::printf("panel opened: built corpus of %zu entries\n",
                    corpus.size());
    }

    void on_deactivate(const float) override {
        std::printf("panel closed: releasing corpus of %zu entries\n",
                    corpus.size());
        corpus.clear();
        corpus.shrink_to_fit();
    }

    void once(const float) override { ++runs; }
};

int main() {
    auto owned = std::make_unique<PanelSearch>();
    PanelSearch *panel = owned.get();

    SystemManager systems;
    systems.register_update_system(std::move(owned));

    systems.run(1.f / 60.f);
    panel->panel_open = true;
    systems.run(1.f / 60.f);
    systems.run(1.f / 60.f);
    panel->panel_open = false;
    systems.run(1.f / 60.f);
    systems.run(1.f / 60.f);

    std::printf("ran %d times while open, corpus holds %zu entries now\n",
                panel->runs, panel->corpus.size());
    return (panel->runs == 2 && panel->corpus.empty()) ? 0 : 1;
}
