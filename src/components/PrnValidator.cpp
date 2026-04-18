#include "brake_tester/components/PrnValidator.hpp"

#include <cstring>
#include <string>

namespace brake_tester {

PrnValidator::PrnValidator(SharedLogger log) : m_Log(std::move(log)) {
  if (m_Log) {
    m_Log->information("[PrnValidator Info]: Initialized.");
  }
}

bool PrnValidator::verifyTemplate(const std::vector<std::uint8_t>& inputBytes) const {
  for (const auto& entry : kVerifyTemplate) {
    const std::size_t labelLength = std::strlen(entry.label);
    if (inputBytes.size() < (entry.offset + labelLength)) {
      if (m_Log) {
        m_Log->warning("[PrnValidator Warning]: PRN verification failed: buffer too small for label '" +
                       std::string(entry.label) + "' at offset " + std::to_string(entry.offset) + ".");
      }
      return false;
    }

    const std::string actual(inputBytes.begin() + static_cast<std::ptrdiff_t>(entry.offset),
                             inputBytes.begin() + static_cast<std::ptrdiff_t>(entry.offset + labelLength));
    if (actual != entry.label) {
      if (m_Log) {
        m_Log->warning("[PrnValidator Warning]: PRN verification failed: expected '" + std::string(entry.label) +
                       "' at offset " + std::to_string(entry.offset) + ", got '" + actual +
                       "'. Dropping payload.");
      }
      return false;
    }
  }

  return true;
}

} // namespace brake_tester
