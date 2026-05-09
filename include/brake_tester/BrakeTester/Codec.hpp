#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Frame.h"

namespace BrakeTester {

uint8_t calculateChecksum(std::span<const uint8_t> bytes);
std::vector<uint8_t> serializeFrame(const Frame& frame);
Frame decodeFrame(std::span<const uint8_t> frameBytes);

} // namespace BrakeTester
