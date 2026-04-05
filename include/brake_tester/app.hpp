#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <sqlite3.h>

#include "brake_tester/components.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/repositories.hpp"

namespace brake_tester {

class App {
public:
  explicit App(std::string database_path) {
    if (sqlite3_open(database_path.c_str(), &db_) != SQLITE_OK) {
      throw std::runtime_error("Failed to open sqlite database");
    }

    settings_repository_ = std::make_unique<SettingsRepository>(db_);
    selected_vehicle_store_ = std::make_unique<SelectedVehicleStore>(db_);

    auto listener = std::make_unique<LptListener>(*settings_repository_);
    auto patcher = std::make_unique<PrnPatcher>(*selected_vehicle_store_);
    patcher->addPatch(0x45F, [](const VehicleSelection&) { return std::string("patched"); });

    auto renderer = std::make_unique<PrnRenderer>();
    auto writer = std::make_unique<RenderedDocumentWriter>("output");

    lpt_manager_ = std::make_unique<LptManager>(
        std::move(listener), std::move(patcher), std::move(renderer), std::move(writer));
  }

  ~App() {
    shutdown();
  }

  void run() {
    lpt_manager_->start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    lpt_manager_->stop();
  }

  void shutdown() {
    if (lpt_manager_) {
      lpt_manager_->stop();
    }

    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
  }

private:
  sqlite3* db_{nullptr};

  std::unique_ptr<SettingsRepository> settings_repository_;
  std::unique_ptr<SelectedVehicleStore> selected_vehicle_store_;
  std::unique_ptr<LptManager> lpt_manager_;
};

} // namespace brake_tester
