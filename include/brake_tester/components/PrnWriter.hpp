#pragma once

#include <filesystem>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrnWriter final : public IPrnWriter {
public:
  PrnWriter(std::filesystem::path rootDirectory, SharedLogger log);

  void writePrn(const std::vector<std::uint8_t>& patchedBytes, const std::string& filenameWithoutExtension) override;

private:
  std::filesystem::path m_RootDirectory;
  SharedLogger m_Log;
};

} // namespace brake_tester
