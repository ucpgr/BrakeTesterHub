#pragma once

#include <mutex>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class CurrentTestAxleDataStore final : public ICurrentTestAxleDataStore {
public:
  std::vector<HistoricalAxleResult> getAxleResults() const override;
  void setAxleResults(const std::vector<HistoricalAxleResult>& axleResults) override;
  bool isEmpty() const override;
  void clear() override;

private:
  mutable std::mutex m_Mutex;
  std::vector<HistoricalAxleResult> m_AxleResults;
};

} // namespace brake_tester
