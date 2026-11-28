#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

#include <chrono>
#include <future>
#include <optional>
#include <thread>

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::testing;

struct Worker {
    std::future<int> result;
    std::optional<int> value;
};

struct StartWorkCommand : System<PendingE2ECommand> {
    Worker &worker;
    explicit StartWorkCommand(Worker &w) : worker(w) {}

    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("start_work")) return;
        worker.value.reset();
        worker.result = std::async(std::launch::async, [] {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return 42;
        });
        cmd.consume();
    }
};

struct ExpectWorkDoneCommand : System<PendingE2ECommand> {
    Worker &worker;
    explicit ExpectWorkDoneCommand(Worker &w) : worker(w) {}

    void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
        if (cmd.is_consumed() || !cmd.is("expect_work_done")) return;
        if (!worker.value) {
            if (!worker.result.valid()) {
                cmd.fail("start_work never ran");
                return;
            }
            if (worker.result.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready) {
                cmd.retry_with("worker still busy");
                return;
            }
            worker.value = worker.result.get();
        }
        const int expected = cmd.arg_as<int>(0);
        if (*worker.value == expected)
            cmd.consume();
        else
            cmd.fail(std::format("worker returned {}, expected {}",
                                 *worker.value, expected));
    }
};

int main(int argc, char *argv[]) {
    E2EHarness harness(argc, argv);
    if (!harness.has_script()) {
        log_error("usage: --test-script worker.e2e [--time-scale N]");
        return 2;
    }

    Worker worker;
    harness.add_systems(std::make_unique<StartWorkCommand>(worker),
                        std::make_unique<ExpectWorkDoneCommand>(worker));

    const float dt = harness.args.time_scale / 60.f;
    const auto start = std::chrono::steady_clock::now();
    int frames = 0;
    const int result = harness.run([&] { ++frames; }, dt);
    const std::chrono::duration<float> real =
        std::chrono::steady_clock::now() - start;
    log_info("{} frames at dt {:.3f}, {:.2f}s real", frames, dt, real.count());
    return result;
}
