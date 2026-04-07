#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace brake_tester {

struct SerialSettings {
  std::string devicePath{ "/dev/ttyS0" };
  std::uint32_t baudRate{ 9600 };
  std::chrono::milliseconds silenceTimeout{ 250 };
  std::size_t readChunkSize{ 256 };
};

struct VehicleSelection {
  int id{ 0 };
  std::string displayName;
  std::string vin;
};

struct RenderedPage {
  std::size_t pageIndex{ 0 };
  std::vector<std::uint8_t> pixels;
  std::size_t width{ 0 };
  std::size_t height{ 0 };
};

} // namespace brake_tester
