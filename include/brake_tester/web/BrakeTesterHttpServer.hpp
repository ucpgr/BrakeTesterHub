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

class BrakeTesterHttpServer {
public:
  BrakeTesterHttpServer(ILptStore& lptStore,
                        IVehicleRepository& vehicleRepository,
                        ISelectedVehicleStore& selectedVehicleStore,
                        SharedLogger log,
                        std::string host = "0.0.0.0",
                        int port = 8080,
                        std::string staticRoot = "www");
  ~BrakeTesterHttpServer();

  void start();
  void stop();

private:
  void configureLptModule();
  void configureVehicleModule();
  void configureStatusModule();
  void startLptBroadcastLoop();
  void broadcastVehicleState();
  nlohmann::json buildVehicleStatePayload() const;
  std::string lptEventNameForStatus(LptProcessStatus status) const;
  void broadcastStatus(const std::string& level, const std::string& text);

  ILptStore& m_LptStore;
  IVehicleRepository& m_VehicleRepository;
  ISelectedVehicleStore& m_SelectedVehicleStore;
  SharedLogger m_Log;
  std::string m_Host;
  int m_Port;
  std::string m_StaticRoot;

  std::atomic_bool m_IsRunning{false};
  std::unique_ptr<httplib::Server> m_Server;
  std::thread m_ServerThread;

  std::thread m_LptBroadcastThread;
  std::mutex m_LptClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_LptClients;

  std::mutex m_VehicleClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_VehicleClients;

  std::mutex m_StatusClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> m_StatusClients;
};

} // namespace brake_tester
