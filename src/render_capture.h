#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace afterhours {

struct RgbaCapture {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct PngCapture {
    std::vector<std::uint8_t> bytes;
};

}
