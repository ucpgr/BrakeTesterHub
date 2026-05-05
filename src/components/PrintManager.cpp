#include "brake_tester/print_manager.hpp"

#include <filesystem>

namespace brake_tester {

PrintManager::PrintManager(CupsPrinterClient printerClient,
                           IPrintSettingsRepository& printSettingsRepository,
                           IPrintStatusStore& printStatusStore,
                           SharedLogger log)
    : m_PrinterClient(std::move(printerClient)),
      m_PrintSettingsRepository(printSettingsRepository),
      m_PrintStatusStore(printStatusStore),
      m_Log(std::move(log)) {
  if (m_Log) {
    m_Log->information("[PrintManager Info]: Constructed print manager.");
  }

  m_PrintStatusStore.setStatus("Idle");
}

PrintManager::~PrintManager() {
  stop();
}

void PrintManager::start() {
  if (m_IsRunning.exchange(true)) {
    if (m_Log) {
      m_Log->warning("[PrintManager Warning]: Start requested while already running.");
    }
    return;
  }

  if (m_Log) {
    m_Log->information("[PrintManager Info]: Starting print monitor thread.");
  }

  m_MonitorThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[PrintManager Info]: Print monitor thread started.");
    }

    while (m_IsRunning.load()) {
      monitorActiveJob();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (m_Log) {
      m_Log->information("[PrintManager Info]: Print monitor thread exiting.");
    }
  });
}

void PrintManager::stop() {
  if (!m_IsRunning.exchange(false)) {
    if (m_Log) {
      m_Log->warning("[PrintManager Warning]: Stop requested while print manager not running.");
    }
    return;
  }

  if (m_Log) {
    m_Log->information("[PrintManager Info]: Stopping print monitor thread.");
  }

  if (m_MonitorThread.joinable()) {
    m_MonitorThread.join();
  }
}

std::vector<PrinterDescriptor> PrintManager::listPrinters() const {
  return m_PrinterClient.listPrinters();
}

bool PrintManager::printPdfFile(const std::string& pdfFilePath) {
  if (!std::filesystem::exists(pdfFilePath)) {
    if (m_Log) {
      m_Log->error("[PrintManager Error]: Requested PDF does not exist: " + pdfFilePath);
    }
    m_PrintStatusStore.setStatus("Print failed.");
    return false;
  }

  const PrintSettings printSettings = m_PrintSettingsRepository.getPrintSettings();
  if (printSettings.selectedPrinter.empty()) {
    if (m_Log) {
      m_Log->error("[PrintManager Error]: Cannot print because no printer is selected in print settings.");
    }
    m_PrintStatusStore.setStatus("Print failed.");
    return false;
  }

  const auto jobId = m_PrinterClient.printPdfFile(printSettings.selectedPrinter, pdfFilePath);
  if (!jobId.has_value()) {
    m_PrintStatusStore.setStatus("Print failed.");
    return false;
  }

  {
    std::scoped_lock lock(m_ActiveJobMutex);
    m_ActiveJob = ActiveJob{*jobId, printSettings.selectedPrinter, std::chrono::steady_clock::now()};
  }

  m_PrintStatusStore.setStatus("Printing...");
  if (m_Log) {
    m_Log->information("[PrintManager Info]: Active print job id=" + std::to_string(*jobId) +
                       " started for printer='" + printSettings.selectedPrinter + "'.");
  }
  return true;
}

std::string PrintManager::getPrintStatus() const {
  return m_PrintStatusStore.getStatus();
}

void PrintManager::monitorActiveJob() {
  std::optional<ActiveJob> activeJob;
  {
    std::scoped_lock lock(m_ActiveJobMutex);
    activeJob = m_ActiveJob;
  }

  if (!activeJob.has_value()) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - activeJob->startedAt);
  if (elapsed > std::chrono::minutes(3)) {
    if (m_Log) {
      m_Log->warning("[PrintManager Warning]: Print job id=" + std::to_string(activeJob->id) +
                     " exceeded timeout. Attempting cancel.");
    }
    m_PrinterClient.cancelJob(activeJob->printerName, activeJob->id);
    m_PrintStatusStore.setStatus("Print failed.");

    std::scoped_lock lock(m_ActiveJobMutex);
    m_ActiveJob.reset();
    return;
  }

  const std::string jobStatus = m_PrinterClient.getJobStatus(activeJob->printerName, activeJob->id);
  if (jobStatus == "Completed") {
    m_PrintStatusStore.setStatus("Print complete.");
    std::scoped_lock lock(m_ActiveJobMutex);
    m_ActiveJob.reset();
  } else if (jobStatus == "Aborted" || jobStatus == "Canceled") {
    m_PrintStatusStore.setStatus("Print failed.");
    std::scoped_lock lock(m_ActiveJobMutex);
    m_ActiveJob.reset();
  } else {
    m_PrintStatusStore.setStatus("Printing...");
  }
}

} // namespace brake_tester
