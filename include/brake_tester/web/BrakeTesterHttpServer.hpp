#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace httplib {
class Server;
namespace ws {
class WebSocket;
} // namespace ws
} // namespace httplib

namespace brake_tester {

class LptManager;

class BrakeTesterHttpServer {
public:
  BrakeTesterHttpServer(ILptStore& lptStore,
                        ISettingsRepository& settingsRepository,
                        ISerialDeviceStore& serialDeviceStore,
                        IVehicleRepository& vehicleRepository,
                        ISelectedVehicleStore& selectedVehicleStore,
                        LptManager& lptManager,
                        SharedLogger log,
                        std::string host = "0.0.0.0",
                        int port = 80,
                        std::string staticRoot = "www");
  ~BrakeTesterHttpServer();

  void start();
  void stop();

private:
  void configureLptModule();
  void configureVehicleModule();
  void configureStatusModule();
  void configureSettingsModule();
  void startLptBroadcastLoop();
  void startSettingsBroadcastLoop();
  void broadcastVehicleState();
  void broadcastSettingsState();
  nlohmann::json buildVehicleStatePayload() const;
  nlohmann::json buildSettingsStatePayload() const;
  std::string lptEventNameForStatus(LptProcessStatus status) const;
  void broadcastStatus(const std::string& level, const std::string& text);

  ILptStore& m_LptStore;
  ISettingsRepository& m_SettingsRepository;
  ISerialDeviceStore& m_SerialDeviceStore;
  IVehicleRepository& m_VehicleRepository;
  ISelectedVehicleStore& m_SelectedVehicleStore;
  LptManager& m_LptManager;
  SharedLogger m_Log;
  std::string m_Host;
  int m_Port;
  std::string m_StaticRoot;

  std::atomic_bool m_IsRunning{false};
  std::unique_ptr<httplib::Server> m_Server;
  std::thread m_ServerThread;

  std::thread m_LptBroadcastThread;
  std::thread m_SettingsBroadcastThread;
  std::mutex m_LptClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_LptClients;

  std::mutex m_VehicleClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_VehicleClients;

  std::mutex m_StatusClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_StatusClients;

  std::mutex m_SettingsClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_SettingsClients;
};

} // namespace brake_tester
