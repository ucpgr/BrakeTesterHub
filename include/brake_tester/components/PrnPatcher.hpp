#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class PrnPatcher final : public IPrnPatcher {
public:
  using PatchGenerator = std::function<std::string(const VehicleSelection&)>;

  PrnPatcher(const ISelectedVehicleStore& selectedVehicleStore, SharedLogger log);

  void addPatch(std::size_t patchOffset, PatchGenerator patchGenerator);
  std::vector<std::uint8_t> patch(const PrnPayload& payload) override;

private:
  const ISelectedVehicleStore& m_SelectedVehicleStore;
  std::vector<std::pair<std::size_t, PatchGenerator>> m_Patches;
  SharedLogger m_Log;
};

} // namespace brake_tester
