#pragma once

#include <memory>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager;
class PrintManager;

class BrakeTesterHttpServer {
public:
  BrakeTesterHttpServer(ILptStore& lptStore,
                        ISettingsRepository& settingsRepository,
                        IPrintSettingsRepository& printSettingsRepository,
                        ISerialDeviceStore& serialDeviceStore,
                        IPrintStatusStore& printStatusStore,
                        IVehicleRepository& vehicleRepository,
                        ISelectedVehicleStore& selectedVehicleStore,
                        LptManager& lptManager,
                        PrintManager& printManager,
                        SharedLogger log,
                        std::string host = "0.0.0.0",
                        int port = 80,
                        std::string staticRoot = "www");
  ~BrakeTesterHttpServer();

  void start();
  void stop();

private:
  struct Impl;

  void configureLptModule();
  void configureVehicleModule();
  void configureStatusModule();
  void configureSettingsModule();
  void startLptBroadcastLoop();
  void startSettingsBroadcastLoop();
  void startPrintStatusBroadcastLoop();
  void broadcastVehicleState();
  void broadcastSettingsState();
  std::string buildVehicleStatePayloadText() const;
  std::string buildSettingsStatePayloadText() const;
  std::string lptEventNameForStatus(LptProcessStatus status) const;
  void broadcastStatus(const std::string& level, const std::string& text);

  ILptStore& m_LptStore;
  ISettingsRepository& m_SettingsRepository;
  IPrintSettingsRepository& m_PrintSettingsRepository;
  ISerialDeviceStore& m_SerialDeviceStore;
  IPrintStatusStore& m_PrintStatusStore;
  IVehicleRepository& m_VehicleRepository;
  ISelectedVehicleStore& m_SelectedVehicleStore;
  LptManager& m_LptManager;
  PrintManager& m_PrintManager;
  SharedLogger m_Log;
  std::string m_Host;
  int m_Port;
  std::string m_StaticRoot;
  std::unique_ptr<Impl> m_Impl;
};

} // namespace brake_tester
