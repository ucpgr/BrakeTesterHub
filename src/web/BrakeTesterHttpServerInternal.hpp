#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <httplib.h>

#include "brake_tester/web/BrakeTesterHttpServer.hpp"

namespace brake_tester {

struct BrakeTesterHttpServer::Impl {
  std::atomic_bool isRunning{false};
  std::unique_ptr<httplib::Server> server{std::make_unique<httplib::Server>()};
  std::thread serverThread;

  std::thread lptBroadcastThread;
  std::thread settingsBroadcastThread;
  std::mutex lptClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> lptClients;

  std::mutex vehicleClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> vehicleClients;

  std::mutex statusClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> statusClients;

  std::mutex settingsClientMutex;
  std::unordered_set<httplib::ws::WebSocket*> settingsClients;
};

} // namespace brake_tester
