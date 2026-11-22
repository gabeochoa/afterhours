#define AFTERHOURS_ENABLE_PROFILING 1
#include <afterhours/src/plugins/profiling.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int failures = 0;
static void check(const std::string &actual, const std::string &expected, const char *label) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL %s\nexpected:\n%s\nactual:\n%s\n", label, expected.c_str(), actual.c_str());
        ++failures;
    }
}

int main() {
    check(profiling::snapshot_to_collapsed(profiling::Snapshot{}), "", "empty snapshot");

    profiling::Snapshot snapshot;
    profiling::SystemSample gpu_a;
    gpu_a.name = "gpu";
    gpu_a.phase = SystemPhase::Update;
    gpu_a.total_ms = 1.5;
    profiling::SystemSample gpu_b = gpu_a;
    gpu_b.total_ms = 2.25;
    profiling::SystemSample audio;
    audio.name = "audio";
    audio.phase = SystemPhase::Render;
    audio.total_ms = 4;
    profiling::SystemSample idle;
    idle.name = "idle";
    idle.phase = SystemPhase::FixedUpdate;
    idle.total_ms = 0;
    snapshot.systems = {gpu_a, audio, gpu_b, idle};
    check(profiling::snapshot_to_collapsed(snapshot),
          "frame;Render;audio 4.000\nframe;Update;gpu 3.750\n", "aggregation and sorting");

    profiling::Snapshot weird;
    profiling::SystemSample semi;
    semi.name = "a;b";
    semi.phase = SystemPhase::Update;
    semi.total_ms = 0.5;
    weird.systems = {semi};
    check(profiling::snapshot_to_collapsed(weird), "frame;Update;a_b 0.500\n", "semicolon sanitized");

    return failures ? 1 : 0;
}
