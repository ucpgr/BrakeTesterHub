#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/print_manager.hpp"
#include "brake_tester/prnAnalyzer.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace brake_tester {
namespace {
std::string currentUtcIsoDateTime() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

  std::tm utcTime{};
#ifdef _WIN32
  gmtime_s(&utcTime, &nowTime);
#else
  gmtime_r(&nowTime, &utcTime);
#endif

  std::ostringstream output;
  output << std::put_time(&utcTime, "%Y-%m-%d %H:%M:%S");
  return output.str();
}

std::vector<HistoricalAxleResult> parseAxleResultsFromPrnBytes(const std::vector<uint8_t>& prnBytes) {
  std::vector<HistoricalAxleResult> axleResults;
  if (prnBytes.empty()) {
    return axleResults;
  }

  const std::string prnText(prnBytes.begin(), prnBytes.end());
  prnAnalyzer<std::string::const_iterator> analyzer(prnText.cbegin(), prnText.cend());
  while (true) {
    const auto nextResult = analyzer.findNextAxleResult();
    if (!nextResult.has_value()) {
      break;
    }

    HistoricalAxleResult historicalAxle{};
    historicalAxle.axleIndex = static_cast<int>(nextResult->axleId);
    historicalAxle.testType = nextResult->type == AxleResult::ResultType::Service ? "service" : "hand_brake";
    historicalAxle.leftBrakeForce = static_cast<int>(nextResult->brakeForce.first);
    historicalAxle.rightBrakeForce = static_cast<int>(nextResult->brakeForce.second);
    historicalAxle.imbalance = static_cast<double>(nextResult->imbalancePct);
    historicalAxle.weight = static_cast<int>(nextResult->weightKgs);
    axleResults.push_back(std::move(historicalAxle));
  }

  return axleResults;
}
} // namespace

LptManager::LptManager(std::unique_ptr<ILptListener> listener,
                       std::unique_ptr<IPrnPatcher> patcher,
                       std::unique_ptr<IPrnValidator> prnValidator,
                       std::unique_ptr<IPrnRenderer> renderer,
                       std::unique_ptr<IPrnWriter> prnWriter,
                       ILptRepository& lptRepository,
                       ICurrentTestAxleDataStore& currentTestAxleDataStore,
                       ISelectedVehicleStore& selectedVehicleStore,
                       ILptStore& lptStore,
                       const ISettingsRepository& settingsRepository,
                       IPrintSettingsRepository& printSettingsRepository,
                       PrintManager& printManager,
                       SharedLogger log)
    : m_Listener(std::move(listener)),
      m_Patcher(std::move(patcher)),
      m_PrnValidator(std::move(prnValidator)),
      m_Renderer(std::move(renderer)),
      m_PrnWriter(std::move(prnWriter)),
      m_LptRepository(lptRepository),
      m_CurrentTestAxleDataStore(currentTestAxleDataStore),
      m_SelectedVehicleStore(selectedVehicleStore),
      m_LptStore(lptStore),
      m_SettingsRepository(settingsRepository),
      m_PrintSettingsRepository(printSettingsRepository),
      m_PrintManager(printManager),
      m_Log(std::move(log)) {
  if (!m_PrnValidator) {
    throw std::invalid_argument("LptManager requires a valid PrnValidator");
  }
  if (m_Log) {
    m_Log->information("[LptManager Info]: Constructed.");
  }
}

LptManager::~LptManager() {
  stop();
}

void LptManager::start() {
  if (m_IsRunning.exchange(true)) {
    if (m_Log) {
      m_Log->warning("[LptManager Warning]: Start requested while already running.");
    }
    return;
  }
  if (m_Log) {
    const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
    m_Log->information("[LptManager Info]: Starting worker thread.");
    m_Log->information("[LptManager Info]: Serial settings -> lptDevicePath=" + serialSettings.lptDevicePath +
                       ", brakeTesterDevicePath=" + serialSettings.brakeTesterDevicePath +
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
        if (m_Log) {
          m_Log->information("[LptManager Info]: Data transfer started. Captured bytes: " +
                             std::to_string(incomingBytes.size()));
        }

        processCapturedPayload(incomingBytes);
      } catch (const std::exception& processingException) {
        if (m_Log) {
          m_Log->Error(processingException.what());
          m_Log->information("[LptManager Info]: Retrying in 10 seconds.");
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
  m_SelectedVehicleWatchdogThread = std::thread([this] { monitorSelectedVehicleTimeout(); });
}

bool LptManager::ingestPrnPayload(const std::vector<uint8_t>& incomingBytes) {
  try {
    return processCapturedPayload(incomingBytes);
  } catch (const std::exception& processingException) {
    if (m_Log) {
      m_Log->Error(std::string("[LptManager Error]: Failed to ingest PRN payload. ") + processingException.what());
    }
    return false;
  }
}

bool LptManager::processCapturedPayload(const std::vector<uint8_t>& incomingBytes) {
  if (!m_PrnValidator || !m_PrnValidator->verifyTemplate(incomingBytes)) {
    return false;
  }

  m_LptStore.setProcessStatus(LptProcessStatus::TransferStarted);
  const auto captureFilename = generateCaptureFilenameWithoutExtension();
  m_LptStore.setCurrentCaptureFilename(captureFilename);
  if (m_Log) {
    m_Log->information("[LptManager Info]: Capture file key assigned: " + captureFilename);
  }

  auto patchedBytes = m_Patcher->patch(incomingBytes);
  m_LptStore.setLptTestEnabled(false);
  m_LptStore.setProcessStatus(LptProcessStatus::DataPatched);
  if (m_Log) {
    m_Log->information("[LptManager Info]: Data patched. Output bytes: " + std::to_string(patchedBytes.size()));
  }

  m_PrnWriter->writePrn(patchedBytes, captureFilename);

  m_LptStore.setProcessStatus(LptProcessStatus::ConversionStarted);
  if (m_Log) {
    m_Log->information("[LptManager Info]: Conversion started for: " + captureFilename + ".prn");
  }
  m_Renderer->render(std::filesystem::path(captureFilename).concat(".prn"));
  m_LptStore.setProcessStatus(LptProcessStatus::ConversionFinished);
  if (m_Log) {
    m_Log->information("[LptManager Info]: Conversion finished for: " + captureFilename + ".prn");
  }
  const VehicleSelection selectedVehicle = m_SelectedVehicleStore.getSelectedVehicle();
  m_SelectedVehicleStore.setSelectedVehicle({});
  {
    std::scoped_lock lock(m_SelectedVehicleUnassignMutex);
    m_SelectedVehicleUnassignDeadline.reset();
    m_SelectedVehicleDeadlineReg.clear();
  }
  if (m_Log) {
    m_Log->information("[LptManager Info]: Vehicle selection reset to unassigned after successful conversion.");
  }
  const auto thumbnailFilePath = generateThumbnailForPdf(std::filesystem::path(captureFilename + ".prn.pdf"));
  if (thumbnailFilePath.has_value()) {
    m_LptStore.setProcessStatus(LptProcessStatus::ThumbnailGenerated);
    if (m_Log) {
      m_Log->information("[LptManager Info]: Thumbnail generated for capture key '" + captureFilename +
                         "' at path: " + *thumbnailFilePath);
    }
  } else if (m_Log) {
    m_Log->warning("[LptManager Warning]: Thumbnail not generated for capture key '" + captureFilename + "'.");
  }

  HistoricalTest historicalTest;
  historicalTest.createdAtUtc = currentUtcIsoDateTime();
  historicalTest.prnFile = captureFilename + ".prn";
  historicalTest.pdfFile = captureFilename + ".prn.pdf";
  historicalTest.thumbnailFile = thumbnailFilePath;
  historicalTest.outcome = TestOutcome::Unknown;

  if (!selectedVehicle.reg.empty()) {
    HistoricalVehicle historicalVehicle;
    historicalVehicle.reg = selectedVehicle.reg;
    historicalVehicle.make = selectedVehicle.make.empty() ? std::nullopt : std::optional<std::string>(selectedVehicle.make);
    historicalVehicle.model = selectedVehicle.model.empty() ? std::nullopt : std::optional<std::string>(selectedVehicle.model);
    historicalVehicle.mileage = selectedVehicle.mileage;
    historicalTest.vehicle = historicalVehicle;
  }

  if (m_CurrentTestAxleDataStore.isEmpty()) {
    try {
      const std::vector<HistoricalAxleResult> parsedAxleResults = parseAxleResultsFromPrnBytes(patchedBytes);
      if (!parsedAxleResults.empty()) {
        m_CurrentTestAxleDataStore.setAxleResults(parsedAxleResults);
        if (m_Log) {
          m_Log->information("[LptManager Info]: Current axle data store was empty. Parsed " +
                             std::to_string(parsedAxleResults.size()) + " axle row(s) from PRN data.");
        }
      }
    } catch (const std::exception& analyzerException) {
      if (m_Log) {
        m_Log->warning(std::string("[LptManager Warning]: Failed to parse axle data from PRN for fallback: ") +
                       analyzerException.what());
      }
    }
  }

  const std::vector<HistoricalAxleResult> axleResultsForTest = m_CurrentTestAxleDataStore.getAxleResults();
  m_LptRepository.createTest(historicalTest, axleResultsForTest);
  m_CurrentTestAxleDataStore.clear();
  const PrintSettings printSettings = m_PrintSettingsRepository.getPrintSettings();
  if (printSettings.autoPrint) {
    const bool printStarted = m_PrintManager.printPdfFile(historicalTest.pdfFile);
    if (m_Log) {
      m_Log->information(std::string("[LptManager Info]: Auto print ") + (printStarted ? "started." : "failed to start."));
    }
  }

  return true;
}

void LptManager::monitorSelectedVehicleTimeout() {
  while (m_IsRunning) {
    evaluateSelectedVehicleTimeout();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

void LptManager::evaluateSelectedVehicleTimeout() {
  const VehicleSelection selectedVehicle = m_SelectedVehicleStore.getSelectedVehicle();
  std::optional<int> deadlineMinutesToLog;
  bool shouldUnassignSelectedVehicle = false;

  {
    std::scoped_lock lock(m_SelectedVehicleUnassignMutex);

    if (selectedVehicle.reg.empty()) {
      m_SelectedVehicleUnassignDeadline.reset();
      m_SelectedVehicleDeadlineReg.clear();
      return;
    }

    const bool selectedVehicleChanged = (m_SelectedVehicleDeadlineReg != selectedVehicle.reg);
    if (!m_SelectedVehicleUnassignDeadline.has_value() || selectedVehicleChanged) {
      const int unassignMinutes = m_SettingsRepository.getVehicleUnassignMinutes();
      m_SelectedVehicleUnassignDeadline = std::chrono::steady_clock::now() + std::chrono::minutes(unassignMinutes);
      m_SelectedVehicleDeadlineReg = selectedVehicle.reg;
      deadlineMinutesToLog = unassignMinutes;
    } else if (std::chrono::steady_clock::now() >= *m_SelectedVehicleUnassignDeadline) {
      m_SelectedVehicleUnassignDeadline.reset();
      m_SelectedVehicleDeadlineReg.clear();
      shouldUnassignSelectedVehicle = true;
    }
  }

  if (deadlineMinutesToLog.has_value() && m_Log) {
    m_Log->information("[LptManager Info]: Vehicle unassign scheduled in " + std::to_string(*deadlineMinutesToLog) +
                       " minute(s) due to active vehicle selection.");
  }

  if (shouldUnassignSelectedVehicle) {
    m_SelectedVehicleStore.setSelectedVehicle({});
    if (m_Log) {
      m_Log->information("[LptManager Info]: Vehicle selection reset to unassigned after timeout.");
    }
  }
}

void LptManager::stop() {
  if (!m_IsRunning.exchange(false)) {
    if (m_Log) {
      m_Log->warning("[LptManager Warning]: Stop requested while not running.");
    }
    return;
  }
  if (m_Log) {
    m_Log->information("[LptManager Info]: Stopping worker thread.");
  }

  if (m_WorkerThread.joinable()) {
    if (m_Log) {
      m_Log->information("[LptManager Info]: Waiting for worker thread to join.");
    }
    m_WorkerThread.join();
    if (m_Log) {
      m_Log->information("[LptManager Info]: Worker thread joined.");
    }
  } else if (m_Log) {
    m_Log->information("[LptManager Info]: Worker thread was not joinable during stop.");
  }

  if (m_SelectedVehicleWatchdogThread.joinable()) {
    m_SelectedVehicleWatchdogThread.join();
  }

  if (m_Log) {
    m_Log->information("[LptManager Info]: Stop sequence completed.");
  }
}

void LptManager::sendTestSignal(bool enableTestFlag) {
  try {
    m_LptStore.setLptTestEnabled(enableTestFlag);
    m_Listener->test();
    if (m_Log) {
      m_Log->information(std::string("[LptManager Info]: Test signal sent. testEnabled=") +
                         (enableTestFlag ? "true" : "false"));
    }
  } catch (const std::exception& testException) {
    if (m_Log) {
      m_Log->Error(std::string("[LptManager Error]: Failed to send test signal. ") + testException.what());
    }
  }
}

std::optional<std::string> LptManager::generateThumbnailForPdf(const std::filesystem::path& pdfPath) const {
  try {
    auto outputPrefixPath = pdfPath;
    outputPrefixPath.replace_extension();
    auto pngPath = pdfPath;
    pngPath.replace_extension(".png");
    if (m_Log) {
      m_Log->information("[LptManager Info]: Generating thumbnail for PDF '" + pdfPath.string() +
                         "' using pdftocairo. Output prefix: " + outputPrefixPath.string() +
                         ", expected PNG path: " + pngPath.string());
    }

    const std::string command = "pdftocairo \"" + pdfPath.string() +
                                "\" -png -singlefile -scale-to 600 \"" + outputPrefixPath.string() + "\"";
    if (m_Log) {
      m_Log->information("[LptManager Info]: Executing command: " + command);
    }

    const int exitCode = std::system(command.c_str());
    if (exitCode != 0) {
      if (m_Log) {
        m_Log->warning("[LptManager Warning]: Thumbnail command failed with exit code " + std::to_string(exitCode) +
                       " for PDF: " + pdfPath.string());
      }
      return std::nullopt;
    }

    if (!std::filesystem::exists(pngPath)) {
      if (m_Log) {
        m_Log->warning("[LptManager Warning]: Thumbnail command succeeded but file does not exist at expected path: " +
                       pngPath.string());
      }
      return std::nullopt;
    }

    if (m_Log) {
      m_Log->information("[LptManager Info]: Thumbnail file verified at expected path: " + pngPath.string());
    }
    return pngPath.string();
  } catch (const std::exception& thumbnailException) {
    if (m_Log) {
      m_Log->warning(std::string("[LptManager Warning]: Exception while generating thumbnail: ") +
                     thumbnailException.what());
    }
    return std::nullopt;
  }
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
