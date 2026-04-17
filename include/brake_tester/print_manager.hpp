#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "brake_tester/components/CupsPrinterClient.hpp"
#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrintManager {
public:
  PrintManager(CupsPrinterClient printerClient,
               IPrintSettingsRepository& printSettingsRepository,
               IPrintStatusStore& printStatusStore,
               SharedLogger log);
  ~PrintManager();

  void start();
  void stop();

  std::vector<PrinterDescriptor> listPrinters() const;
  bool printPdfFile(const std::string& pdfFilePath);
  std::string getPrintStatus() const;

private:
  struct ActiveJob {
    int id{0};
    std::string printerName;
    std::chrono::steady_clock::time_point startedAt;
  };

  void monitorActiveJob();

  std::atomic_bool m_IsRunning{false};
  std::thread m_MonitorThread;

  CupsPrinterClient m_PrinterClient;
  IPrintSettingsRepository& m_PrintSettingsRepository;
  IPrintStatusStore& m_PrintStatusStore;
  SharedLogger m_Log;

  mutable std::mutex m_ActiveJobMutex;
  std::optional<ActiveJob> m_ActiveJob;
};

} // namespace brake_tester
