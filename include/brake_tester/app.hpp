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
  explicit App(std::string databasePath) {
    if (sqlite3_open(databasePath.c_str(), &m_DatabaseHandle) != SQLITE_OK) {
      throw std::runtime_error("Failed to open sqlite database");
    }

    m_SettingsRepository = std::make_unique<SettingsRepository>(m_DatabaseHandle);
    m_SelectedVehicleStore = std::make_unique<SelectedVehicleStore>(m_DatabaseHandle);

    auto listener = std::make_unique<LptListener>(*m_SettingsRepository);
    auto patcher = std::make_unique<PrnPatcher>(*m_SelectedVehicleStore);
    patcher->addPatch(0x45F, [](const VehicleSelection&) { return std::string("patched"); });

    auto renderer = std::make_unique<PrnRenderer>();
    auto writer = std::make_unique<RenderedDocumentWriter>("output");

    m_LptManager = std::make_unique<LptManager>(
        std::move(listener), std::move(patcher), std::move(renderer), std::move(writer));
  }

  ~App() {
    shutdown();
  }

  void run() {
    m_LptManager->start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    m_LptManager->stop();
  }

  void shutdown() {
    if (m_LptManager) {
      m_LptManager->stop();
    }

    if (m_DatabaseHandle != nullptr) {
      sqlite3_close(m_DatabaseHandle);
      m_DatabaseHandle = nullptr;
    }
  }

private:
  sqlite3* m_DatabaseHandle{nullptr};

  std::unique_ptr<SettingsRepository> m_SettingsRepository;
  std::unique_ptr<SelectedVehicleStore> m_SelectedVehicleStore;
  std::unique_ptr<LptManager> m_LptManager;
};

} // namespace brake_tester
