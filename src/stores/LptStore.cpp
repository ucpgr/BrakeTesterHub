#include "brake_tester/stores/LptStore.hpp"

namespace brake_tester {

LptListenerStatus LptStore::getListenerStatus() const {
  std::scoped_lock lock(m_Mutex);
  return m_ListenerStatus;
}

void LptStore::setListenerStatus(LptListenerStatus status) {
  std::scoped_lock lock(m_Mutex);
  m_ListenerStatus = status;
}

std::string LptStore::getCurrentCaptureFilename() const {
  std::scoped_lock lock(m_Mutex);
  return m_CurrentCaptureFilename;
}

void LptStore::setCurrentCaptureFilename(std::string filename) {
  std::scoped_lock lock(m_Mutex);
  m_CurrentCaptureFilename = std::move(filename);
}

LptProcessStatus LptStore::getProcessStatus() const {
  std::scoped_lock lock(m_Mutex);
  return m_ProcessStatus;
}

void LptStore::setProcessStatus(LptProcessStatus status) {
  {
    std::scoped_lock lock(m_Mutex);
    m_ProcessStatus = status;
    ++m_ProcessStatusVersion;
  }
  m_ProcessStatusCondition.notify_all();
}

std::uint64_t LptStore::getProcessStatusVersion() const {
  std::scoped_lock lock(m_Mutex);
  return m_ProcessStatusVersion;
}

bool LptStore::waitForProcessStatusAfter(std::uint64_t afterVersion,
                                         std::chrono::milliseconds timeout,
                                         LptProcessStatus& status,
                                         std::uint64_t& version) const {
  std::unique_lock lock(m_Mutex);
  const bool updated = m_ProcessStatusCondition.wait_for(
      lock,
      timeout,
      [this, afterVersion] { return m_ProcessStatusVersion > afterVersion; });

  if (!updated) {
    return false;
  }

  status = m_ProcessStatus;
  version = m_ProcessStatusVersion;
  return true;
}

} // namespace brake_tester
