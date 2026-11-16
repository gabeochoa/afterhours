#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/plugins/jobs.h"

using namespace afterhours;
using namespace std::chrono_literals;

struct LevelInfo : BaseComponent {
    std::string name;
    int thumbnails = 0;
};

struct DrainJobs : System<> {
    jobs::Pool &pool;
    explicit DrainJobs(jobs::Pool &p) : pool(p) {}
    void once(float) override { pool.drain(); }
};

int main(int, char **) {
    jobs::Pool pool(2, 16);
    SystemManager systems;
    systems.register_update_system(std::make_unique<DrainJobs>(pool));

    auto &level = EntityHelper::createPermanentEntity();
    level.addComponent<LevelInfo>();
    const EntityID level_id = level.id;

    pool.submit(
        [](std::stop_token) {
            std::this_thread::sleep_for(20ms);
            return std::string("harbor_night");
        },
        [level_id](std::string name) {
            EntityHelper::getEntityForIDEnforce(level_id)
                .get<LevelInfo>()
                .name = name;
            std::cout << "loaded level " << name << std::endl;
        });

    pool.submit(
        [](std::stop_token) {
            std::this_thread::sleep_for(40ms);
            return 12;
        },
        [level_id](int count) {
            EntityHelper::getEntityForIDEnforce(level_id)
                .get<LevelInfo>()
                .thumbnails = count;
            std::cout << "built " << count << " thumbnails" << std::endl;
        },
        jobs::Priority::Background);

    auto search = pool.submit(
        [](std::stop_token stop) {
            while (!stop.stop_requested()) std::this_thread::sleep_for(1ms);
            return std::string("never delivered");
        },
        [](std::string result) {
            std::cout << "search finished: " << result << std::endl;
        });
    pool.cancel(*search);
    std::cout << "cancelled search job " << *search << std::endl;

    const auto &info = level.get<LevelInfo>();
    int frame = 0;
    for (; frame < 600 && (info.name.empty() || info.thumbnails == 0);
         frame++) {
        systems.run(1.f / 60.f);
        std::this_thread::sleep_for(1ms);
    }

    pool.shutdown();
    std::cout << "level " << info.name << " has " << info.thumbnails
              << " thumbnails after " << frame << " frames" << std::endl;
    return info.name.empty() || info.thumbnails == 0 ? 1 : 0;
}
