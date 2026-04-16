#include "brake_tester/web/BrakeTesterHttpServer.hpp"

#include <chrono>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "brake_tester/lpt_manager.hpp"

namespace brake_tester {

BrakeTesterHttpServer::BrakeTesterHttpServer(ILptStore& lptStore,
                                             ISettingsRepository& settingsRepository,
                                             ISerialDeviceStore& serialDeviceStore,
                                             IVehicleRepository& vehicleRepository,
                                             ISelectedVehicleStore& selectedVehicleStore,
                                             LptManager& lptManager,
                                             SharedLogger log,
                                             std::string host,
                                             int port,
                                             std::string staticRoot)
    : m_LptStore(lptStore),
      m_SettingsRepository(settingsRepository),
      m_SerialDeviceStore(serialDeviceStore),
      m_VehicleRepository(vehicleRepository),
      m_SelectedVehicleStore(selectedVehicleStore),
      m_LptManager(lptManager),
      m_Log(std::move(log)),
      m_Host(std::move(host)),
      m_Port(port),
      m_StaticRoot(std::move(staticRoot)),
      m_Server(std::make_unique<httplib::Server>()) {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Constructed for " + m_Host + ":" + std::to_string(m_Port) +
                       ", staticRoot=" + m_StaticRoot);
  }
}

BrakeTesterHttpServer::~BrakeTesterHttpServer() {
  stop();
}

void BrakeTesterHttpServer::start() {
  if (m_IsRunning.exchange(true)) {
    if (m_Log) {
      m_Log->warning("[BrakeTesterHttpServer Warning]: Start requested while server already running.");
    }
    return;
  }

  m_Server->set_mount_point("/", m_StaticRoot);
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Mounted static files at '/' from " + m_StaticRoot);
  }
  configureLptModule();
  configureVehicleModule();
  configureStatusModule();
  configureSettingsModule();
  startLptBroadcastLoop();
  startSettingsBroadcastLoop();

  m_ServerThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: HTTP server listening on " + m_Host + ":" +
                         std::to_string(m_Port));
    }
    m_Server->listen(m_Host, m_Port);
  });
}

void BrakeTesterHttpServer::stop() {
  if (!m_IsRunning.exchange(false)) {
    if (m_Log) {
      m_Log->warning("[BrakeTesterHttpServer Warning]: Stop requested while server not running.");
    }
    return;
  }
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Stopping HTTP server.");
  }

  if (m_Server) {
    m_Server->stop();
  }

  if (m_LptBroadcastThread.joinable()) {
    m_LptBroadcastThread.join();
  }
  if (m_SettingsBroadcastThread.joinable()) {
    m_SettingsBroadcastThread.join();
  }

  if (m_ServerThread.joinable()) {
    m_ServerThread.join();
  }
}

void BrakeTesterHttpServer::configureLptModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring LPT websocket module at /lpt.");
  }
  m_Server->WebSocket("/lpt", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_LptClientMutex);
      m_LptClients.insert(&socket);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: /lpt client connected. total=" +
                           std::to_string(m_LptClients.size()));
      }
    }

    std::string message;
    while (m_IsRunning.load() && socket.read(message) != httplib::ws::Fail) {
    }

    {
      std::scoped_lock lock(m_LptClientMutex);
      m_LptClients.erase(&socket);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: /lpt client disconnected. total=" +
                           std::to_string(m_LptClients.size()));
      }
    }
  });
}

void BrakeTesterHttpServer::startLptBroadcastLoop() {
  m_LptBroadcastThread = std::thread([this] {
    std::uint64_t lastSeenVersion = m_LptStore.getProcessStatusVersion();

    while (m_IsRunning.load()) {
      LptProcessStatus status = LptProcessStatus::Idle;
      std::uint64_t version = lastSeenVersion;

      if (!m_LptStore.waitForProcessStatusAfter(lastSeenVersion, std::chrono::milliseconds(250), status, version)) {
        continue;
      }

      lastSeenVersion = version;
      nlohmann::json payload{{"event", lptEventNameForStatus(status)}};
      const auto payloadText = payload.dump();

      std::scoped_lock lock(m_LptClientMutex);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: Broadcasting LPT event to " +
                           std::to_string(m_LptClients.size()) + " client(s): " + payloadText);
      }
      for (auto* socket : m_LptClients) {
        if (socket != nullptr) {
          socket->send(payloadText);
        }
      }

      switch (status) {
        case LptProcessStatus::TransferStarted:
          broadcastStatus("progress", "Capture transfer started");
          break;
        case LptProcessStatus::DataPatched:
          broadcastStatus("progress", "Captured data patched");
          break;
        case LptProcessStatus::ConversionStarted:
          broadcastStatus("progress", "Document conversion started");
          break;
        case LptProcessStatus::ConversionFinished:
          broadcastStatus("info", "Document conversion finished");
          break;
        default:
          broadcastStatus("idle", "Idle");
          break;
      }
    }
  });
}

void BrakeTesterHttpServer::configureVehicleModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring vehicle websocket module at /vehicles.");
  }

  m_Server->WebSocket("/vehicles", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_VehicleClientMutex);
      m_VehicleClients.insert(&socket);
    }

    socket.send(buildVehicleStatePayload().dump());

    std::string message;
    while (m_IsRunning.load() && socket.read(message) != httplib::ws::Fail) {
      try {
        const auto payload = nlohmann::json::parse(message);
        const std::string action = payload.value("action", "");

        if (action == "add") {
          const auto vehiclePayload = payload.at("vehicle");
          VehicleSelection vehicle;
          vehicle.reg = vehiclePayload.value("reg", "");
          vehicle.make = vehiclePayload.value("make", "");
          vehicle.model = vehiclePayload.value("model", "");

          if (vehiclePayload.contains("mileage") && vehiclePayload["mileage"].is_string()) {
            const std::string mileageValue = vehiclePayload.value("mileage", "");
            if (!mileageValue.empty()) {
              vehicle.mileage = mileageValue;
            }
          } else if (vehiclePayload.contains("mileage") && vehiclePayload["mileage"].is_number()) {
            const std::string mileageUnit = vehiclePayload.value("mileageUnit", "km");
            std::string mileageValue;

            if (vehiclePayload["mileage"].is_number_integer()) {
              mileageValue = std::to_string(vehiclePayload.value("mileage", 0));
            } else {
              mileageValue = std::to_string(vehiclePayload.value("mileage", 0.0));
              mileageValue.erase(mileageValue.find_last_not_of('0') + 1);
              if (!mileageValue.empty() && mileageValue.back() == '.') {
                mileageValue.pop_back();
              }
            }

            if (!mileageValue.empty()) {
              vehicle.mileage = mileageValue + mileageUnit;
            }
          }

          if (!vehicle.reg.empty() && !vehicle.make.empty() && !vehicle.model.empty()) {
            VehicleSelection created = m_VehicleRepository.addVehicle(vehicle);
            m_SelectedVehicleStore.setSelectedVehicle(created);
            if (m_Log) {
              m_Log->information("[BrakeTesterHttpServer Info]: Vehicle saved: id=" + std::to_string(created.id) +
                                 ", reg=" + created.reg + ", make=" + created.make + ", model=" + created.model);
            }
            broadcastVehicleState();
            broadcastStatus("info", "Vehicle saved: " + created.reg);
          }
        } else if (action == "select") {
          const int id = payload.value("id", 0);
          if (id <= 0) {
            m_SelectedVehicleStore.setSelectedVehicle({});
            if (m_Log) {
              m_Log->information("[BrakeTesterHttpServer Info]: Vehicle selection cleared.");
            }
            broadcastStatus("info", "No vehicle selected");
          } else {
            VehicleSelection selected;
            if (m_VehicleRepository.tryGetVehicle(id, selected)) {
              m_SelectedVehicleStore.setSelectedVehicle(selected);
              if (m_Log) {
                m_Log->information("[BrakeTesterHttpServer Info]: Vehicle selected: id=" + std::to_string(selected.id) +
                                   ", reg=" + selected.reg + ", make=" + selected.make + ", model=" + selected.model);
              }
              broadcastStatus("info", "Vehicle selected: " + selected.reg);
            }
          }
          broadcastVehicleState();
        } else if (action == "update_mileage") {
          const int id = payload.value("id", 0);
          std::optional<std::string> mileage;

          if (payload.contains("mileage") && payload["mileage"].is_string()) {
            const std::string mileageValue = payload.value("mileage", "");
            if (!mileageValue.empty()) {
              mileage = mileageValue;
            }
          }

          if (id > 0 && m_VehicleRepository.updateVehicleMileage(id, mileage)) {
            VehicleSelection selected;
            if (m_VehicleRepository.tryGetVehicle(id, selected)) {
              const VehicleSelection currentSelection = m_SelectedVehicleStore.getSelectedVehicle();
              if (currentSelection.id == id) {
                m_SelectedVehicleStore.setSelectedVehicle(selected);
              }

              if (m_Log) {
                m_Log->information("[BrakeTesterHttpServer Info]: Vehicle mileage updated: id=" + std::to_string(id) +
                                   ", mileage=" + (selected.mileage.has_value() ? *selected.mileage : "<none>"));
              }
              broadcastStatus("info", "Vehicle mileage updated");
            }
            broadcastVehicleState();
          }
        } else if (action == "delete") {
          const int id = payload.value("id", 0);
          if (id > 0 && m_VehicleRepository.deleteVehicle(id)) {
            const VehicleSelection selected = m_SelectedVehicleStore.getSelectedVehicle();
            if (selected.id == id) {
              m_SelectedVehicleStore.setSelectedVehicle({});
            }
            if (m_Log) {
              m_Log->information("[BrakeTesterHttpServer Info]: Vehicle deleted: id=" + std::to_string(id));
            }
            broadcastVehicleState();
            broadcastStatus("warning", "Vehicle deleted");
          }
        }
      } catch (const std::exception& ex) {
        if (m_Log) {
          m_Log->warning(std::string("[BrakeTesterHttpServer Warning]: Invalid /vehicles message: ") + ex.what());
        }
      }
    }

    {
      std::scoped_lock lock(m_VehicleClientMutex);
      m_VehicleClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::configureSettingsModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring settings websocket module at /settings.");
  }

  m_Server->WebSocket("/settings", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_SettingsClientMutex);
      m_SettingsClients.insert(&socket);
    }

    socket.send(buildSettingsStatePayload().dump());

    std::string message;
    while (m_IsRunning.load() && socket.read(message) != httplib::ws::Fail) {
      try {
        const auto payload = nlohmann::json::parse(message);
        const std::string action = payload.value("action", "");
        SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();

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
        } else if (action == "test_lpt") {
          const bool setTestEnabled = payload.value("setTestEnabled", false);
          m_LptManager.sendTestSignal(setTestEnabled);
          broadcastStatus("progress", std::string("LPT test sent (") + (setTestEnabled ? "Test 1" : "Test 2") + ")");
        }
      } catch (const std::exception& ex) {
        if (m_Log) {
          m_Log->warning(std::string("[BrakeTesterHttpServer Warning]: Invalid /settings message: ") + ex.what());
        }
      }
    }

    {
      std::scoped_lock lock(m_SettingsClientMutex);
      m_SettingsClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::configureStatusModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring status websocket module at /status.");
  }

  m_Server->WebSocket("/status", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_StatusClientMutex);
      m_StatusClients.insert(&socket);
    }

    socket.send(nlohmann::json{{"event", "status.update"}, {"status", {{"level", "idle"}, {"text", "Idle"}}}}.dump());

    std::string message;
    while (m_IsRunning.load() && socket.read(message) != httplib::ws::Fail) {
    }

    {
      std::scoped_lock lock(m_StatusClientMutex);
      m_StatusClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::broadcastStatus(const std::string& level, const std::string& text) {
  const nlohmann::json payload = {
      {"event", "status.update"},
      {"status", {{"level", level}, {"text", text}}},
  };

  const std::string payloadText = payload.dump();
  std::scoped_lock lock(m_StatusClientMutex);
  for (auto* socket : m_StatusClients) {
    if (socket != nullptr) {
      socket->send(payloadText);
    }
  }
}

void BrakeTesterHttpServer::startSettingsBroadcastLoop() {
  m_SettingsBroadcastThread = std::thread([this] {
    std::uint64_t lastSeenVersion = m_SerialDeviceStore.getVersion();

    while (m_IsRunning.load()) {
      std::vector<std::string> devices;
      std::uint64_t version = lastSeenVersion;
      if (!m_SerialDeviceStore.waitForVersionAfter(lastSeenVersion, std::chrono::milliseconds(250), devices, version)) {
        continue;
      }

      lastSeenVersion = version;
      broadcastSettingsState();
    }
  });
}

nlohmann::json BrakeTesterHttpServer::buildSettingsStatePayload() const {
  const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
  const auto devices = m_SerialDeviceStore.getDevices();

  nlohmann::json deviceItems = nlohmann::json::array();
  for (const auto& device : devices) {
    deviceItems.push_back(device);
  }

  return {
      {"event", "settings.state"},
      {"serialDevices", deviceItems},
      {"lptDevicePath", serialSettings.lptDevicePath},
      {"brakeTesterDevicePath", serialSettings.brakeTesterDevicePath},
  };
}

void BrakeTesterHttpServer::broadcastSettingsState() {
  const std::string payload = buildSettingsStatePayload().dump();
  std::scoped_lock lock(m_SettingsClientMutex);
  for (auto* socket : m_SettingsClients) {
    if (socket != nullptr) {
      socket->send(payload);
    }
  }
}

nlohmann::json BrakeTesterHttpServer::buildVehicleStatePayload() const {
  const auto vehicles = m_VehicleRepository.getVehicles();
  const VehicleSelection selectedVehicle = m_SelectedVehicleStore.getSelectedVehicle();

  nlohmann::json vehicleItems = nlohmann::json::array();
  for (const auto& vehicle : vehicles) {
    nlohmann::json vehicleItem = {
        {"id", vehicle.id},
        {"reg", vehicle.reg},
        {"make", vehicle.make},
        {"model", vehicle.model},
    };

    if (vehicle.mileage.has_value()) {
      vehicleItem["mileage"] = *vehicle.mileage;
    } else {
      vehicleItem["mileage"] = nullptr;
    }

    vehicleItems.push_back(vehicleItem);
  }

  const nlohmann::json selectedVehicleId = selectedVehicle.id > 0 ? nlohmann::json(selectedVehicle.id)
                                                                 : nlohmann::json(nullptr);

  return {
      {"event", "vehicles.state"},
      {"vehicles", vehicleItems},
      {"selectedVehicleId", selectedVehicleId},
  };
}

void BrakeTesterHttpServer::broadcastVehicleState() {
  const std::string payload = buildVehicleStatePayload().dump();
  std::scoped_lock lock(m_VehicleClientMutex);
  for (auto* socket : m_VehicleClients) {
    if (socket != nullptr) {
      socket->send(payload);
    }
  }
}

std::string BrakeTesterHttpServer::lptEventNameForStatus(LptProcessStatus status) const {
  switch (status) {
    case LptProcessStatus::TransferStarted: return "lpt.transfer_started";
    case LptProcessStatus::DataPatched: return "lpt.data_patched";
    case LptProcessStatus::ConversionStarted: return "lpt.conversion_started";
    case LptProcessStatus::ConversionFinished: return "lpt.conversion_finished";
    case LptProcessStatus::Idle:
    default: return "lpt.idle";
  }
}

} // namespace brake_tester
