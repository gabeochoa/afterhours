#include <cstdio>

#include "../../shared/vector.h"

#include "../../../../ah.h"
#include "../../../../src/plugins/ui.h"

enum struct Action { Up, Down };

using namespace afterhours::ui;

template <typename ActionEnum> void validate(bool full) {
  if (full)
    validate_enum_has_value(ActionEnum, "Up", "move up");
  else
    validate_enum_has_value(ActionEnum, "Jump", "jump");
}

int main(int argc, char **) {
  validate<Action>(argc > 1);
  std::printf("ok\n");
  return 0;
}
