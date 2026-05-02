#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class PrnPayloadStore final : public IPrnPayloadStore {
public:
  void enqueue(std::vector<std::uint8_t> payload) override;
  bool tryDequeue(std::vector<std::uint8_t>& payload) override;

private:
  std::mutex m_Mutex;
  std::queue<std::vector<std::uint8_t>> m_PayloadQueue;
};

} // namespace brake_tester
