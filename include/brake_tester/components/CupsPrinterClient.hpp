#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "brake_tester/logging.hpp"

namespace brake_tester {

struct PrinterDescriptor {
  std::string name;
  std::string info;
};

class CupsPrinterClient {
public:
  explicit CupsPrinterClient(SharedLogger log);
  ~CupsPrinterClient();

  CupsPrinterClient(const CupsPrinterClient&) = delete;
  CupsPrinterClient& operator=(const CupsPrinterClient&) = delete;

  CupsPrinterClient(CupsPrinterClient&&) noexcept;
  CupsPrinterClient& operator=(CupsPrinterClient&&) noexcept;

  std::vector<PrinterDescriptor> listPrinters() const;
  std::optional<int> printPdfFile(const std::string& printerName, const std::string& pdfFilePath) const;
  std::string getJobStatus(const std::string& printerName, int jobId) const;
  bool cancelJob(const std::string& printerName, int jobId) const;

private:
  struct Impl;
  std::unique_ptr<Impl> m_Impl;
};

} // namespace brake_tester
