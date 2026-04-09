#pragma once

#include <atomic>
#include <memory>
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
             ILptStore& lptStore,
             const ISettingsRepository& settingsRepository,
             SharedLogger log);
  ~LptManager();

  void start();
  void stop();
  void sendTestSignal();

private:
  static std::string generateCaptureFilenameWithoutExtension();
  static std::string randomSuffix();

  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;

  std::unique_ptr<ILptListener> m_Listener;
  std::unique_ptr<IPrnPatcher> m_Patcher;
  std::unique_ptr<IPrnRenderer> m_Renderer;
  std::unique_ptr<IPrnWriter> m_PrnWriter;
  ILptStore& m_LptStore;
  const ISettingsRepository& m_SettingsRepository;
  SharedLogger m_Log;
};

} // namespace brake_tester
