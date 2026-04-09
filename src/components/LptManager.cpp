#include "brake_tester/lpt_manager.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace brake_tester {

LptManager::LptManager(std::unique_ptr<ILptListener> listener,
                       std::unique_ptr<IPrnPatcher> patcher,
                       std::unique_ptr<IPrnRenderer> renderer,
                       std::unique_ptr<IPrnWriter> prnWriter,
                       ILptStore& lptStore,
                       const ISettingsRepository& settingsRepository,
                       SharedLogger log)
    : m_Listener(std::move(listener)),
      m_Patcher(std::move(patcher)),
      m_Renderer(std::move(renderer)),
      m_PrnWriter(std::move(prnWriter)),
      m_LptStore(lptStore),
      m_SettingsRepository(settingsRepository),
      m_Log(std::move(log)) {}

LptManager::~LptManager() {
  stop();
}

void LptManager::start() {
  if (m_IsRunning.exchange(true)) {
    return;
  }

  m_WorkerThread = std::thread([this] {
    while (m_IsRunning) {
      try {
        auto incomingBytes = m_Listener->captureTransmission(m_IsRunning);
        if (incomingBytes.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }

        m_LptStore.setProcessStatus(LptProcessStatus::TransferStarted);
        const auto captureFilename = generateCaptureFilenameWithoutExtension();
        m_LptStore.setCurrentCaptureFilename(captureFilename);

        auto patchedBytes = m_Patcher->patch(incomingBytes);
        m_LptStore.setProcessStatus(LptProcessStatus::DataPatched);

        m_PrnWriter->writePrn(patchedBytes, captureFilename);

        m_LptStore.setProcessStatus(LptProcessStatus::ConversionStarted);
        m_Renderer->render(std::filesystem::path(captureFilename).concat(".prn"));
        m_LptStore.setProcessStatus(LptProcessStatus::ConversionFinished);
      } catch (const std::exception& processingException) {
        if (m_Log) {
          m_Log->Error(processingException.what());
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

void LptManager::stop() {
  if (!m_IsRunning.exchange(false)) {
    return;
  }

  if (m_WorkerThread.joinable()) {
    m_WorkerThread.join();
  }
}

void LptManager::sendTestSignal() {
  m_Listener->test();
}

std::string LptManager::generateCaptureFilenameWithoutExtension() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  const auto millisecondsSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  const auto millisecondsInSecond = static_cast<int>(millisecondsSinceEpoch % 1000);

  std::tm utcTime{};
#ifdef _WIN32
  gmtime_s(&utcTime, &nowTime);
#else
  gmtime_r(&nowTime, &utcTime);
#endif

  std::ostringstream pathStream;
  pathStream << "tests/" << std::put_time(&utcTime, "%Y/%m/");
  pathStream << std::put_time(&utcTime, "%Y%m%d%H%M%S") << std::setw(3) << std::setfill('0') << millisecondsInSecond << "_"
             << randomSuffix();
  return pathStream.str();
}

std::string LptManager::randomSuffix() {
  thread_local std::mt19937 generator(std::random_device{}());
  std::uniform_int_distribution<unsigned int> distribution(0, 0xFFFF);
  std::ostringstream suffixStream;
  suffixStream << std::hex << std::setw(4) << std::setfill('0') << distribution(generator);
  return suffixStream.str();
}

} // namespace brake_tester
