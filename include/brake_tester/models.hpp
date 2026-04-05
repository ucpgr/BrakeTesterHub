#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace brake_tester {

struct SerialSettings {
  std::string device_path{ "/dev/ttyS0" };
  std::uint32_t baud_rate{ 9600 };
  std::chrono::milliseconds silence_timeout{ 250 };
  std::size_t read_chunk_size{ 256 };
};

struct VehicleSelection {
  int id{ 0 };
  std::string display_name;
  std::string vin;
};

struct RenderedPage {
  std::size_t page_index{ 0 };
  std::vector<std::uint8_t> pixels;
  std::size_t width{ 0 };
  std::size_t height{ 0 };
};

} // namespace brake_tester
