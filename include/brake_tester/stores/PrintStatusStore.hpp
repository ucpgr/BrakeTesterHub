#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class PrintStatusStore final : public IPrintStatusStore {
public:
  std::string getStatus() const override;
  void setStatus(std::string status) override;
  std::uint64_t getVersion() const override;
  bool waitForVersionAfter(std::uint64_t afterVersion,
                           std::chrono::milliseconds timeout,
                           std::string& status,
                           std::uint64_t& version) const override;

private:
  mutable std::mutex m_Mutex;
  mutable std::condition_variable m_Condition;
  std::string m_Status{"Idle"};
  std::uint64_t m_Version{0};
};

} // namespace brake_tester
