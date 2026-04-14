#include "brake_tester/repositories/VehicleRepository.hpp"

#include <stdexcept>

namespace brake_tester {

VehicleRepository::VehicleRepository(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("VehicleRepository requires a valid sqlite3 handle");
  }
  initializeSchema();
}

void VehicleRepository::initializeSchema() const {
  static constexpr const char* createTableSql =
      "CREATE TABLE IF NOT EXISTS Vehicles ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "reg TEXT NOT NULL,"
      "make TEXT NOT NULL,"
      "model TEXT NOT NULL,"
      "mileage INTEGER NOT NULL DEFAULT 0,"
      "mileageUnit TEXT NOT NULL DEFAULT 'km'"
      ");";

  char* errorMessage = nullptr;
  if (sqlite3_exec(m_DatabaseHandle, createTableSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);
    throw std::runtime_error(sqliteError);
  }
}

std::vector<VehicleSelection> VehicleRepository::getVehicles() const {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* selectSql =
      "SELECT id, reg, make, model, mileage, mileageUnit FROM Vehicles ORDER BY reg ASC, id ASC;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle select statement");
  }

  std::vector<VehicleSelection> vehicles;
  while (sqlite3_step(statement) == SQLITE_ROW) {
    VehicleSelection vehicle;
    vehicle.id = sqlite3_column_int(statement, 0);
    vehicle.reg = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    vehicle.make = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    vehicle.model = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    vehicle.mileage = sqlite3_column_int(statement, 4);
    vehicle.mileageUnit = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    vehicles.push_back(std::move(vehicle));
  }

  sqlite3_finalize(statement);
  return vehicles;
}

VehicleSelection VehicleRepository::addVehicle(const VehicleSelection& vehicle) {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* insertSql =
      "INSERT INTO Vehicles (reg, make, model, mileage, mileageUnit) VALUES (?, ?, ?, ?, ?);";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, insertSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle insert statement");
  }

  sqlite3_bind_text(statement, 1, vehicle.reg.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, vehicle.make.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 3, vehicle.model.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement, 4, vehicle.mileage);
  sqlite3_bind_text(statement, 5, vehicle.mileageUnit.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    throw std::runtime_error("Failed to insert vehicle");
  }

  sqlite3_finalize(statement);

  VehicleSelection created = vehicle;
  created.id = static_cast<int>(sqlite3_last_insert_rowid(m_DatabaseHandle));
  return created;
}

bool VehicleRepository::deleteVehicle(int id) {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* deleteSql = "DELETE FROM Vehicles WHERE id = ?;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, deleteSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle delete statement");
  }

  sqlite3_bind_int(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    throw std::runtime_error("Failed to delete vehicle");
  }

  const int deletedRows = sqlite3_changes(m_DatabaseHandle);
  sqlite3_finalize(statement);
  return deletedRows > 0;
}

bool VehicleRepository::tryGetVehicle(int id, VehicleSelection& vehicle) const {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* selectSql =
      "SELECT id, reg, make, model, mileage, mileageUnit FROM Vehicles WHERE id = ? LIMIT 1;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle by-id select statement");
  }

  sqlite3_bind_int(statement, 1, id);
  const int stepResult = sqlite3_step(statement);
  if (stepResult == SQLITE_ROW) {
    vehicle.id = sqlite3_column_int(statement, 0);
    vehicle.reg = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    vehicle.make = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    vehicle.model = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    vehicle.mileage = sqlite3_column_int(statement, 4);
    vehicle.mileageUnit = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    sqlite3_finalize(statement);
    return true;
  }

  sqlite3_finalize(statement);
  return false;
}

} // namespace brake_tester
