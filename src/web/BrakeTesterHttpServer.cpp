#include "brake_tester/web/BrakeTesterHttpServer.hpp"

#include <chrono>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {

BrakeTesterHttpServer::BrakeTesterHttpServer(ILptStore& lptStore,
                                             IVehicleRepository& vehicleRepository,
                                             ISelectedVehicleStore& selectedVehicleStore,
                                             SharedLogger log,
                                             std::string host,
                                             int port,
                                             std::string staticRoot)
    : m_LptStore(lptStore),
      m_VehicleRepository(vehicleRepository),
      m_SelectedVehicleStore(selectedVehicleStore),
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
  startLptBroadcastLoop();

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
            broadcastVehicleState();
          }
        } else if (action == "select") {
          const int id = payload.value("id", 0);
          if (id <= 0) {
            m_SelectedVehicleStore.setSelectedVehicle({});
          } else {
            VehicleSelection selected;
            if (m_VehicleRepository.tryGetVehicle(id, selected)) {
              m_SelectedVehicleStore.setSelectedVehicle(selected);
            }
          }
          broadcastVehicleState();
        } else if (action == "delete") {
          const int id = payload.value("id", 0);
          if (id > 0 && m_VehicleRepository.deleteVehicle(id)) {
            const VehicleSelection selected = m_SelectedVehicleStore.getSelectedVehicle();
            if (selected.id == id) {
              m_SelectedVehicleStore.setSelectedVehicle({});
            }
            broadcastVehicleState();
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
