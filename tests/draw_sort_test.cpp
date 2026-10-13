// draw_sort_test.cpp
// sort() had no callers, so its type tiebreak had never moved a scissor off
// what it clips. Opt-in now, which only helps if it is also correct.

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;

namespace {
int checks_run = 0, checks_passed = 0;
void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

// Layers in emission order, for readability in the assertions below.
std::vector<int> layers_of(const RenderCommandBuffer &buf) {
  std::vector<int> out;
  for (const auto &c : buf.commands())
    out.push_back(c.layer);
  return out;
}
} // namespace

int main() {
  printf("=== draw sort tests ===\n\n");

  const RectangleType r{0, 0, 10, 10};
  const Color c{255, 255, 255, 255};

  // Out of layer order on purpose.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_rectangle(r, c, /*layer=*/5, 1);
    buf.add_rectangle(r, c, /*layer=*/1, 2);
    buf.add_rectangle(r, c, /*layer=*/3, 3);

    check(layers_of(buf) == std::vector<int>({5, 1, 3}),
          "unsorted, the buffer keeps emission order");

    buf.sort();
    check(layers_of(buf) == std::vector<int>({1, 3, 5}),
          "sorted, the buffer is ordered by layer");
  }

  // Same layer: emission order is the paint order, so it must survive.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_rectangle(r, c, 2, /*entity=*/10);
    buf.add_rectangle(r, c, 2, /*entity=*/11);
    buf.add_rectangle(r, c, 2, /*entity=*/12);
    buf.sort();
    std::vector<int> ids;
    for (const auto &cmd : buf.commands())
      ids.push_back(cmd.entity_id);
    check(ids == std::vector<int>({10, 11, 12}),
          "within one layer the sort is stable");
  }

  // A scissor must still bracket its geometry afterwards.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_scissor_start(0, 0, 10, 10, 4, 20);
    buf.add_rectangle(r, c, 4, 21);
    buf.add_scissor_end(4, 22);
    buf.sort();

    const auto &cmds = buf.commands();
    check(cmds.size() == 3, "three commands survive the sort");
    if (cmds.size() == 3) {
      check(cmds[0].type == RenderPrimitiveType::ScissorStart &&
                cmds[2].type == RenderPrimitiveType::ScissorEnd,
            "the scissor still wraps the geometry it clips");
    }
  }

  // A shader scope around one entity must survive the sort as a scope.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    graphics::ShaderType shader{};
    buf.add_rectangle(r, c, 5, 1);
    buf.add_shader_start(&shader, 2, 7);
    buf.add_rectangle(r, c, 2, 7);
    buf.add_text(r, "x", "", 10.f, c, TextAlignment::Left, 2, 7);
    buf.add_shader_end(2, 7);
    buf.add_rectangle(r, c, 1, 3);
    buf.sort();
    std::vector<RenderPrimitiveType> types;
    for (const auto &cmd : buf.commands())
      types.push_back(cmd.type);
    check(types == std::vector<RenderPrimitiveType>(
                       {RenderPrimitiveType::Rectangle, RenderPrimitiveType::ShaderStart,
                        RenderPrimitiveType::Rectangle, RenderPrimitiveType::Text,
                        RenderPrimitiveType::ShaderEnd, RenderPrimitiveType::Rectangle}),
          "shader start and end still bracket the entity's commands");
  }

  // A failed arena copy must skip the command, not emit null pointers.
  {
    static std::string errors;
    errors.clear();
    const auto previous_sink = log_sink_fn;
    log_sink_fn = [](const char *level, const char *message) {
      if (level && message &&
          std::string_view(level).find("ERROR") != std::string_view::npos) {
        errors += message;
        errors += '\n';
      }
    };
    const std::string huge(50 * 1024, 'x');

    {
      Arena arena(4096);
      RenderCommandBuffer buf(arena, 1);
      buf.add_text(r, huge, "font", 10.f, c, TextAlignment::Left, 0, 1);
      buf.add_text(r, huge, "font", 10.f, c, TextAlignment::Left, 0, 1);
      check(buf.commands().empty(),
            "text allocation failure emits no command");
    }
    {
      Arena arena(4096);
      RenderCommandBuffer buf(arena, 1);
      buf.add_text(r, "x", huge, 10.f, c, TextAlignment::Left, 0, 2);
      check(buf.commands().empty(),
            "font allocation failure emits no command");
    }
    {
      Arena arena(4096);
      RenderCommandBuffer buf(arena, 1);
      const std::vector<TextUnitInstance> units(
          1024, {"x", 0.f, 0.f, 20.f, c});
      buf.add_text_units(r, units.data(), units.size(), "font", 0, 3);
      check(buf.commands().empty(),
            "text-unit allocation failure emits no command");
    }

    size_t error_count = 0;
    for (size_t pos = errors.find("render arena exhausted"); pos != std::string::npos;
         pos = errors.find("render arena exhausted", pos + 1))
      error_count++;
    check(error_count == 1, "arena exhaustion logs one error");
    log_sink_fn = previous_sink;

    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena, 1);
    buf.add_text(r, "ok", "font", 10.f, c, TextAlignment::Left, 0, 4);
    const auto &cmds = buf.commands();
    check(cmds.size() == 1 && cmds[0].data.text.text &&
              cmds[0].data.text.font_name &&
              std::string(cmds[0].data.text.text) == "ok" &&
              std::string(cmds[0].data.text.font_name) == "font",
          "successful text copy still emits one intact command");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
