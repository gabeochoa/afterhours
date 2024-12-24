

#include "../shared/vector.h"

#include "../../ah.h"
#include "../../src/plugins/autolayout.h"
#include "../../src/plugins/ui.h"

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

} // namespace afterhours

// TODO what happens if we pass void?
enum struct InputAction {};

int main(int, char **) {
  using namespace afterhours;

  {
    auto &entity = EntityHelper::createEntity();
    ui::add_singleton_components<InputAction>(entity);
  }

  SystemManager systems;

  ui::enforce_singletons<InputAction>(systems);

  systems.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
  systems.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());

  for (int i = 0; i < 2; i++) {
    systems.run(1.f);
  }

  return 0;
}
