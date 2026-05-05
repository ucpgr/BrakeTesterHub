#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "brake_tester/models.hpp"

namespace brake_tester {

class IPrnPatcher {
public:
  virtual ~IPrnPatcher() = default;
  virtual std::vector<std::uint8_t> patch(const PrnPayload& payload) = 0;
};

class IPrnValidator {
public:
  virtual ~IPrnValidator() = default;
  virtual bool verifyTemplate(const std::vector<std::uint8_t>& inputBytes) const = 0;
};

class IPrnRenderer {
public:
  virtual ~IPrnRenderer() = default;
  virtual void render(const std::filesystem::path& prnFilePath) = 0;
};

class IRenderedDocumentWriter {
public:
  virtual ~IRenderedDocumentWriter() = default;
  virtual void writePages(const std::vector<RenderedPage>& pages, const std::string& documentId) = 0;
};

class IPrnWriter {
public:
  virtual ~IPrnWriter() = default;
  virtual void writePrn(const std::vector<std::uint8_t>& patchedBytes, const std::string& filenameWithoutExtension) = 0;
};

} // namespace brake_tester
