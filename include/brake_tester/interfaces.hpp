#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
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
  virtual void enqueue(std::vector<std::uint8_t> payload) = 0;
  virtual bool tryDequeue(std::vector<std::uint8_t>& payload) = 0;
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
