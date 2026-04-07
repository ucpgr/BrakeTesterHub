#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "brake_tester/components.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/logging.hpp"
#include "brake_tester/repositories.hpp"

namespace brake_tester {

class App {
public:
  explicit App(std::string databasePath) {
    m_Log = std::make_shared<Logger>(LogVerbosity::Information);
    m_Log->information("[App Info]: Initializing application.");

    if (sqlite3_open(databasePath.c_str(), &m_DatabaseHandle) != SQLITE_OK) {
      throw std::runtime_error("Failed to open sqlite database");
    }
    m_Log->information("[App Info]: Opened sqlite database at path: " + databasePath);

    m_SettingsRepository = std::make_unique<SettingsRepository>(m_DatabaseHandle, m_Log);
    m_SelectedVehicleStore = std::make_unique<SelectedVehicleStore>();

    auto listener = std::make_unique<LptListener>(*m_SettingsRepository, m_Log);
    auto patcher = std::make_unique<PrnPatcher>(*m_SelectedVehicleStore, m_Log);
    patcher->addPatch(0x45F, [](const VehicleSelection&) { return std::string("patched"); });

    auto renderer = std::make_unique<PrnRenderer>(m_Log);
    auto writer = std::make_unique<RenderedDocumentWriter>("output", m_Log);

    m_LptManager = std::make_unique<LptManager>(
        std::move(listener), std::move(patcher), std::move(renderer), std::move(writer), m_Log);
  }

  ~App() {
    shutdown();
  }

  void run() {
    m_Log->information("[App Info]: Starting application runtime.");
    m_LptManager->start();
  }

  void shutdown() {
    if (m_Log) {
      m_Log->information("[App Info]: Shutting down application.");
    }
    if (m_LptManager) {
      m_LptManager->stop();
    }

    if (m_DatabaseHandle != nullptr) {
      sqlite3_close(m_DatabaseHandle);
      m_DatabaseHandle = nullptr;
    }
  }

private:
  SharedLogger m_Log;
  sqlite3* m_DatabaseHandle{nullptr};

  std::unique_ptr<SettingsRepository> m_SettingsRepository;
  std::unique_ptr<SelectedVehicleStore> m_SelectedVehicleStore;
  std::unique_ptr<LptManager> m_LptManager;
};

} // namespace brake_tester
