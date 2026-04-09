#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class LptStore final : public ILptStore {
public:
  LptListenerStatus getListenerStatus() const override;
  void setListenerStatus(LptListenerStatus status) override;
  std::string getCurrentCaptureFilename() const override;
  void setCurrentCaptureFilename(std::string filename) override;

  LptProcessStatus getProcessStatus() const override;
  void setProcessStatus(LptProcessStatus status) override;
  std::uint64_t getProcessStatusVersion() const override;
  bool waitForProcessStatusAfter(std::uint64_t afterVersion,
                                 std::chrono::milliseconds timeout,
                                 LptProcessStatus& status,
                                 std::uint64_t& version) const override;

private:
  mutable std::mutex m_Mutex;
  mutable std::condition_variable m_ProcessStatusCondition;

  LptListenerStatus m_ListenerStatus{LptListenerStatus::Idle};
  std::string m_CurrentCaptureFilename;

  LptProcessStatus m_ProcessStatus{LptProcessStatus::Idle};
  std::uint64_t m_ProcessStatusVersion{0};
};

} // namespace brake_tester
