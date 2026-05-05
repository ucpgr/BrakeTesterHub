#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace brake_tester {

class ILptListener {
public:
  virtual ~ILptListener() = default;
  virtual std::vector<std::uint8_t> captureTransmission(const std::atomic_bool& shouldKeepRunning) = 0;
  virtual void test() = 0;
};

} // namespace brake_tester
