#pragma once

#include <mutex>
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
  }

  SerialSettings getSerialSettings() const override {
    std::scoped_lock lock(m_Mutex);
    return m_CachedSerialSettings;
  }

  void setSerialSettings(const SerialSettings& serialSettings) override {
    std::scoped_lock lock(m_Mutex);
    m_CachedSerialSettings = serialSettings;

    // Placeholder for sqlite persistence via m_DatabaseHandle.
    // Production implementation can bind and execute a prepared statement.
  }

private:
  sqlite3* m_DatabaseHandle;
  mutable std::mutex m_Mutex;
  SerialSettings m_CachedSerialSettings{};
};

class SelectedVehicleStore final : public ISelectedVehicleStore {
public:
  explicit SelectedVehicleStore(sqlite3* databaseHandle) : m_DatabaseHandle(databaseHandle) {
    if (m_DatabaseHandle == nullptr) {
      throw std::invalid_argument("SelectedVehicleStore requires a valid sqlite3 handle");
    }
  }

  VehicleSelection getSelectedVehicle() const override {
    std::scoped_lock lock(m_Mutex);
    return m_SelectedVehicle;
  }

  void setSelectedVehicle(const VehicleSelection& selectedVehicle) override {
    std::scoped_lock lock(m_Mutex);
    m_SelectedVehicle = selectedVehicle;

    // Placeholder for sqlite persistence via m_DatabaseHandle.
    // Production implementation can bind and execute a prepared statement.
  }

private:
  sqlite3* m_DatabaseHandle;
  mutable std::mutex m_Mutex;
  VehicleSelection m_SelectedVehicle{};
};

} // namespace brake_tester
