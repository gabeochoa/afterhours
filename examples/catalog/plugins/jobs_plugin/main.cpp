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

struct LevelPreview : BaseComponent {
    std::string level_name;
    int thumbnail_count = 0;
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

    auto &preview = EntityHelper::createPermanentEntity();
    preview.addComponent<LevelPreview>();
    const EntityID preview_id = preview.id;

    pool.submit(
        [](std::stop_token) {
            std::this_thread::sleep_for(20ms);
            return std::string("harbor_night");
        },
        [preview_id](std::string name) {
            EntityHelper::getEntityForIDEnforce(preview_id)
                .get<LevelPreview>()
                .level_name = name;
            std::cout << "loaded level " << name << std::endl;
        });

    pool.submit(
        [](std::stop_token) {
            std::this_thread::sleep_for(40ms);
            return 12;
        },
        [preview_id](int count) {
            EntityHelper::getEntityForIDEnforce(preview_id)
                .get<LevelPreview>()
                .thumbnail_count = count;
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

    const auto &preview_data = preview.get<LevelPreview>();
    int frame = 0;
    for (; frame < 600 &&
           (preview_data.level_name.empty() || preview_data.thumbnail_count == 0);
         frame++) {
        systems.run(1.f / 60.f);
        std::this_thread::sleep_for(1ms);
    }

    pool.shutdown();
    std::cout << "level " << preview_data.level_name << " has "
              << preview_data.thumbnail_count << " thumbnails after " << frame
              << " frames" << std::endl;
    return preview_data.level_name.empty() || preview_data.thumbnail_count == 0
               ? 1
               : 0;
}
