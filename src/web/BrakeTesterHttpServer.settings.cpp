#include "brake_tester/web/BrakeTesterHttpServer.hpp"
#include "web/BrakeTesterHttpServerInternal.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/print_manager.hpp"

namespace brake_tester {

void BrakeTesterHttpServer::configureSettingsModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring settings websocket module at /api/settings.");
  }

  m_Impl->server->WebSocket("/api/settings", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_Impl->settingsClientMutex);
      m_Impl->settingsClients.insert(&socket);
    }

    socket.send(buildSettingsStatePayloadText());

    std::string message;
    while (m_Impl->isRunning.load() && socket.read(message) != httplib::ws::Fail) {
      try {
        const auto payload = nlohmann::json::parse(message);
        const std::string action = payload.value("action", "");
        SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
        PrintSettings printSettings = m_PrintSettingsRepository.getPrintSettings();

        if (action == "assign_lpt") {
          const std::string devicePath = payload.value("devicePath", "");
          if (!devicePath.empty() && devicePath != serialSettings.lptDevicePath) {
            serialSettings.lptDevicePath = devicePath;
            m_SettingsRepository.setSerialSettings(serialSettings);
            m_LptStore.setLptSerialDeviceChanged(true);
            broadcastStatus("info", "LPT serial device updated");
            broadcastSettingsState();
          }
        } else if (action == "unassign_lpt") {
          if (!serialSettings.lptDevicePath.empty()) {
            serialSettings.lptDevicePath.clear();
            m_SettingsRepository.setSerialSettings(serialSettings);
            m_LptStore.setLptSerialDeviceChanged(true);
            broadcastStatus("warning", "LPT serial device cleared");
            broadcastSettingsState();
          }
        } else if (action == "assign_braketester") {
          const std::string devicePath = payload.value("devicePath", "");
          if (!devicePath.empty() && devicePath != serialSettings.brakeTesterDevicePath) {
            serialSettings.brakeTesterDevicePath = devicePath;
            m_SettingsRepository.setSerialSettings(serialSettings);
            m_LptStore.setBrakeTesterSerialDeviceChanged(true);
            broadcastStatus("info", "BrakeTester serial device updated");
            broadcastSettingsState();
          }
        } else if (action == "unassign_braketester") {
          if (!serialSettings.brakeTesterDevicePath.empty()) {
            serialSettings.brakeTesterDevicePath.clear();
            m_SettingsRepository.setSerialSettings(serialSettings);
            m_LptStore.setBrakeTesterSerialDeviceChanged(true);
            broadcastStatus("warning", "BrakeTester serial device cleared");
            broadcastSettingsState();
          }
        } else if (action == "select_printer") {
          printSettings.selectedPrinter = payload.value("printerName", "");
          m_PrintSettingsRepository.setPrintSettings(printSettings);
          if (m_Log) {
            m_Log->information("[BrakeTesterHttpServer Info]: Selected printer updated to '" +
                               printSettings.selectedPrinter + "'.");
          }
          broadcastStatus("info", printSettings.selectedPrinter.empty() ? "Printer cleared" : "Printer updated");
          broadcastSettingsState();
        } else if (action == "set_auto_print") {
          printSettings.autoPrint = payload.value("enabled", false);
          m_PrintSettingsRepository.setPrintSettings(printSettings);
          if (m_Log) {
            m_Log->information(std::string("[BrakeTesterHttpServer Info]: Auto print updated. enabled=") +
                               (printSettings.autoPrint ? "true" : "false"));
          }
          broadcastStatus("info", std::string("Auto print ") + (printSettings.autoPrint ? "enabled" : "disabled"));
          broadcastSettingsState();
        } else if (action == "refresh_printers") {
          if (m_Log) {
            m_Log->information("[BrakeTesterHttpServer Info]: Printer refresh requested by frontend.");
          }
          broadcastSettingsState();
        } else if (action == "test_lpt") {
          const bool setTestEnabled = payload.value("setTestEnabled", false);
          m_LptManager.sendTestSignal(setTestEnabled);
          broadcastStatus("progress", std::string("LPT test sent (") + (setTestEnabled ? "Test 1" : "Test 2") + ")");
        }
      } catch (const std::exception& ex) {
        if (m_Log) {
          m_Log->warning(std::string("[BrakeTesterHttpServer Warning]: Invalid /api/settings message: ") + ex.what());
        }
      }
    }

    {
      std::scoped_lock lock(m_Impl->settingsClientMutex);
      m_Impl->settingsClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::startSettingsBroadcastLoop() {
  m_Impl->settingsBroadcastThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Settings broadcast thread started.");
    }
    std::uint64_t lastSeenVersion = m_SerialDeviceStore.getVersion();

    while (m_Impl->isRunning.load()) {
      std::vector<std::string> devices;
      std::uint64_t version = lastSeenVersion;
      if (!m_SerialDeviceStore.waitForVersionAfter(lastSeenVersion, std::chrono::milliseconds(250), devices, version)) {
        continue;
      }

      lastSeenVersion = version;
      broadcastSettingsState();
    }
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Settings broadcast thread stopping.");
    }
  });
}

void BrakeTesterHttpServer::startPrintStatusBroadcastLoop() {
  m_Impl->printStatusBroadcastThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Print status broadcast thread started.");
    }

    std::uint64_t lastSeenVersion = m_PrintStatusStore.getVersion();
    while (m_Impl->isRunning.load()) {
      std::string statusText;
      std::uint64_t version = lastSeenVersion;
      if (!m_PrintStatusStore.waitForVersionAfter(lastSeenVersion,
                                                  std::chrono::milliseconds(250),
                                                  statusText,
                                                  version)) {
        continue;
      }

      lastSeenVersion = version;
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: Print status changed -> " + statusText);
      }

      if (statusText == "Print failed.") {
        broadcastStatus("error", statusText);
      } else if (statusText == "Printing...") {
        broadcastStatus("progress", statusText);
      } else if (statusText == "Idle") {
        broadcastStatus("idle", statusText);
      } else {
        broadcastStatus("info", statusText);
      }
    }

    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Print status broadcast thread stopping.");
    }
  });
}

std::string BrakeTesterHttpServer::buildSettingsStatePayloadText() const {
  const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
  const PrintSettings printSettings = m_PrintSettingsRepository.getPrintSettings();
  const auto devices = m_SerialDeviceStore.getDevices();
  const auto printers = m_PrintManager.listPrinters();

  nlohmann::json deviceItems = nlohmann::json::array();
  for (const auto& device : devices) {
    deviceItems.push_back(device);
  }

  nlohmann::json printerItems = nlohmann::json::array();
  for (const auto& printer : printers) {
    printerItems.push_back({{"name", printer.name}, {"info", printer.info}});
  }

  const nlohmann::json payload = {
      {"event", "settings.state"},
      {"serialDevices", deviceItems},
      {"lptDevicePath", serialSettings.lptDevicePath},
      {"brakeTesterDevicePath", serialSettings.brakeTesterDevicePath},
      {"printers", printerItems},
      {"selectedPrinter", printSettings.selectedPrinter},
      {"autoPrint", printSettings.autoPrint},
      {"printStatus", m_PrintStatusStore.getStatus()},
  };
  return payload.dump();
}

void BrakeTesterHttpServer::broadcastSettingsState() {
  const std::string payload = buildSettingsStatePayloadText();
  std::scoped_lock lock(m_Impl->settingsClientMutex);
  for (auto* socket : m_Impl->settingsClients) {
    if (socket != nullptr) {
      socket->send(payload);
    }
  }
}

} // namespace brake_tester
