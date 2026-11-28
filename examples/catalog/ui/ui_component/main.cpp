
#include <source_location>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../shared/vector.h"

namespace afterhours {

struct Transform : public BaseComponent {
    Transform(float x, float y) {
        position.x = x;
        position.y = y;
    }

    virtual ~Transform() {}

    [[nodiscard]] vec2 pos() const { return position; }
    void update(vec2 v) { position = v; }

   private:
    vec2 position;
};

}  // namespace afterhours

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

UIComponent &_fetch_component(Entity &ent) {
    if (ent.is_missing<UIComponent>()) {
        // this is the first time we are looking in here
    }
    return ent.get<UIComponent>();
}

struct UITest : System<DefaultUIContext> {
    virtual void for_each_with(Entity &entity, DefaultUIContext &context,
                               float) override {
        auto elem = imm::div(context, mk(entity));
        if (button(context, mk(elem.ent()))) {
            std::cout << "button clicked" << std::endl;
        }
    }
};

int main(int, char **) {
    auto app = make_logic_ui<UITest>({1280, 720}, std::make_unique<UITest>());
    app.run(1, 1.f);

    return 0;
}
