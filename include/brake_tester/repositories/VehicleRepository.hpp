#pragma once

#include <mutex>

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class VehicleRepository final : public IVehicleRepository {
public:
  VehicleRepository(sqlite3* databaseHandle, SharedLogger log);

  std::vector<VehicleSelection> getVehicles() const override;
  VehicleSelection addVehicle(const VehicleSelection& vehicle) override;
  bool deleteVehicle(int id) override;
  bool tryGetVehicle(int id, VehicleSelection& vehicle) const override;

private:
  void initializeSchema() const;

  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
  mutable std::mutex m_Mutex;
};

} // namespace brake_tester
