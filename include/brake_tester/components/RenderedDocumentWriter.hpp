#pragma once

#include <filesystem>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class RenderedDocumentWriter final : public IRenderedDocumentWriter {
public:
  RenderedDocumentWriter(std::filesystem::path outputDirectory, SharedLogger log);

  void writePages(const std::vector<RenderedPage>& pages, const std::string& documentId) override;

private:
  std::filesystem::path m_OutputDirectory;
  SharedLogger m_Log;
};

} // namespace brake_tester
