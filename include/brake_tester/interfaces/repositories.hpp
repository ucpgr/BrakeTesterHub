#pragma once

#include <optional>
#include <string>
#include <vector>

#include "brake_tester/models.hpp"

namespace brake_tester {

class IPrintSettingsRepository {
public:
  virtual ~IPrintSettingsRepository() = default;
  virtual PrintSettings getPrintSettings() const = 0;
  virtual void setPrintSettings(const PrintSettings& printSettings) = 0;
};

class ISettingsRepository {
public:
  virtual ~ISettingsRepository() = default;
  virtual SerialSettings getSerialSettings() const = 0;
  virtual void setSerialSettings(const SerialSettings& serialSettings) = 0;
  virtual int getVehicleUnassignMinutes() const = 0;
  virtual void setVehicleUnassignMinutes(int minutes) = 0;
};

class ILptRepository {
public:
  virtual ~ILptRepository() = default;
  virtual int createTest(const HistoricalTest& test, const std::vector<HistoricalAxleResult>& axleResults) = 0;
  virtual HistoricalPage getTests(const HistoricalTestQuery& query) const = 0;
  virtual bool tryGetTestDetails(int testId, HistoricalTestDetails& details) const = 0;
  virtual bool deleteTest(int testId) = 0;
  virtual int getResultsPerPagePreference() const = 0;
  virtual void setResultsPerPagePreference(int value) = 0;
};

class IVehicleRepository {
public:
  virtual ~IVehicleRepository() = default;
  virtual std::vector<VehicleSelection> getVehicles() const = 0;
  virtual VehicleSelection addVehicle(const VehicleSelection& vehicle) = 0;
  virtual bool deleteVehicle(int id) = 0;
  virtual bool tryGetVehicle(int id, VehicleSelection& vehicle) const = 0;
  virtual bool updateVehicleMileage(int id, const std::optional<std::string>& mileage) = 0;
};

} // namespace brake_tester
