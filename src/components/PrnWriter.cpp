#include "brake_tester/components/PrnWriter.hpp"

#include <fstream>

namespace brake_tester {

PrnWriter::PrnWriter(std::filesystem::path rootDirectory, SharedLogger log)
    : m_RootDirectory(std::move(rootDirectory)), m_Log(std::move(log)) {}

void PrnWriter::writePrn(const std::vector<std::uint8_t>& patchedBytes, const std::string& filenameWithoutExtension) {
  if (filenameWithoutExtension.empty()) {
    return;
  }

  std::filesystem::path relativePath(filenameWithoutExtension);
  relativePath += ".prn";
  const auto fullPath = m_RootDirectory / relativePath;
  std::filesystem::create_directories(fullPath.parent_path());

  std::ofstream outputStream(fullPath, std::ios::binary);
  outputStream.write(reinterpret_cast<const char*>(patchedBytes.data()),
                     static_cast<std::streamsize>(patchedBytes.size()));
}

} // namespace brake_tester
