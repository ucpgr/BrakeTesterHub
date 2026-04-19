#include "brake_tester/stores/CurrentTestAxleDataStore.hpp"

namespace brake_tester {

std::vector<HistoricalAxleResult> CurrentTestAxleDataStore::getAxleResults() const {
  std::scoped_lock lock(m_Mutex);
  return m_AxleResults;
}

void CurrentTestAxleDataStore::setAxleResults(const std::vector<HistoricalAxleResult>& axleResults) {
  std::scoped_lock lock(m_Mutex);
  m_AxleResults = axleResults;
}

bool CurrentTestAxleDataStore::isEmpty() const {
  std::scoped_lock lock(m_Mutex);
  return m_AxleResults.empty();
}

void CurrentTestAxleDataStore::clear() {
  std::scoped_lock lock(m_Mutex);
  m_AxleResults.clear();
}

} // namespace brake_tester
