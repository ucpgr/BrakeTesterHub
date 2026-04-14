#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <sqlite3.h>

#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager;
class BrakeTesterHttpServer;
class SettingsRepository;
class VehicleRepository;
class LptRepository;
class SelectedVehicleStore;
class LptStore;

class App {
public:
  explicit App(std::string databasePath);
  ~App();

  void run();
  void shutdown();

private:
  void startInputListener();

  std::atomic_bool m_IsInputListening{false};
  SharedLogger m_Log;
  sqlite3* m_DatabaseHandle{nullptr};

  std::unique_ptr<SettingsRepository> m_SettingsRepository;
  std::unique_ptr<VehicleRepository> m_VehicleRepository;
  std::unique_ptr<LptRepository> m_LptRepository;
  std::unique_ptr<SelectedVehicleStore> m_SelectedVehicleStore;
  std::unique_ptr<LptStore> m_LptStore;
  std::unique_ptr<LptManager> m_LptManager;
  std::unique_ptr<BrakeTesterHttpServer> m_HttpServer;
};

} // namespace brake_tester
