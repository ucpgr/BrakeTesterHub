#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace brake_tester {

struct SerialSettings {
  std::string lptDevicePath{"/dev/ttyS0"};
  std::string brakeTesterDevicePath{"/dev/ttyS1"};
  std::uint32_t baudRate{9600};
  std::chrono::milliseconds silenceTimeout{250};
  std::size_t readChunkSize{256};
};

struct VehicleSelection {
  int id{0};
  std::string reg;
  std::string make;
  std::string model;
  std::optional<std::string> mileage;
};

struct RenderedPage {
  std::size_t pageIndex{0};
  std::vector<std::uint8_t> pixels;
  std::size_t width{0};
  std::size_t height{0};
};

enum class LptListenerStatus {
  Idle,
  CaptureStarted
};

enum class LptProcessStatus {
  Idle,
  TransferStarted,
  DataPatched,
  ConversionStarted,
  ConversionFinished
};

} // namespace brake_tester
