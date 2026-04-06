#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <stdexcept>

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class SettingsRepository final : public ISettingsRepository {
public:
  explicit SettingsRepository(sqlite3* databaseHandle) : m_DatabaseHandle(databaseHandle) {
    if (m_DatabaseHandle == nullptr) {
      throw std::invalid_argument("SettingsRepository requires a valid sqlite3 handle");
    }

    initializeSchema();
    loadCachedSerialSettings();
  }

  SerialSettings getSerialSettings() const override {
    std::scoped_lock lock(m_Mutex);
    return m_CachedSerialSettings;
  }

  void setSerialSettings(const SerialSettings& serialSettings) override {
    std::scoped_lock lock(m_Mutex);
    m_CachedSerialSettings = serialSettings;
    upsertCachedSerialSettings();
  }

private:
  void initializeSchema() {
    static constexpr const char* createTableSql =
        "CREATE TABLE IF NOT EXISTS LptSettings ("
        "Id INTEGER PRIMARY KEY CHECK (Id = 1),"
        "DevicePath TEXT NOT NULL,"
        "BaudRate INTEGER NOT NULL,"
        "SilenceTimeoutMs INTEGER NOT NULL,"
        "ReadChunkSize INTEGER NOT NULL"
        ");";
    executeSql(createTableSql);
  }

  void loadCachedSerialSettings() {
    static constexpr const char* selectSql =
        "SELECT DevicePath, BaudRate, SilenceTimeoutMs, ReadChunkSize "
        "FROM LptSettings WHERE Id = 1;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare LptSettings select statement");
    }

    const int stepResult = sqlite3_step(statement);
    if (stepResult == SQLITE_ROW) {
      m_CachedSerialSettings.devicePath =
          reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
      m_CachedSerialSettings.baudRate = static_cast<std::uint32_t>(sqlite3_column_int64(statement, 1));
      m_CachedSerialSettings.silenceTimeout =
          std::chrono::milliseconds(static_cast<std::int64_t>(sqlite3_column_int64(statement, 2)));
      m_CachedSerialSettings.readChunkSize = static_cast<std::size_t>(sqlite3_column_int64(statement, 3));
    } else if (stepResult != SQLITE_DONE) {
      sqlite3_finalize(statement);
      throw std::runtime_error("Failed to read LptSettings row");
    }

    sqlite3_finalize(statement);
  }

  void upsertCachedSerialSettings() const {
    static constexpr const char* upsertSql =
        "INSERT INTO LptSettings (Id, DevicePath, BaudRate, SilenceTimeoutMs, ReadChunkSize) "
        "VALUES (1, ?, ?, ?, ?) "
        "ON CONFLICT(Id) DO UPDATE SET "
        "DevicePath = excluded.DevicePath, "
        "BaudRate = excluded.BaudRate, "
        "SilenceTimeoutMs = excluded.SilenceTimeoutMs, "
        "ReadChunkSize = excluded.ReadChunkSize;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_DatabaseHandle, upsertSql, -1, &statement, nullptr) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare LptSettings upsert statement");
    }

    sqlite3_bind_text(statement, 1, m_CachedSerialSettings.devicePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(m_CachedSerialSettings.baudRate));
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(m_CachedSerialSettings.silenceTimeout.count()));
    sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(m_CachedSerialSettings.readChunkSize));

    if (sqlite3_step(statement) != SQLITE_DONE) {
      sqlite3_finalize(statement);
      throw std::runtime_error("Failed to write LptSettings row");
    }

    sqlite3_finalize(statement);
  }

  void executeSql(const char* sqlText) const {
    char* errorMessage = nullptr;
    if (sqlite3_exec(m_DatabaseHandle, sqlText, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
      const std::string sqliteError = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
      sqlite3_free(errorMessage);
      throw std::runtime_error(sqliteError);
    }
  }

  sqlite3* m_DatabaseHandle;
  mutable std::mutex m_Mutex;
  SerialSettings m_CachedSerialSettings{};
};

class SelectedVehicleStore final : public ISelectedVehicleStore {
public:
  SelectedVehicleStore() = default;

  VehicleSelection getSelectedVehicle() const override {
    std::scoped_lock lock(m_Mutex);
    return m_SelectedVehicle;
  }

  void setSelectedVehicle(const VehicleSelection& selectedVehicle) override {
    std::scoped_lock lock(m_Mutex);
    m_SelectedVehicle = selectedVehicle;
  }

private:
  mutable std::mutex m_Mutex;
  VehicleSelection m_SelectedVehicle{};
};

} // namespace brake_tester
