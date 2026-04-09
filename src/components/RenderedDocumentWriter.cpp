#include "brake_tester/components/RenderedDocumentWriter.hpp"

#include <fstream>
#include <sstream>

namespace brake_tester {

RenderedDocumentWriter::RenderedDocumentWriter(std::filesystem::path outputDirectory, SharedLogger log)
    : m_OutputDirectory(std::move(outputDirectory)), m_Log(std::move(log)) {}

void RenderedDocumentWriter::writePages(const std::vector<RenderedPage>& pages, const std::string& documentId) {
  for (const auto& renderedPage : pages) {
    std::ostringstream relativePathStream;
    relativePathStream << documentId << "_" << renderedPage.pageIndex << ".bin";
    const std::filesystem::path fullPath = m_OutputDirectory / relativePathStream.str();
    std::filesystem::create_directories(fullPath.parent_path());

    std::ofstream outputStream(fullPath, std::ios::binary);
    outputStream.write(reinterpret_cast<const char*>(renderedPage.pixels.data()),
                       static_cast<std::streamsize>(renderedPage.pixels.size()));
  }
}

} // namespace brake_tester
