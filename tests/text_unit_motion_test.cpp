#include <afterhours/src/plugins/ui.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
  printf("Running text unit motion tests...\n\n");

  check(near(stagger_delay(0, 5, 0.04f, StaggerOrder::Forward), 0.f) &&
            near(stagger_delay(4, 5, 0.04f, StaggerOrder::Forward), 0.16f),
        "forward stagger grows with index");
  check(near(stagger_delay(0, 5, 0.04f, StaggerOrder::Reverse), 0.16f) &&
            near(stagger_delay(4, 5, 0.04f, StaggerOrder::Reverse), 0.f),
        "reverse stagger shrinks with index");
  check(near(stagger_delay(2, 5, 0.04f, StaggerOrder::CenterOut), 0.f) &&
            near(stagger_delay(0, 5, 0.04f, StaggerOrder::CenterOut), 0.08f) &&
            near(stagger_delay(4, 5, 0.04f, StaggerOrder::CenterOut), 0.08f),
        "centre-out stagger is zero in the middle and symmetric");
  {
    bool in_range = true;
    for (size_t i = 0; i < 16; ++i) {
      const float d = stagger_delay(i, 16, 0.04f, StaggerOrder::Random);
      in_range &= d >= 0.f && d <= 0.04f * 15.f + 1e-4f;
    }
    check(in_range && near(stagger_delay(3, 16, 0.04f, StaggerOrder::Random),
                           stagger_delay(3, 16, 0.04f, StaggerOrder::Random)),
          "random stagger stays in range and is deterministic");
    bool permuted = true, not_forward = false;
    std::vector<float> delays;
    for (size_t i = 0; i < 5; ++i)
      delays.push_back(stagger_delay(i, 5, 0.05f, StaggerOrder::Random));
    std::vector<float> sorted = delays;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < 5; ++i) {
      permuted &= near(sorted[i], 0.05f * float(i));
      not_forward |= !near(delays[i], 0.05f * float(i));
    }
    check(permuted && not_forward,
          "random stagger is a permutation of the forward delays, not forward order");
  }

  {
    Entity &e = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    auto &st = e.addComponent<HasTextUnitMotion>();
    st.cfg.unit = TextUnit::Word;
    st.cfg.stagger = 0.1f;
    update_text_units(e, "one two three");
    check(st.units.size() == 3, "words are split into units");
    const auto &floats = e.get<motion::HasTracks>().floats;
    check(floats.count(unit_key(2, UnitProp::Opacity)) == 1 &&
              floats.at(unit_key(2, UnitProp::Opacity)).active(),
          "every unit gets tracks on first sight");
    check(near(unit_draw(e, 0).opacity, 0.f) && near(unit_draw(e, 0).y, 8.f),
          "units start at their from values");

    for (auto &[k, tr] : e.get<motion::HasTracks>().floats)
      tr.from(tr.target());
    update_text_units(e, "one two four");
    check(!floats.at(unit_key(0, UnitProp::Opacity)).active() &&
              !floats.at(unit_key(1, UnitProp::Opacity)).active() &&
              floats.at(unit_key(2, UnitProp::Opacity)).active(),
          "only the unit whose text changed animates again");

    update_text_units(e, "one two four five");
    check(floats.at(unit_key(3, UnitProp::Opacity)).active() &&
              !floats.at(unit_key(0, UnitProp::Opacity)).active(),
          "an appended word animates alone");
    check(text_units_active(e), "text_units_active sees the running unit");
    e.cleanup = true;
    EntityHelper::cleanup();
  }

  {
    Arena arena(64 * 1024);
    RenderCommandBuffer buffer(arena);
    std::vector<TextUnitInstance> units;
    for (int i = 0; i < 40; ++i)
      units.push_back({"x", float(i) * 10.f, float(i), 20.f, Color{255, 255, 255, 255}});
    buffer.add_text_units({0.f, 0.f, 400.f, 24.f}, units.data(), units.size(), "default", 3, 42);
    check(buffer.commands().size() == 1, "forty units batch into one command");
    const auto &cmd = buffer.commands()[0];
    check(cmd.type == RenderPrimitiveType::TextUnits && cmd.data.text_units.count == 40 &&
              cmd.data.text_units.units[39].x == 390.f && cmd.layer == 3 && cmd.entity_id == 42,
          "the batched command carries every instance, layer and entity");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
