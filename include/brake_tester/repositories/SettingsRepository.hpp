#pragma once

#include <mutex>
#include <string>

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class SettingsRepository final : public ISettingsRepository {
public:
  SettingsRepository(sqlite3* databaseHandle, SharedLogger log);

  SerialSettings getSerialSettings() const override;
  void setSerialSettings(const SerialSettings& serialSettings) override;
  int getVehicleUnassignMinutes() const override;
  void setVehicleUnassignMinutes(int minutes) override;

private:
  void initializeSchema();
  void loadCachedSerialSettings();
  static int clampVehicleUnassignMinutes(int minutes);
  std::string getSettingValueOrDefault(const std::string& settingName, const std::string& defaultValue) const;
  void upsertCachedSerialSettings() const;
  void persistSetting(sqlite3_stmt* statement, const std::string& settingName, const std::string& settingValue) const;
  void executeSql(const char* sqlText) const;

  static void resetStatement(sqlite3_stmt* statement);
  static void finalizeStatement(sqlite3_stmt* statement);

  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
  mutable std::mutex m_Mutex;
  SerialSettings m_CachedSerialSettings{};
};

} // namespace brake_tester
