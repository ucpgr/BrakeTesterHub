#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager {
public:
  LptManager(std::unique_ptr<ILptListener> listener,
             std::unique_ptr<IPrnPatcher> patcher,
             std::unique_ptr<IPrnRenderer> renderer,
             std::unique_ptr<IPrnWriter> prnWriter,
             ILptRepository& lptRepository,
             ISelectedVehicleStore& selectedVehicleStore,
             ILptStore& lptStore,
             const ISettingsRepository& settingsRepository,
             SharedLogger log);
  ~LptManager();

  void start();
  void stop();
  void sendTestSignal(bool enableTestFlag);

private:
  std::optional<std::string> generateThumbnailForPdf(const std::filesystem::path& pdfPath) const;
  static std::string generateCaptureFilenameWithoutExtension();
  static std::string randomSuffix();

  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;

  std::unique_ptr<ILptListener> m_Listener;
  std::unique_ptr<IPrnPatcher> m_Patcher;
  std::unique_ptr<IPrnRenderer> m_Renderer;
  std::unique_ptr<IPrnWriter> m_PrnWriter;
  ILptRepository& m_LptRepository;
  ISelectedVehicleStore& m_SelectedVehicleStore;
  ILptStore& m_LptStore;
  const ISettingsRepository& m_SettingsRepository;
  SharedLogger m_Log;
};

} // namespace brake_tester
