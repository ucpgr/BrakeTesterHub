#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrnRenderer final : public IPrnRenderer {
public:
  explicit PrnRenderer(SharedLogger log);

  void render(const std::filesystem::path& prnFilePath) override;

private:
  void runCommand(const std::string& command, const std::string& failureMessage) const;
  static std::string shellQuote(const std::string& value);
  static std::vector<std::filesystem::path> collectGeneratedPages(const std::filesystem::path& pdfFolder);
  static void cleanupPdfFolder(const std::filesystem::path& pdfFolder);

  SharedLogger m_Log;
};

} // namespace brake_tester
