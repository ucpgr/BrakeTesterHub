#include "brake_tester/stores/LptRepository.hpp"

#include <stdexcept>

namespace brake_tester {

LptRepository::LptRepository(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("LptRepository requires a valid sqlite3 handle");
  }
  initializeSchema();
}

void LptRepository::initializeSchema() const {
  static constexpr const char* createTestsSql =
      "CREATE TABLE IF NOT EXISTS tests ("
      "id INTEGER PRIMARY KEY,"
      "prnFile TEXT NOT NULL,"
      "pdfFile TEXT NOT NULL"
      ");";

  static constexpr const char* createAxleResultsSql =
      "CREATE TABLE IF NOT EXISTS axle_results ("
      "id INTEGER PRIMARY KEY,"
      "test_id INTEGER NOT NULL,"
      "axle_index INTEGER NOT NULL,"
      "test_type INTEGER NOT NULL,"
      "left_brake_force INTEGER,"
      "right_brake_force INTEGER,"
      "left_wheel_stalled INTEGER,"
      "right_wheel_stalled INTEGER,"
      "weight INTEGER,"
      "raw_memory_blob BLOB NOT NULL,"
      "FOREIGN KEY (test_id) REFERENCES tests(id) ON DELETE CASCADE"
      ");";

  char* errorMessage = nullptr;
  if (sqlite3_exec(m_DatabaseHandle, createTestsSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string message = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);
    throw std::runtime_error(message);
  }

  if (sqlite3_exec(m_DatabaseHandle, createAxleResultsSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string message = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);
    throw std::runtime_error(message);
  }
}

} // namespace brake_tester
