#pragma once

#include <mutex>

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrintSettingsRepository final : public IPrintSettingsRepository {
public:
  PrintSettingsRepository(sqlite3* databaseHandle, SharedLogger log);

  PrintSettings getPrintSettings() const override;
  void setPrintSettings(const PrintSettings& printSettings) override;

private:
  void initializeSchema();
  void loadCachedPrintSettings();
  std::string getSettingValueOrDefault(const std::string& settingName, const std::string& defaultValue) const;
  void upsertCachedPrintSettings() const;
  void persistSetting(sqlite3_stmt* statement, const std::string& settingName, const std::string& settingValue) const;
  void executeSql(const char* sqlText) const;

  static void resetStatement(sqlite3_stmt* statement);
  static void finalizeStatement(sqlite3_stmt* statement);

  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
  mutable std::mutex m_Mutex;
  PrintSettings m_CachedPrintSettings{};
};

} // namespace brake_tester
