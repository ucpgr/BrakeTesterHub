#include "brake_tester/web/BrakeTesterHttpServer.hpp"
#include "web/BrakeTesterHttpServerInternal.hpp"

#include <optional>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {

void BrakeTesterHttpServer::configureVehicleModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring vehicle websocket module at /api/vehicles.");
  }

  m_Impl->server->WebSocket("/api/vehicles", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_Impl->vehicleClientMutex);
      m_Impl->vehicleClients.insert(&socket);
    }

    socket.send(buildVehicleStatePayloadText());

    std::string message;
    while (m_Impl->isRunning.load() && socket.read(message) != httplib::ws::Fail) {
      try {
        const auto payload = nlohmann::json::parse(message);
        const std::string action = payload.value("action", "");

        if (action == "add") {
          const auto vehiclePayload = payload.at("vehicle");
          VehicleSelection vehicle;
          vehicle.reg = vehiclePayload.value("reg", "");
          vehicle.make = vehiclePayload.value("make", "");
          vehicle.model = vehiclePayload.value("model", "");

          // Frontend clients may send mileage as free text or as a numeric value plus unit.
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
          m_Log->warning(std::string("[BrakeTesterHttpServer Warning]: Invalid /api/vehicles message: ") + ex.what());
        }
      }
    }

    {
      std::scoped_lock lock(m_Impl->vehicleClientMutex);
      m_Impl->vehicleClients.erase(&socket);
    }
  });
}

std::string BrakeTesterHttpServer::buildVehicleStatePayloadText() const {
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

  const nlohmann::json payload = {
      {"event", "vehicles.state"},
      {"vehicles", vehicleItems},
      {"selectedVehicleId", selectedVehicleId},
  };
  return payload.dump();
}

void BrakeTesterHttpServer::broadcastVehicleState() {
  const std::string payload = buildVehicleStatePayloadText();
  std::scoped_lock lock(m_Impl->vehicleClientMutex);
  for (auto* socket : m_Impl->vehicleClients) {
    if (socket != nullptr) {
      socket->send(payload);
    }
  }
}

} // namespace brake_tester
