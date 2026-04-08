#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
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
    m_LptStore = std::make_unique<LptStore>();

    auto listener = std::make_unique<LptListener>(*m_SettingsRepository, *m_LptStore, m_Log);
    auto patcher = std::make_unique<PrnPatcher>(*m_SelectedVehicleStore, m_Log);
    patcher->addPatch(0x45F, [](const VehicleSelection&) { return std::string("patched"); });

    auto renderer = std::make_unique<PrnRenderer>(m_Log);
    auto writer = std::make_unique<RenderedDocumentWriter>(".", m_Log);
    auto prnWriter = std::make_unique<PrnWriter>(".", m_Log);

    m_LptManager = std::make_unique<LptManager>(std::move(listener),
                                                std::move(patcher),
                                                std::move(renderer),
                                                std::move(writer),
                                                std::move(prnWriter),
                                                *m_LptStore,
                                                *m_SettingsRepository,
                                                m_Log);
  }

  ~App() {
    shutdown();
  }

  void run() {
    m_Log->information("[App Info]: Starting application runtime.");
    m_LptManager->start();
    startInputListener();
  }

  void shutdown() {
    m_IsInputListening = false;
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
  void startInputListener() {
    if (m_IsInputListening.exchange(true)) {
      return;
    }

    std::thread([this]() {
      if (m_Log) {
        m_Log->information("[App Info]: Type 't' then Enter to send a test byte.");
      }

      std::string consoleInput;
      while (m_IsInputListening) {
        if (!std::getline(std::cin, consoleInput)) {
          std::cin.clear();
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }

        if (consoleInput == "t") {
          m_LptManager->sendTestSignal();
        }
      }
    }).detach();
  }

  std::atomic_bool m_IsInputListening{false};
  SharedLogger m_Log;
  sqlite3* m_DatabaseHandle{nullptr};

  std::unique_ptr<SettingsRepository> m_SettingsRepository;
  std::unique_ptr<SelectedVehicleStore> m_SelectedVehicleStore;
  std::unique_ptr<LptStore> m_LptStore;
  std::unique_ptr<LptManager> m_LptManager;
};

} // namespace brake_tester
