#include "brake_tester/components/PrnPatcher.hpp"

#include <algorithm>

namespace brake_tester {

PrnPatcher::PrnPatcher(const ISelectedVehicleStore& selectedVehicleStore, SharedLogger log)
    : m_SelectedVehicleStore(selectedVehicleStore), m_Log(std::move(log)) {
  if (m_Log) {
    m_Log->information("[PrnPatcher Info]: Initialized.");
  }
}

void PrnPatcher::addPatch(std::size_t patchOffset, PatchGenerator patchGenerator) {
  m_Patches.emplace_back(patchOffset, std::move(patchGenerator));
  if (m_Log) {
    m_Log->information("[PrnPatcher Info]: Registered patch at offset " + std::to_string(patchOffset) + ".");
  }
}

std::vector<std::uint8_t> PrnPatcher::patch(const std::vector<std::uint8_t>& inputBytes) {
  if (m_Log) {
    m_Log->information("[PrnPatcher Info]: Applying " + std::to_string(m_Patches.size()) +
                       " patch(es) to " + std::to_string(inputBytes.size()) + " bytes.");
  }
  std::vector<std::uint8_t> patchedOutputBytes = inputBytes;
  const VehicleSelection selectedVehicle = m_SelectedVehicleStore.getSelectedVehicle();

  for (const auto& [patchOffset, patchGenerator] : m_Patches) {
    const std::string replacementText = patchGenerator(selectedVehicle);
    if (patchOffset >= patchedOutputBytes.size()) {
      continue;
    }

    const auto replacementByteCount = std::min(replacementText.size(), patchedOutputBytes.size() - patchOffset);
    for (std::size_t byteIndex = 0; byteIndex < replacementByteCount; ++byteIndex) {
      patchedOutputBytes[patchOffset + byteIndex] = static_cast<std::uint8_t>(replacementText[byteIndex]);
    }
  }

  return patchedOutputBytes;
}

} // namespace brake_tester
