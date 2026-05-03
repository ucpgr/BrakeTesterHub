#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class PrnPayloadStore final : public IPrnPayloadStore {
public:
  void enqueue(PrnPayload payload) override;
  bool tryDequeue(PrnPayload& payload) override;

private:
  std::mutex m_Mutex;
  std::queue<PrnPayload> m_PayloadQueue;
};

} // namespace brake_tester
