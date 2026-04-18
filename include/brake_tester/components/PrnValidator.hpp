#pragma once

#include <cstddef>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrnValidator final : public IPrnValidator {
public:
  explicit PrnValidator(SharedLogger log);

  bool verifyTemplate(const std::vector<std::uint8_t>& inputBytes) const override;

private:
  struct VerifyEntry {
    const char* label;
    std::size_t offset;
  };

  static constexpr VerifyEntry kVerifyTemplate[] = {
      {"Lic.Plate:", 10041},
      {"Make", 10121},
      {"Model", 10201},
      {"Km", 10281},
      {"Test Date", 10165},
      {"Test Time", 10245},
  };

  SharedLogger m_Log;
};

} // namespace brake_tester
