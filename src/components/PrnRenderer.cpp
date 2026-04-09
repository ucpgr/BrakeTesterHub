#include "brake_tester/components/PrnRenderer.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace brake_tester {

PrnRenderer::PrnRenderer(SharedLogger log) : m_Log(std::move(log)) {
  if (m_Log) {
    m_Log->information("[PrnRenderer Info]: Initialized.");
  }
}

void PrnRenderer::render(const std::filesystem::path& prnFilePath) {
  if (m_Log) {
    m_Log->information("[PrnRenderer Info]: Rendering PRN file: " + prnFilePath.string());
  }
  const std::filesystem::path pdfFolder = std::filesystem::path("tests") / "pdf";
  cleanupPdfFolder(pdfFolder);

  const std::string prnFilePathString = prnFilePath.string();
  const std::string renderCommand =
      "printerToPDF -8 -o tests/ -f /opt/font2/Epson-PC437-US.C16 " + shellQuote(prnFilePathString);
  runCommand(renderCommand, "[PrnRenderer Error]: Failed to render prn with printerToPDF.");

  const auto pagePdfPaths = collectGeneratedPages(pdfFolder);
  if (pagePdfPaths.empty()) {
    throw std::runtime_error("[PrnRenderer Error]: No page PDFs generated in tests/pdf.");
  }

  std::string mergeCommand = "pdfunite";
  for (const auto& pagePdfPath : pagePdfPaths) {
    mergeCommand += " " + shellQuote(pagePdfPath.string());
  }
  mergeCommand += " " + shellQuote(prnFilePathString + ".pdf");
  runCommand(mergeCommand, "[PrnRenderer Error]: Failed to merge rendered PDFs with pdfunite.");
  if (m_Log) {
    m_Log->information("[PrnRenderer Info]: Render complete. Output: " + prnFilePath.string() + ".pdf");
  }

  cleanupPdfFolder(pdfFolder);
}

std::string PrnRenderer::shellQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
}

std::vector<std::filesystem::path> PrnRenderer::collectGeneratedPages(const std::filesystem::path& pdfFolder) {
  if (!std::filesystem::exists(pdfFolder) || !std::filesystem::is_directory(pdfFolder)) {
    return {};
  }

  std::vector<std::filesystem::path> pdfPages;
  for (const auto& entry : std::filesystem::directory_iterator(pdfFolder)) {
    if (entry.is_regular_file() && entry.path().extension() == ".pdf") {
      pdfPages.push_back(entry.path());
    }
  }
  std::sort(pdfPages.begin(), pdfPages.end());
  return pdfPages;
}

void PrnRenderer::cleanupPdfFolder(const std::filesystem::path& pdfFolder) {
  std::error_code removalError;
  std::filesystem::remove_all(pdfFolder, removalError);
}

void PrnRenderer::runCommand(const std::string& command, const std::string& failureMessage) const {
  if (m_Log) {
    m_Log->information("[PrnRenderer Info]: Executing command: " + command);
  }
  const int commandExitCode = std::system(command.c_str());
  if (commandExitCode != 0) {
    throw std::runtime_error(failureMessage + " Exit code: " + std::to_string(commandExitCode));
  }
}

} // namespace brake_tester
