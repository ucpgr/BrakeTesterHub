#include "brake_tester/components/DatabaseSchema.hpp"

#include <array>
#include <stdexcept>

namespace brake_tester {

DatabaseSchema::DatabaseSchema(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("DatabaseSchema requires a valid sqlite3 handle");
  }
}

void DatabaseSchema::ensureCreated() const {
  static constexpr const char* createHistoricalVehiclesSql =
      "CREATE TABLE IF NOT EXISTS historical_vehicles ("
      "id INTEGER PRIMARY KEY,"
      "reg TEXT NOT NULL UNIQUE,"
      "make TEXT,"
      "model TEXT,"
      "mileage TEXT"
      ");";

  static constexpr const char* createTestsSql =
      "CREATE TABLE IF NOT EXISTS tests ("
      "id INTEGER PRIMARY KEY,"
      "created_at_utc TEXT NOT NULL,"
      "prnFile TEXT,"
      "pdfFile TEXT NOT NULL,"
      "thumbnailFile TEXT,"
      "outcome TEXT NOT NULL DEFAULT 'unknown',"
      "historical_vehicle_id INTEGER,"
      "FOREIGN KEY (historical_vehicle_id) REFERENCES historical_vehicles(id) ON DELETE SET NULL"
      ");";

  static constexpr const char* createAxleResultsSql =
      "CREATE TABLE IF NOT EXISTS axle_results ("
      "id INTEGER PRIMARY KEY,"
      "test_id INTEGER NOT NULL,"
      "axle_index INTEGER NOT NULL,"
      "test_type TEXT NOT NULL,"
      "left_brake_force INTEGER,"
      "right_brake_force INTEGER,"
      "efficiency REAL,"
      "imbalance REAL,"
      "weight INTEGER,"
      "FOREIGN KEY (test_id) REFERENCES tests(id) ON DELETE CASCADE"
      ");";

  static constexpr const char* createPreferenceSql =
      "CREATE TABLE IF NOT EXISTS history_preferences ("
      "id INTEGER PRIMARY KEY CHECK (id = 1),"
      "results_per_page INTEGER NOT NULL"
      ");";

  const std::array<const char*, 4> statements = {
      createHistoricalVehiclesSql,
      createTestsSql,
      createAxleResultsSql,
      createPreferenceSql,
  };

  for (const char* statementText : statements) {
    char* errorMessage = nullptr;
    if (sqlite3_exec(m_DatabaseHandle, statementText, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
      const std::string message = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
      sqlite3_free(errorMessage);
      throw std::runtime_error(message);
    }
  }

  sqlite3_exec(m_DatabaseHandle,
               "INSERT INTO history_preferences (id, results_per_page) VALUES (1, 20) "
               "ON CONFLICT(id) DO NOTHING",
               nullptr,
               nullptr,
               nullptr);

  if (m_Log) {
    m_Log->information("Database schema ensured for tables: tests, axle_results, historical_vehicles, history_preferences.");
  }
}

} // namespace brake_tester
