#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

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
                        SharedLogger log,
                        std::string host = "0.0.0.0",
                        int port = 8080,
                        std::string staticRoot = "www");
  ~BrakeTesterHttpServer();

  void start();
  void stop();

private:
  void configureLptModule();
  void startLptBroadcastLoop();
  std::string lptEventNameForStatus(LptProcessStatus status) const;

  ILptStore& m_LptStore;
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
};

} // namespace brake_tester
