#include "brake_tester/components/PrnWriter.hpp"

#include <fstream>

namespace brake_tester {

PrnWriter::PrnWriter(std::filesystem::path rootDirectory, SharedLogger log)
    : m_RootDirectory(std::move(rootDirectory)), m_Log(std::move(log)) {
  if (m_Log) {
    m_Log->information("[PrnWriter Info]: Initialized with root directory: " + m_RootDirectory.string());
  }
}

void PrnWriter::writePrn(const std::vector<std::uint8_t>& patchedBytes, const std::string& filenameWithoutExtension) {
  if (filenameWithoutExtension.empty()) {
    if (m_Log) {
      m_Log->warning("[PrnWriter Warning]: Empty filename, skipping PRN write.");
    }
    return;
  }

  std::filesystem::path relativePath(filenameWithoutExtension);
  relativePath += ".prn";
  const auto fullPath = m_RootDirectory / relativePath;
  std::filesystem::create_directories(fullPath.parent_path());

  std::ofstream outputStream(fullPath, std::ios::binary);
  outputStream.write(reinterpret_cast<const char*>(patchedBytes.data()),
                     static_cast<std::streamsize>(patchedBytes.size()));
  if (m_Log) {
    m_Log->information("[PrnWriter Info]: Wrote PRN file: " + fullPath.string() +
                       " (" + std::to_string(patchedBytes.size()) + " bytes).");
  }
}

} // namespace brake_tester
