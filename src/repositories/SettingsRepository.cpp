#include "brake_tester/repositories/SettingsRepository.hpp"

#include <cstdlib>
#include <stdexcept>

namespace brake_tester {

SettingsRepository::SettingsRepository(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("SettingsRepository requires a valid sqlite3 handle");
  }
  if (m_Log) {
    m_Log->information("[SettingsRepository Info]: Initializing.");
  }

  initializeSchema();
  loadCachedSerialSettings();
}

SerialSettings SettingsRepository::getSerialSettings() const {
  std::scoped_lock lock(m_Mutex);
  return m_CachedSerialSettings;
}

void SettingsRepository::setSerialSettings(const SerialSettings& serialSettings) {
  std::scoped_lock lock(m_Mutex);
  m_CachedSerialSettings = serialSettings;
  upsertCachedSerialSettings();
  if (m_Log) {
    m_Log->information("[SettingsRepository Info]: Updated and persisted serial settings.");
  }
}

void SettingsRepository::initializeSchema() {
  static constexpr const char* createTableSql =
      "CREATE TABLE IF NOT EXISTS LptSettings ("
      "name TEXT PRIMARY KEY,"
      "value TEXT NOT NULL"
      ");";
  executeSql(createTableSql);
  if (m_Log) {
    m_Log->information("[SettingsRepository Info]: Ensured LptSettings table exists.");
  }
}

void SettingsRepository::loadCachedSerialSettings() {
  m_CachedSerialSettings.lptDevicePath = getSettingValueOrDefault(
      "lptDevicePath",
      getSettingValueOrDefault("devicePath", m_CachedSerialSettings.lptDevicePath));
  m_CachedSerialSettings.brakeTesterDevicePath =
      getSettingValueOrDefault("brakeTesterDevicePath", m_CachedSerialSettings.brakeTesterDevicePath);

  const std::string baudRateText = getSettingValueOrDefault("baudRate", std::to_string(m_CachedSerialSettings.baudRate));
  m_CachedSerialSettings.baudRate = static_cast<std::uint32_t>(std::strtoul(baudRateText.c_str(), nullptr, 10));

  const std::string silenceTimeoutText =
      getSettingValueOrDefault("silenceTimeoutMs", std::to_string(m_CachedSerialSettings.silenceTimeout.count()));
  m_CachedSerialSettings.silenceTimeout = std::chrono::milliseconds(std::strtol(silenceTimeoutText.c_str(), nullptr, 10));

  const std::string readChunkSizeText =
      getSettingValueOrDefault("readChunkSize", std::to_string(m_CachedSerialSettings.readChunkSize));
  m_CachedSerialSettings.readChunkSize = static_cast<std::size_t>(std::strtoull(readChunkSizeText.c_str(), nullptr, 10));

  if (m_Log) {
    m_Log->information("[SettingsRepository Info]: Loaded cached serial settings.");
  }
}

std::string SettingsRepository::getSettingValueOrDefault(const std::string& settingName, const std::string& defaultValue) const {
  static constexpr const char* selectSql = "SELECT value FROM LptSettings WHERE name = ?;";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare LptSettings name/value select statement");
  }
  sqlite3_bind_text(statement, 1, settingName.c_str(), -1, SQLITE_TRANSIENT);

  const int stepResult = sqlite3_step(statement);
  std::string resolvedValue = defaultValue;
  if (stepResult == SQLITE_ROW) {
    resolvedValue = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  } else if (stepResult != SQLITE_DONE) {
    sqlite3_finalize(statement);
    throw std::runtime_error("Failed to read LptSettings name/value row");
  }

  sqlite3_finalize(statement);
  return resolvedValue;
}

void SettingsRepository::upsertCachedSerialSettings() const {
  static constexpr const char* upsertSql =
      "INSERT INTO LptSettings (name, value) VALUES (?, ?) "
      "ON CONFLICT(name) DO UPDATE SET value = excluded.value;";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, upsertSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare LptSettings upsert statement");
  }

  persistSetting(statement, "devicePath", m_CachedSerialSettings.lptDevicePath);
  persistSetting(statement, "lptDevicePath", m_CachedSerialSettings.lptDevicePath);
  persistSetting(statement, "brakeTesterDevicePath", m_CachedSerialSettings.brakeTesterDevicePath);
  persistSetting(statement, "baudRate", std::to_string(m_CachedSerialSettings.baudRate));
  persistSetting(statement, "silenceTimeoutMs", std::to_string(m_CachedSerialSettings.silenceTimeout.count()));
  persistSetting(statement, "readChunkSize", std::to_string(m_CachedSerialSettings.readChunkSize));
  finalizeStatement(statement);

  if (m_Log) {
    m_Log->information("[SettingsRepository Info]: Persisted serial settings to sqlite.");
  }
}

void SettingsRepository::resetStatement(sqlite3_stmt* statement) {
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

void SettingsRepository::finalizeStatement(sqlite3_stmt* statement) {
  sqlite3_finalize(statement);
}

void SettingsRepository::persistSetting(sqlite3_stmt* statement,
                                        const std::string& settingName,
                                        const std::string& settingValue) const {
  resetStatement(statement);
  sqlite3_bind_text(statement, 1, settingName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, settingValue.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    finalizeStatement(statement);
    throw std::runtime_error("Failed to write LptSettings name/value row");
  }
}

void SettingsRepository::executeSql(const char* sqlText) const {
  char* errorMessage = nullptr;
  if (sqlite3_exec(m_DatabaseHandle, sqlText, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
    const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
    sqlite3_free(errorMessage);
    if (m_Log) {
      m_Log->Error("[SettingsRepository Error]: SQL execution failed: " + sqliteError);
    }
    throw std::runtime_error(sqliteError);
  }
}

} // namespace brake_tester
