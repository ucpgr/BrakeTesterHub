#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "brake_tester/models.hpp"

namespace brake_tester {

class ISettingsRepository {
public:
  virtual ~ISettingsRepository() = default;
  virtual SerialSettings getSerialSettings() const = 0;
  virtual void setSerialSettings(const SerialSettings& serialSettings) = 0;
};

class ILptRepository {
public:
  virtual ~ILptRepository() = default;
};

class ISelectedVehicleStore {
public:
  virtual ~ISelectedVehicleStore() = default;
  virtual VehicleSelection getSelectedVehicle() const = 0;
  virtual void setSelectedVehicle(const VehicleSelection& selectedVehicle) = 0;
};

class IVehicleRepository {
public:
  virtual ~IVehicleRepository() = default;
  virtual std::vector<VehicleSelection> getVehicles() const = 0;
  virtual VehicleSelection addVehicle(const VehicleSelection& vehicle) = 0;
  virtual bool deleteVehicle(int id) = 0;
  virtual bool tryGetVehicle(int id, VehicleSelection& vehicle) const = 0;
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
  virtual std::uint64_t getProcessStatusVersion() const = 0;
  virtual bool waitForProcessStatusAfter(std::uint64_t afterVersion,
                                         std::chrono::milliseconds timeout,
                                         LptProcessStatus& status,
                                         std::uint64_t& version) const = 0;
};

class ILptListener {
public:
  virtual ~ILptListener() = default;
  virtual std::vector<std::uint8_t> captureTransmission(const std::atomic_bool& shouldKeepRunning) = 0;
  virtual void test() = 0;
};

class IPrnPatcher {
public:
  virtual ~IPrnPatcher() = default;
  virtual std::vector<std::uint8_t> patch(const std::vector<std::uint8_t>& inputBytes) = 0;
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
