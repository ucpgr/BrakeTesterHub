#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <thread>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager {
public:
  LptManager(std::unique_ptr<ILptListener> listener,
             std::unique_ptr<IPrnPatcher> patcher,
             std::unique_ptr<IPrnRenderer> renderer,
             std::unique_ptr<IRenderedDocumentWriter> writer,
             std::unique_ptr<IPrnWriter> prnWriter,
             ILptStore& lptStore,
             const ISettingsRepository& settingsRepository,
             SharedLogger log)
      : m_Listener(std::move(listener)),
        m_Patcher(std::move(patcher)),
        m_Renderer(std::move(renderer)),
        m_Writer(std::move(writer)),
        m_PrnWriter(std::move(prnWriter)),
        m_LptStore(lptStore),
        m_SettingsRepository(settingsRepository),
        m_Log(std::move(log)) {}

  ~LptManager() {
    stop();
  }

  void start() {
    if (m_IsRunning.exchange(true)) {
      return;
    }
    if (m_Log) {
      const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
      m_Log->information("[LptManager Info]: Starting worker thread.");
      m_Log->information("[LptManager Info]: Serial settings -> devicePath=" + serialSettings.devicePath +
                         ", baudRate=" + std::to_string(serialSettings.baudRate) + ", readChunkSize=" +
                         std::to_string(serialSettings.readChunkSize) + ", silenceTimeoutMs=" +
                         std::to_string(serialSettings.silenceTimeout.count()));
    }

    m_WorkerThread = std::thread([this] {
      while (m_IsRunning) {
        try {
          auto incomingBytes = m_Listener->captureTransmission(m_IsRunning);
          if (incomingBytes.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
          }
          const auto captureFilename = generateCaptureFilenameWithoutExtension();
          m_LptStore.setCurrentCaptureFilename(captureFilename);

          auto patchedBytes = m_Patcher->patch(incomingBytes);
          auto renderedPages = m_Renderer->render(patchedBytes);
          m_Writer->writePages(renderedPages, "capture");
          m_PrnWriter->writePrn(patchedBytes, captureFilename);
        } catch (const std::exception& processingException) {
          if (m_Log) {
            m_Log->Error(processingException.what());
            m_Log->information("[LptManager Info]: Trying again in 10 seconds.");
          }

          constexpr auto retryWaitDuration = std::chrono::seconds(10);
          constexpr auto retryPollInterval = std::chrono::milliseconds(100);
          auto waitedDuration = std::chrono::milliseconds(0);
          while (m_IsRunning && waitedDuration < retryWaitDuration) {
            std::this_thread::sleep_for(retryPollInterval);
            waitedDuration += retryPollInterval;
          }
        }
      }
    });
  }

  void stop() {
    if (!m_IsRunning.exchange(false)) {
      return;
    }
    if (m_Log) {
      m_Log->information("[LptManager Info]: Stopping worker thread.");
    }

    if (m_WorkerThread.joinable()) {
      m_WorkerThread.join();
    }
  }

  void sendTestSignal() {
    try {
      m_Listener->test();
    } catch (const std::exception& testException) {
      if (m_Log) {
        m_Log->Error(testException.what());
      }
    }
  }

private:
  static std::string generateCaptureFilenameWithoutExtension() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    const auto millisecondsSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const auto millisecondsInSecond = static_cast<int>(millisecondsSinceEpoch % 1000);

    std::tm utcTime {};
#ifdef _WIN32
    gmtime_s(&utcTime, &nowTime);
#else
    gmtime_r(&nowTime, &utcTime);
#endif

    std::ostringstream pathStream;
    pathStream << "tests/" << std::put_time(&utcTime, "%Y/%m/");
    pathStream << std::put_time(&utcTime, "%Y%m%d%H%M%S")
               << std::setw(3) << std::setfill('0') << millisecondsInSecond
               << "_" << randomSuffix();
    return pathStream.str();
  }

  static std::string randomSuffix() {
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<unsigned int> distribution(0, 0xFFFF);
    std::ostringstream suffixStream;
    suffixStream << std::hex << std::setw(4) << std::setfill('0') << distribution(generator);
    return suffixStream.str();
  }

  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;

  std::unique_ptr<ILptListener> m_Listener;
  std::unique_ptr<IPrnPatcher> m_Patcher;
  std::unique_ptr<IPrnRenderer> m_Renderer;
  std::unique_ptr<IRenderedDocumentWriter> m_Writer;
  std::unique_ptr<IPrnWriter> m_PrnWriter;
  ILptStore& m_LptStore;
  const ISettingsRepository& m_SettingsRepository;
  SharedLogger m_Log;
};

} // namespace brake_tester
