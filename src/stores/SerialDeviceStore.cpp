#include "brake_tester/stores/SerialDeviceStore.hpp"

#include <algorithm>

namespace brake_tester {

std::vector<std::string> SerialDeviceStore::getDevices() const {
  std::scoped_lock lock(m_Mutex);
  return m_Devices;
}

void SerialDeviceStore::setDevices(std::vector<std::string> devices) {
  std::sort(devices.begin(), devices.end());
  devices.erase(std::unique(devices.begin(), devices.end()), devices.end());

  bool changed = false;
  {
    std::scoped_lock lock(m_Mutex);
    if (m_Devices != devices) {
      m_Devices = std::move(devices);
      ++m_Version;
      changed = true;
    }
  }

  if (changed) {
    m_Condition.notify_all();
  }
}

std::uint64_t SerialDeviceStore::getVersion() const {
  std::scoped_lock lock(m_Mutex);
  return m_Version;
}

bool SerialDeviceStore::waitForVersionAfter(std::uint64_t afterVersion,
                                            std::chrono::milliseconds timeout,
                                            std::vector<std::string>& devices,
                                            std::uint64_t& version) const {
  std::unique_lock lock(m_Mutex);
  const bool updated = m_Condition.wait_for(
      lock,
      timeout,
      [this, afterVersion] { return m_Version > afterVersion; });

  if (!updated) {
    return false;
  }

  devices = m_Devices;
  version = m_Version;
  return true;
}

} // namespace brake_tester
