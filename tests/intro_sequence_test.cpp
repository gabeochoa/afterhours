#include <afterhours/src/plugins/intro_sequence.h>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace afterhours;
static int checks = 0, failures = 0;
static void check(bool c, const char *w) { ++checks; if (!c) { ++failures; std::printf("  FAIL: %s\n", w); } }
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
using S = IntroSequence::Step;
static IntroSequence seq_of(std::vector<S> s) { IntroSequence q; for (auto &x : s) q.add(std::move(x)); return q; }

int main() {
  {
    std::vector<int> drawn; std::vector<float> alphas;
    auto mk = [&](int id) { return S{.fade_in=.5f,.hold=1.f,.fade_out=.5f,.draw=[&, id](float a){drawn.push_back(id);alphas.push_back(a);}}; };
    auto q = seq_of({mk(0), mk(1)}); int done = 0; q.on_finished([&]{done++;});
    q.update(.25f); check(q.index()==0 && near(q.alpha(),.5f), "fade-in mid alpha");
    q.draw(); check(drawn.size()==1 && drawn[0]==0 && near(alphas[0],.5f), "draw cb step0 alpha");
    q.update(.25f); check(near(q.alpha(),1.f), "hold alpha 1");
    q.update(1.f); check(near(q.alpha(),1.f), "fade-out starts at 1");
    q.update(.5f); check(q.index()==1, "advances in order");
    q.update(2.f); check(q.finished() && done==1, "finishes after last fade-out");
    q.update(1.f); check(done==1, "finish fires once");
  }
  {
    auto q = seq_of({S{.hold=10.f}, S{.hold=10.f,.skippable=false}, S{.hold=10.f}});
    q.skip(); check(q.index()==1, "skip advances one step");
    q.skip(); check(q.index()==1, "non-skippable step ignores skip");
    q.update(20.f); check(q.index()==2, "timed advance past non-skippable");
    q.skip(); check(q.finished(), "skip on last step finishes");
  }
  {
    auto q = seq_of({S{.fade_in=0,.hold=.5f,.fade_out=0,.wait_for_progress=true}, S{.fade_in=0,.hold=.5f,.fade_out=0}});
    q.set_progress(.5f); q.update(10.f); check(q.index()==0, "progress <1 gates");
    q.set_progress(2.f); check(near(q.progress(),1.f), "progress clamped to 1");
    q.update(.4f); check(q.index()==0, "hold runs after progress completes");
    q.update(.2f); check(q.index()==1, "progress step advances after hold");
  }
  { auto q = seq_of({}); int d=0; q.on_finished([&]{d++;}); q.update(.1f); check(q.finished()&&d==1, "empty sequence finishes once"); }
  std::printf("intro_sequence_test: %d/%d checks passed\n", checks-failures, checks);
  return failures ? 1 : 0;
}
