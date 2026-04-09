#pragma once

#include <mutex>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class SelectedVehicleStore final : public ISelectedVehicleStore {
public:
  VehicleSelection getSelectedVehicle() const override;
  void setSelectedVehicle(const VehicleSelection& selectedVehicle) override;

private:
  mutable std::mutex m_Mutex;
  VehicleSelection m_SelectedVehicle{};
};

} // namespace brake_tester
