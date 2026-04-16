#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class SerialDeviceStore final : public ISerialDeviceStore {
public:
  std::vector<std::string> getDevices() const override;
  void setDevices(std::vector<std::string> devices) override;
  std::uint64_t getVersion() const override;
  bool waitForVersionAfter(std::uint64_t afterVersion,
                           std::chrono::milliseconds timeout,
                           std::vector<std::string>& devices,
                           std::uint64_t& version) const override;

private:
  mutable std::mutex m_Mutex;
  mutable std::condition_variable m_Condition;
  std::vector<std::string> m_Devices;
  std::uint64_t m_Version{0};
};

} // namespace brake_tester
