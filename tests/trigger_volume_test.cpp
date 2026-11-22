#include <afterhours/src/plugins/trigger_volume.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;
using Volume = trigger_volume::Volume;
using Candidate = trigger_volume::Candidate;
using Phase = trigger_volume::Phase;

static int checks_run = 0, checks_passed = 0;
static void check(bool c, const std::string &w) {
  checks_run++;
  if (c) checks_passed++; else fprintf(stderr, "  FAIL: %s\n", w.c_str());
}
static Volume box() { Volume v; v.bounds = {0, 0, 10, 10}; return v; }

int main() {
  printf("Running trigger_volume tests...\n\n");
  {
    auto v = box();
    v.update({{1, 5, 5}}, 0.016f);
    check(v.count() == 1 && v.inside(1) && v.entered.size() == 1 &&
              v.entered[0] == 1 && v.exited.empty(),
          "enter edge on first frame inside");
    v.update({{1, 5, 5}}, 0.016f);
    check(v.entered.empty() && v.exited.empty(), "no edge while staying inside");
    v.update({{1, 50, 5}}, 0.016f);
    check(v.count() == 0 && v.exited.size() == 1 && v.exited[0] == 1 &&
              v.phase() == Phase::Empty,
          "exit edge on leaving");
  }
  {
    auto v = box(); v.required_count = 2;
    v.update({{1, 1, 1}, {2, 2, 2}, {3, 99, 99}}, 0.016f);
    check(v.count() == 2, "two entrants count, outside candidate excluded");
    check(std::fabs(v.progress() - 1.f) < 1e-6f &&
              v.phase() == Phase::Complete,
          "progress 1 and Complete at required count");
    v.update({{1, 1, 1}}, 0.016f);
    check(std::fabs(v.progress() - 0.5f) < 1e-6f &&
              v.phase() == Phase::Occupied,
          "progress 0.5 and Occupied below required");
  }
  {
    auto v = box();
    v.validator = [](EntityID id) { return id != 7; };
    v.update({{7, 5, 5}, {8, 5, 5}}, 0.016f);
    check(v.count() == 1 && !v.inside(7) && v.inside(8) &&
              v.entered.size() == 1,
          "validation rejects entrant");
  }
  {
    auto v = box();
    for (int i = 0; i < 600; ++i) v.update({{1, 5, 5}}, 1.f / 60.f);
    check(v.presence > 0.99f && v.presence <= 1.f, "presence approaches 1, bounded");
    float peak = v.presence;
    for (int i = 0; i < 600; ++i) v.update({}, 1.f / 60.f);
    check(v.presence >= 0.f && v.presence < 0.01f && v.presence <= peak,
          "presence falls toward 0, bounded");
    v.update({{1, 5, 5}}, 10.f);
    check(v.presence >= 0.f && v.presence <= 1.f, "presence bounded on huge dt");
  }
  {
    Volume v; v.bounds = {0, 0, 10, 10, 0, 4};
    v.update({{1, 5, 5, 9}}, 0.016f);
    check(v.count() == 0, "3D volume rejects outside depth");
    v.update({{1, 5, 5, 2}}, 0.016f);
    check(v.count() == 1, "3D volume accepts inside depth");
  }
  {
    struct Pos : BaseComponent {
      float x = 0, y = 0;
      Pos() = default;
      Pos(float x_, float y_) : x(x_), y(y_) {}
    };
    SystemManager sm;
    trigger_volume::register_update_systems<Pos>(
        sm, [](const Entity &e, const Pos &pos) {
          return Candidate{e.id, pos.x, pos.y};
        });
    auto &vol = EntityHelper::createEntity();
    vol.addComponent<trigger_volume::HasTriggerVolume>(
        trigger_volume::AABB{0, 0, 10, 10}, 1);
    auto &actor = EntityHelper::createEntity();
    actor.addComponent<Pos>(5.f, 5.f);
    EntityHelper::merge_entity_arrays();
    sm.run(0.016f);
    check(vol.get<trigger_volume::HasTriggerVolume>().volume.count() == 1,
          "registered system counts entrant");
  }
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
