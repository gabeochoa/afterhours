#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace afterhours {

struct RGBACapture {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct PNGCapture {
    std::vector<std::uint8_t> bytes;
};

}
