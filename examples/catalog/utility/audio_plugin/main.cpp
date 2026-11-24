#include <cstdio>
#include <vector>

#include "../../../../src/graphics.h"
#include "../../../../src/plugins/sound_system.h"

namespace graphics = afterhours::graphics;
using Audio = afterhours::sound_system;

static int played_count = 0;
static float bus_volume = 0.f;

int main() {
  graphics::RunConfig config;
  config.display = graphics::DisplayMode::Headless;
  config.audio = true;
  config.frame = [&] {
    auto &sounds = Audio::SoundLibrary::get();
    Audio::set_recording(true);
    Audio::set_bus_volume("ambience", 0.6f);

    std::vector<float> samples(2205, 0.2f);
    sounds.add_samples("generated_blip", samples, 44100, 1, "ambience");

    Audio::set_listener_position({0.f, 0.f, 0.f}, {0.f, 0.f, -1.f});
    sounds.set_range("generated_blip", 20.f);
    Audio::play_at("generated_blip", {2.f, 0.f, -4.f});
    Audio::play_at("generated_blip", {0.f, 0.f, -100.f});

    bus_volume = Audio::get_bus_volume("ambience");
    played_count = static_cast<int>(Audio::play_log().size());
    std::printf("bird bus volume=%.3f\n", bus_volume);
    for (const auto &rec : Audio::play_log()) {
      std::printf("played %s bus=%s volume=%.3f pan=%.2f\n", rec.name.c_str(),
                  rec.bus.c_str(), rec.volume, rec.pan);
    }
    sounds.unload_all();
    graphics::request_quit();
  };
  graphics::run(config);
  // Only the in-range play is recorded; the one 100 units away is culled
  // by the 20-unit range set above.
  return played_count == 1 && bus_volume > 0.59f && bus_volume < 0.61f ? 0 : 1;
}
