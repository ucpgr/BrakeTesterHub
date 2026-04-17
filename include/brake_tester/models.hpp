#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace brake_tester {

struct SerialSettings {
  std::string lptDevicePath{"/dev/ttyS0"};
  std::string brakeTesterDevicePath{"/dev/ttyS1"};
  std::uint32_t baudRate{9600};
  std::chrono::milliseconds silenceTimeout{250};
  std::size_t readChunkSize{256};
};


struct PrintSettings {
  std::string selectedPrinter;
  bool autoPrint{false};
};

struct VehicleSelection {
  int id{0};
  std::string reg;
  std::string make;
  std::string model;
  std::optional<std::string> mileage;
};

enum class TestOutcome {
  Unknown,
  Pass,
  Fail
};

struct HistoricalVehicle {
  int id{0};
  std::string reg;
  std::optional<std::string> make;
  std::optional<std::string> model;
  std::optional<std::string> mileage;
};

struct HistoricalAxleResult {
  int id{0};
  int testId{0};
  int axleIndex{0};
  std::string testType;
  std::optional<int> leftBrakeForce;
  std::optional<int> rightBrakeForce;
  std::optional<double> efficiency;
  std::optional<double> imbalance;
  std::optional<int> weight;
};

struct HistoricalTest {
  int id{0};
  std::string createdAtUtc;
  std::optional<std::string> prnFile;
  std::string pdfFile;
  std::optional<std::string> thumbnailFile;
  TestOutcome outcome{TestOutcome::Unknown};
  std::optional<HistoricalVehicle> vehicle;
};

struct HistoricalTestDetails {
  HistoricalTest test;
  std::vector<HistoricalAxleResult> axleResults;
};

struct HistoricalTestQuery {
  std::optional<int> year;
  std::optional<int> month;
  std::optional<std::string> vehicleReg;
  int page{1};
  int perPage{20};
};

struct HistoricalMonthOption {
  int month{0};
  std::string label;
};

struct HistoricalFilterOptions {
  std::vector<int> years;
  std::vector<HistoricalMonthOption> months;
  std::vector<std::string> vehicleRegistrations;
};

struct HistoricalPage {
  std::vector<HistoricalTest> tests;
  int page{1};
  int perPage{20};
  int totalCount{0};
  HistoricalFilterOptions filterOptions;
};

struct RenderedPage {
  std::size_t pageIndex{0};
  std::vector<std::uint8_t> pixels;
  std::size_t width{0};
  std::size_t height{0};
};

enum class LptListenerStatus {
  Idle,
  CaptureStarted
};

enum class LptProcessStatus {
  Idle,
  TransferStarted,
  DataPatched,
  ConversionStarted,
  ConversionFinished,
  ThumbnailGenerated
};

} // namespace brake_tester
