#include "brake_tester/components/CupsPrinterClient.hpp"

#include <cups/cups.h>

#include <utility>

namespace brake_tester {

struct CupsPrinterClient::Impl {
  explicit Impl(SharedLogger inputLog) : log(std::move(inputLog)) {
    if (log) {
      log->information("[CupsPrinterClient Info]: Constructed libcups wrapper.");
    }
  }

  SharedLogger log;
};

CupsPrinterClient::CupsPrinterClient(SharedLogger log) : m_Impl(std::make_unique<Impl>(std::move(log))) {}

CupsPrinterClient::~CupsPrinterClient() = default;

CupsPrinterClient::CupsPrinterClient(CupsPrinterClient&&) noexcept = default;

CupsPrinterClient& CupsPrinterClient::operator=(CupsPrinterClient&&) noexcept = default;

std::vector<PrinterDescriptor> CupsPrinterClient::listPrinters() const {
  cups_dest_t* destinations = nullptr;
  const int destinationCount = cupsGetDests(&destinations);

  std::vector<PrinterDescriptor> printers;
  printers.reserve(destinationCount > 0 ? static_cast<std::size_t>(destinationCount) : 0U);

  for (int index = 0; index < destinationCount; ++index) {
    const cups_dest_t& destination = destinations[index];
    PrinterDescriptor descriptor;
    descriptor.name = destination.name != nullptr ? destination.name : "";

    const char* infoText = cupsGetOption("printer-info", destination.num_options, destination.options);
    descriptor.info = infoText != nullptr ? infoText : "";

    if (!descriptor.name.empty()) {
      printers.push_back(std::move(descriptor));
    }
  }

  cupsFreeDests(destinationCount, destinations);

  if (m_Impl->log) {
    m_Impl->log->information("[CupsPrinterClient Info]: listPrinters resolved " + std::to_string(printers.size()) +
                             " printer(s).");
  }

  return printers;
}

std::optional<int> CupsPrinterClient::printPdfFile(const std::string& printerName, const std::string& pdfFilePath) const {
  if (printerName.empty() || pdfFilePath.empty()) {
    if (m_Impl->log) {
      m_Impl->log->Error("[CupsPrinterClient Error]: printPdfFile rejected due to empty printer name or pdf path.");
    }
    return std::nullopt;
  }

  const int jobId = cupsPrintFile(printerName.c_str(), pdfFilePath.c_str(), "BrakeTesterHub Job", 0, nullptr);
  if (jobId <= 0) {
    if (m_Impl->log) {
      m_Impl->log->Error("[CupsPrinterClient Error]: cupsPrintFile failed for printer='" + printerName +
                         "', file='" + pdfFilePath + "'.");
    }
    return std::nullopt;
  }

  if (m_Impl->log) {
    m_Impl->log->information("[CupsPrinterClient Info]: Submitted print job id=" + std::to_string(jobId) +
                             " to printer='" + printerName + "'.");
  }

  return jobId;
}

std::string CupsPrinterClient::getJobStatus(const std::string& printerName, int jobId) const {
  cups_job_t* jobs = nullptr;
  const int jobCount = cupsGetJobs(&jobs, printerName.empty() ? nullptr : printerName.c_str(), 0, CUPS_WHICHJOBS_ALL);

  std::string statusText = "Unknown";

  for (int index = 0; index < jobCount; ++index) {
    const cups_job_t& currentJob = jobs[index];
    if (currentJob.id != jobId) {
      continue;
    }

    switch (currentJob.state) {
      case IPP_JSTATE_PENDING: statusText = "Pending"; break;
      case IPP_JSTATE_HELD: statusText = "Held"; break;
      case IPP_JSTATE_PROCESSING: statusText = "Processing"; break;
      case IPP_JSTATE_STOPPED: statusText = "Stopped"; break;
      case IPP_JSTATE_CANCELED: statusText = "Canceled"; break;
      case IPP_JSTATE_ABORTED: statusText = "Aborted"; break;
      case IPP_JSTATE_COMPLETED: statusText = "Completed"; break;
      default: statusText = "Unknown"; break;
    }
    break;
  }

  cupsFreeJobs(jobCount, jobs);

  if (m_Impl->log) {
    m_Impl->log->information("[CupsPrinterClient Info]: Job status lookup for id=" + std::to_string(jobId) +
                             " on printer='" + printerName + "' -> " + statusText);
  }

  return statusText;
}

bool CupsPrinterClient::cancelJob(const std::string& printerName, int jobId) const {
  const int result = cupsCancelJob2(CUPS_HTTP_DEFAULT,
                                    printerName.empty() ? nullptr : printerName.c_str(),
                                    jobId,
                                    0);

  if (result <= IPP_STATUS_OK_CONFLICTING) {
    if (m_Impl->log) {
      m_Impl->log->information("[CupsPrinterClient Info]: Canceled job id=" + std::to_string(jobId) + " successfully.");
    }
    return true;
  }

  if (m_Impl->log) {
    m_Impl->log->Error("[CupsPrinterClient Error]: Failed to cancel job id=" + std::to_string(jobId) +
                       " on printer='" + printerName + "'.");
  }
  return false;
}

} // namespace brake_tester
