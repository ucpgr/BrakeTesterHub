#include "brake_tester/stores/PrintStatusStore.hpp"

namespace brake_tester {

std::string PrintStatusStore::getStatus() const {
  std::scoped_lock lock(m_Mutex);
  return m_Status;
}

void PrintStatusStore::setStatus(std::string status) {
  bool changed = false;
  {
    std::scoped_lock lock(m_Mutex);
    if (m_Status != status) {
      m_Status = std::move(status);
      ++m_Version;
      changed = true;
    }
  }

  if (changed) {
    m_Condition.notify_all();
  }
}

std::uint64_t PrintStatusStore::getVersion() const {
  std::scoped_lock lock(m_Mutex);
  return m_Version;
}

bool PrintStatusStore::waitForVersionAfter(std::uint64_t afterVersion,
                                           std::chrono::milliseconds timeout,
                                           std::string& status,
                                           std::uint64_t& version) const {
  std::unique_lock lock(m_Mutex);
  const bool updated = m_Condition.wait_for(lock, timeout, [this, afterVersion] { return m_Version > afterVersion; });

  if (!updated) {
    return false;
  }

  status = m_Status;
  version = m_Version;
  return true;
}

} // namespace brake_tester
