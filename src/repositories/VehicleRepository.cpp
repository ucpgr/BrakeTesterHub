#include "brake_tester/repositories/VehicleRepository.hpp"

#include <stdexcept>
#include <unordered_set>

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
      "mileage TEXT"
      ");";

  char* errorMessage = nullptr;
  if (sqlite3_exec(m_DatabaseHandle, createTableSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);
    throw std::runtime_error(sqliteError);
  }

  static constexpr const char* tableInfoSql = "PRAGMA table_info(Vehicles);";
  sqlite3_stmt* tableInfoStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, tableInfoSql, -1, &tableInfoStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to inspect Vehicles table schema");
  }

  std::unordered_set<std::string> columnNames;
  while (sqlite3_step(tableInfoStatement) == SQLITE_ROW) {
    const unsigned char* nameText = sqlite3_column_text(tableInfoStatement, 1);
    if (nameText != nullptr) {
      columnNames.emplace(reinterpret_cast<const char*>(nameText));
    }
  }
  sqlite3_finalize(tableInfoStatement);

  if (columnNames.contains("mileageUnit")) {
    static constexpr const char* migrateSql =
        "BEGIN TRANSACTION;"
        "CREATE TABLE Vehicles_new ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "reg TEXT NOT NULL,"
        "make TEXT NOT NULL,"
        "model TEXT NOT NULL,"
        "mileage TEXT"
        ");"
        "INSERT INTO Vehicles_new (id, reg, make, model, mileage) "
        "SELECT id, reg, make, model, "
        "CASE "
        "WHEN mileageUnit IS NULL OR TRIM(mileageUnit) = '' THEN CAST(mileage AS TEXT) "
        "WHEN mileage IS NULL OR TRIM(CAST(mileage AS TEXT)) = '' THEN NULL "
        "ELSE TRIM(CAST(mileage AS TEXT)) || TRIM(mileageUnit) "
        "END "
        "FROM Vehicles;"
        "DROP TABLE Vehicles;"
        "ALTER TABLE Vehicles_new RENAME TO Vehicles;"
        "COMMIT;";

    if (sqlite3_exec(m_DatabaseHandle, migrateSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
      const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
      sqlite3_free(errorMessage);
      throw std::runtime_error(sqliteError);
    }
  }
}

std::vector<VehicleSelection> VehicleRepository::getVehicles() const {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* selectSql =
      "SELECT id, reg, make, model, mileage FROM Vehicles ORDER BY reg ASC, id ASC;";

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

    if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
      vehicle.mileage = std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 4)));
    }

    vehicles.push_back(std::move(vehicle));
  }

  sqlite3_finalize(statement);
  return vehicles;
}

VehicleSelection VehicleRepository::addVehicle(const VehicleSelection& vehicle) {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* insertSql =
      "INSERT INTO Vehicles (reg, make, model, mileage) VALUES (?, ?, ?, ?);";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, insertSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle insert statement");
  }

  sqlite3_bind_text(statement, 1, vehicle.reg.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, vehicle.make.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 3, vehicle.model.c_str(), -1, SQLITE_TRANSIENT);

  if (vehicle.mileage.has_value() && !vehicle.mileage->empty()) {
    sqlite3_bind_text(statement, 4, vehicle.mileage->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, 4);
  }

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
      "SELECT id, reg, make, model, mileage FROM Vehicles WHERE id = ? LIMIT 1;";

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

    if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
      vehicle.mileage = std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 4)));
    } else {
      vehicle.mileage.reset();
    }

    sqlite3_finalize(statement);
    return true;
  }

  sqlite3_finalize(statement);
  return false;
}


bool VehicleRepository::updateVehicleMileage(int id, const std::optional<std::string>& mileage) {
  std::scoped_lock lock(m_Mutex);

  static constexpr const char* updateSql = "UPDATE Vehicles SET mileage = ? WHERE id = ?;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, updateSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare vehicle mileage update statement");
  }

  if (mileage.has_value() && !mileage->empty()) {
    sqlite3_bind_text(statement, 1, mileage->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, 1);
  }

  sqlite3_bind_int(statement, 2, id);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    throw std::runtime_error("Failed to update vehicle mileage");
  }

  const int updatedRows = sqlite3_changes(m_DatabaseHandle);
  sqlite3_finalize(statement);
  return updatedRows > 0;
}

} // namespace brake_tester
