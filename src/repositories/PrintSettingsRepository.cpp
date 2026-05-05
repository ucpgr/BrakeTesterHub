#include "brake_tester/repositories/PrintSettingsRepository.hpp"

#include <stdexcept>

namespace brake_tester {

PrintSettingsRepository::PrintSettingsRepository(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("PrintSettingsRepository requires a valid sqlite3 handle");
  }

  if (m_Log) {
    m_Log->information("[PrintSettingsRepository Info]: Initializing print settings repository.");
  }

  initializeSchema();
  loadCachedPrintSettings();
}

PrintSettings PrintSettingsRepository::getPrintSettings() const {
  std::scoped_lock lock(m_Mutex);
  return m_CachedPrintSettings;
}

void PrintSettingsRepository::setPrintSettings(const PrintSettings& printSettings) {
  std::scoped_lock lock(m_Mutex);
  m_CachedPrintSettings = printSettings;
  upsertCachedPrintSettings();

  if (m_Log) {
    m_Log->information("[PrintSettingsRepository Info]: Print settings persisted successfully.");
  }
}

void PrintSettingsRepository::initializeSchema() {
  static constexpr const char* createTableSql =
      "CREATE TABLE IF NOT EXISTS PrintSettings ("
      "name TEXT PRIMARY KEY,"
      "value TEXT NOT NULL"
      ");";

  executeSql(createTableSql);

  if (m_Log) {
    m_Log->information("[PrintSettingsRepository Info]: Ensured PrintSettings table exists.");
  }
}

void PrintSettingsRepository::loadCachedPrintSettings() {
  m_CachedPrintSettings.selectedPrinter = getSettingValueOrDefault("selectedPrinter", "");
  const std::string autoPrintText = getSettingValueOrDefault("autoPrint", "0");
  m_CachedPrintSettings.autoPrint = (autoPrintText == "1" || autoPrintText == "true" || autoPrintText == "TRUE");

  if (m_Log) {
    m_Log->information("[PrintSettingsRepository Info]: Loaded cached print settings.");
  }
}

std::string PrintSettingsRepository::getSettingValueOrDefault(const std::string& settingName,
                                                              const std::string& defaultValue) const {
  static constexpr const char* selectSql = "SELECT value FROM PrintSettings WHERE name = ?;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare PrintSettings select statement");
  }

  sqlite3_bind_text(statement, 1, settingName.c_str(), -1, SQLITE_TRANSIENT);

  const int stepResult = sqlite3_step(statement);
  std::string resolvedValue = defaultValue;
  if (stepResult == SQLITE_ROW) {
    resolvedValue = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  } else if (stepResult != SQLITE_DONE) {
    sqlite3_finalize(statement);
    throw std::runtime_error("Failed to read PrintSettings row");
  }

  sqlite3_finalize(statement);
  return resolvedValue;
}

void PrintSettingsRepository::upsertCachedPrintSettings() const {
  static constexpr const char* upsertSql =
      "INSERT INTO PrintSettings (name, value) VALUES (?, ?) "
      "ON CONFLICT(name) DO UPDATE SET value = excluded.value;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, upsertSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare PrintSettings upsert statement");
  }

  persistSetting(statement, "selectedPrinter", m_CachedPrintSettings.selectedPrinter);
  persistSetting(statement, "autoPrint", m_CachedPrintSettings.autoPrint ? "1" : "0");

  finalizeStatement(statement);

  if (m_Log) {
    m_Log->information("[PrintSettingsRepository Info]: Upserted print settings rows.");
  }
}

void PrintSettingsRepository::resetStatement(sqlite3_stmt* statement) {
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

void PrintSettingsRepository::finalizeStatement(sqlite3_stmt* statement) {
  sqlite3_finalize(statement);
}

void PrintSettingsRepository::persistSetting(sqlite3_stmt* statement,
                                             const std::string& settingName,
                                             const std::string& settingValue) const {
  resetStatement(statement);
  sqlite3_bind_text(statement, 1, settingName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, settingValue.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(statement) != SQLITE_DONE) {
    finalizeStatement(statement);
    throw std::runtime_error("Failed to write PrintSettings row");
  }
}

void PrintSettingsRepository::executeSql(const char* sqlText) const {
  char* errorMessage = nullptr;
  if (sqlite3_exec(m_DatabaseHandle, sqlText, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);

    if (m_Log) {
      m_Log->error("[PrintSettingsRepository Error]: SQL execution failed. " + sqliteError);
    }

    throw std::runtime_error(sqliteError);
  }
}

} // namespace brake_tester
