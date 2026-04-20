#pragma once

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptRepository final : public ILptRepository {
public:
  LptRepository(sqlite3* databaseHandle, SharedLogger log);
  int createTest(const HistoricalTest& test, const std::vector<HistoricalAxleResult>& axleResults) override;
  HistoricalPage getTests(const HistoricalTestQuery& query) const override;
  bool tryGetTestDetails(int testId, HistoricalTestDetails& details) const override;
  bool deleteTest(int testId) override;
  int getResultsPerPagePreference() const override;
  void setResultsPerPagePreference(int value) override;

private:
  void initializeSchema() const;
  int ensureHistoricalVehicle(const HistoricalVehicle& vehicle) const;
  void cleanupUnreferencedHistoricalVehicles() const;
  HistoricalFilterOptions buildFilterOptions(const HistoricalTestQuery& query) const;
  static std::string monthLabel(int month);

  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
};

} // namespace brake_tester
