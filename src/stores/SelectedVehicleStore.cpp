#include "brake_tester/stores/SelectedVehicleStore.hpp"

namespace brake_tester {

VehicleSelection SelectedVehicleStore::getSelectedVehicle() const {
  std::scoped_lock lock(m_Mutex);
  return m_SelectedVehicle;
}

void SelectedVehicleStore::setSelectedVehicle(const VehicleSelection& selectedVehicle) {
  std::scoped_lock lock(m_Mutex);
  m_SelectedVehicle = selectedVehicle;
}

} // namespace brake_tester
