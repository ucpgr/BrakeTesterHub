#pragma once

#include <mutex>
#include <stdexcept>

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class SettingsRepository final : public ISettingsRepository {
public:
  explicit SettingsRepository(sqlite3* db) : db_(db) {
    if (db_ == nullptr) {
      throw std::invalid_argument("SettingsRepository requires a valid sqlite3 handle");
    }
  }

  SerialSettings getSerialSettings() const override {
    std::scoped_lock lock(mutex_);
    return cached_settings_;
  }

  void setSerialSettings(const SerialSettings& settings) override {
    std::scoped_lock lock(mutex_);
    cached_settings_ = settings;

    // Placeholder for sqlite persistence via db_.
    // Production implementation can bind and execute a prepared statement.
  }

private:
  sqlite3* db_;
  mutable std::mutex mutex_;
  SerialSettings cached_settings_{};
};

class SelectedVehicleStore final : public ISelectedVehicleStore {
public:
  explicit SelectedVehicleStore(sqlite3* db) : db_(db) {
    if (db_ == nullptr) {
      throw std::invalid_argument("SelectedVehicleStore requires a valid sqlite3 handle");
    }
  }

  VehicleSelection getSelectedVehicle() const override {
    std::scoped_lock lock(mutex_);
    return selected_vehicle_;
  }

  void setSelectedVehicle(const VehicleSelection& selected_vehicle) override {
    std::scoped_lock lock(mutex_);
    selected_vehicle_ = selected_vehicle;

    // Placeholder for sqlite persistence via db_.
    // Production implementation can bind and execute a prepared statement.
  }

private:
  sqlite3* db_;
  mutable std::mutex mutex_;
  VehicleSelection selected_vehicle_{};
};

} // namespace brake_tester
