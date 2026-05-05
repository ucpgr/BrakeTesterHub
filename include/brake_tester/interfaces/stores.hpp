#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "brake_tester/models.hpp"

namespace brake_tester {

class ISerialDeviceStore {
public:
  virtual ~ISerialDeviceStore() = default;
  virtual std::vector<std::string> getDevices() const = 0;
  virtual void setDevices(std::vector<std::string> devices) = 0;
  virtual std::uint64_t getVersion() const = 0;
  virtual bool waitForVersionAfter(std::uint64_t afterVersion,
                                   std::chrono::milliseconds timeout,
                                   std::vector<std::string>& devices,
                                   std::uint64_t& version) const = 0;
};

class IPrintStatusStore {
public:
  virtual ~IPrintStatusStore() = default;
  virtual std::string getStatus() const = 0;
  virtual void setStatus(std::string status) = 0;
  virtual std::uint64_t getVersion() const = 0;
  virtual bool waitForVersionAfter(std::uint64_t afterVersion,
                                   std::chrono::milliseconds timeout,
                                   std::string& status,
                                   std::uint64_t& version) const = 0;
};

class ISelectedVehicleStore {
public:
  virtual ~ISelectedVehicleStore() = default;
  virtual VehicleSelection getSelectedVehicle() const = 0;
  virtual void setSelectedVehicle(const VehicleSelection& selectedVehicle) = 0;
};

class ICurrentTestAxleDataStore {
public:
  virtual ~ICurrentTestAxleDataStore() = default;
  virtual std::vector<HistoricalAxleResult> getAxleResults() const = 0;
  virtual void setAxleResults(const std::vector<HistoricalAxleResult>& axleResults) = 0;
  virtual bool isEmpty() const = 0;
  virtual void clear() = 0;
};

class IPrnPayloadStore {
public:
  virtual ~IPrnPayloadStore() = default;
  virtual void enqueue(PrnPayload payload) = 0;
  virtual bool tryDequeue(PrnPayload& payload) = 0;
};

class ILptStore {
public:
  virtual ~ILptStore() = default;
  virtual LptListenerStatus getListenerStatus() const = 0;
  virtual void setListenerStatus(LptListenerStatus status) = 0;
  virtual std::string getCurrentCaptureFilename() const = 0;
  virtual void setCurrentCaptureFilename(std::string filename) = 0;

  virtual LptProcessStatus getProcessStatus() const = 0;
  virtual void setProcessStatus(LptProcessStatus status) = 0;
  virtual bool isLptTestEnabled() const = 0;
  virtual void setLptTestEnabled(bool enabled) = 0;
  virtual bool consumeLptSerialDeviceChanged() = 0;
  virtual void setLptSerialDeviceChanged(bool changed) = 0;
  virtual bool consumeBrakeTesterSerialDeviceChanged() = 0;
  virtual void setBrakeTesterSerialDeviceChanged(bool changed) = 0;
  virtual std::uint64_t getProcessStatusVersion() const = 0;
  virtual bool waitForProcessStatusAfter(std::uint64_t afterVersion,
                                         std::chrono::milliseconds timeout,
                                         LptProcessStatus& status,
                                         std::uint64_t& version) const = 0;
};

} // namespace brake_tester
