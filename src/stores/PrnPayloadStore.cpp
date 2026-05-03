#include "brake_tester/stores/PrnPayloadStore.hpp"

namespace brake_tester {

void PrnPayloadStore::enqueue(PrnPayload payload) {
  std::scoped_lock lock(m_Mutex);
  m_PayloadQueue.push(std::move(payload));
}

bool PrnPayloadStore::tryDequeue(PrnPayload& payload) {
  std::scoped_lock lock(m_Mutex);
  if (m_PayloadQueue.empty()) {
    return false;
  }

  payload = std::move(m_PayloadQueue.front());
  m_PayloadQueue.pop();
  return true;
}

} // namespace brake_tester
