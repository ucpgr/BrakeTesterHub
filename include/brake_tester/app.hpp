#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <sqlite3.h>

#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager;
class BrakeTesterHttpServer;
class SettingsRepository;
class PrintSettingsRepository;
class VehicleRepository;
class LptRepository;
class CurrentTestAxleDataStore;
class PrnPayloadStore;
class SelectedVehicleStore;
class LptStore;
class SerialDeviceStore;
class PrintStatusStore;
class PrintManager;
class BrakeTesterSerialDispatcher;

class App {
public:
  explicit App(std::string databasePath);
  ~App();

  void run();
  void shutdown();

private:
  void startSerialDeviceRefreshLoop();

  SharedLogger m_Log;
  sqlite3* m_DatabaseHandle{nullptr};

  std::unique_ptr<SettingsRepository> m_SettingsRepository;
  std::unique_ptr<PrintSettingsRepository> m_PrintSettingsRepository;
  std::unique_ptr<VehicleRepository> m_VehicleRepository;
  std::unique_ptr<LptRepository> m_LptRepository;
  std::unique_ptr<CurrentTestAxleDataStore> m_CurrentTestAxleDataStore;
  std::unique_ptr<PrnPayloadStore> m_PrnPayloadStore;
  std::unique_ptr<SelectedVehicleStore> m_SelectedVehicleStore;
  std::unique_ptr<LptStore> m_LptStore;
  std::unique_ptr<SerialDeviceStore> m_SerialDeviceStore;
  std::unique_ptr<PrintStatusStore> m_PrintStatusStore;
  std::unique_ptr<LptManager> m_LptManager;
  std::unique_ptr<PrintManager> m_PrintManager;
  std::unique_ptr<BrakeTesterHttpServer> m_HttpServer;
  std::unique_ptr<BrakeTesterSerialDispatcher> m_BrakeTesterSerialDispatcher;
  std::thread m_SerialDeviceRefreshThread;
  std::atomic_bool m_IsSerialDeviceRefreshRunning{false};
};

} // namespace brake_tester
