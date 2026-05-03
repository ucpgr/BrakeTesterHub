#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrintManager;

class LptManager {
public:
  LptManager(std::unique_ptr<ILptListener> listener,
             std::unique_ptr<IPrnPatcher> patcher,
             std::unique_ptr<IPrnValidator> prnValidator,
             std::unique_ptr<IPrnRenderer> renderer,
             std::unique_ptr<IPrnWriter> prnWriter,
             ILptRepository& lptRepository,
             ICurrentTestAxleDataStore& currentTestAxleDataStore,
             IPrnPayloadStore& prnPayloadStore,
             ISelectedVehicleStore& selectedVehicleStore,
             ILptStore& lptStore,
             const ISettingsRepository& settingsRepository,
             IPrintSettingsRepository& printSettingsRepository,
             PrintManager& printManager,
             SharedLogger log);
  ~LptManager();

  void start();
  void stop();
  void sendTestSignal(bool enableTestFlag);
  bool ingestPrnPayload(const std::vector<uint8_t>& incomingBytes,
                        PrnPayloadSource source = PrnPayloadSource::LptListener,
                        const std::optional<std::string>& preferredFilenameWithoutExtension = std::nullopt,
                        const std::optional<std::string>& preservedCreatedAtUtc = std::nullopt);

private:
  bool processCapturedPayload(const PrnPayload& payload);
  void monitorSelectedVehicleTimeout();
  void evaluateSelectedVehicleTimeout();
  std::optional<std::string> generateThumbnailForPdf(const std::filesystem::path& pdfPath) const;
  static std::string generateCaptureFilenameWithoutExtension();
  static std::string randomSuffix();

  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;
  std::thread m_QueuedPayloadThread;
  std::thread m_SelectedVehicleWatchdogThread;

  std::unique_ptr<ILptListener> m_Listener;
  std::unique_ptr<IPrnPatcher> m_Patcher;
  std::unique_ptr<IPrnValidator> m_PrnValidator;
  std::unique_ptr<IPrnRenderer> m_Renderer;
  std::unique_ptr<IPrnWriter> m_PrnWriter;
  ILptRepository& m_LptRepository;
  ICurrentTestAxleDataStore& m_CurrentTestAxleDataStore;
  IPrnPayloadStore& m_PrnPayloadStore;
  ISelectedVehicleStore& m_SelectedVehicleStore;
  ILptStore& m_LptStore;
  const ISettingsRepository& m_SettingsRepository;
  IPrintSettingsRepository& m_PrintSettingsRepository;
  PrintManager& m_PrintManager;
  SharedLogger m_Log;
  std::optional<std::chrono::steady_clock::time_point> m_SelectedVehicleUnassignDeadline;
  std::string m_SelectedVehicleDeadlineReg;
  mutable std::mutex m_SelectedVehicleUnassignMutex;
};

} // namespace brake_tester
