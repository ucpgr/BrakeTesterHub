#include "brake_tester/app.hpp"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "brake_tester/components.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/print_manager.hpp"
#include "brake_tester/repositories.hpp"
#include "brake_tester/stores.hpp"
#include "brake_tester/stores/SerialDeviceStore.hpp"
#include "brake_tester/web/BrakeTesterHttpServer.hpp"

namespace brake_tester {
namespace {
std::string formatCurrentUtc(const char* formatPattern) {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm utcTime{};
#ifdef _WIN32
  gmtime_s(&utcTime, &nowTime);
#else
  gmtime_r(&nowTime, &utcTime);
#endif
  std::ostringstream textStream;
  textStream << std::put_time(&utcTime, formatPattern);
  return textStream.str();
}

std::string formatPrnField(const std::string& value, std::size_t fieldLength) {
  const std::string trimmedValue = value.substr(0, fieldLength);
  if (trimmedValue.size() >= fieldLength) {
    return trimmedValue;
  }

  return trimmedValue + std::string(fieldLength - trimmedValue.size(), ' ');
}

std::string formatMileageForPrn(const std::optional<std::string>& mileage) {
  if (!mileage.has_value() || mileage->empty()) {
    return std::string();
  }

  std::string formattedMileage = *mileage;
  const auto endsWithKm = formattedMileage.size() >= 2 &&
                          (formattedMileage.substr(formattedMileage.size() - 2) == "km" ||
                           formattedMileage.substr(formattedMileage.size() - 2) == "KM");
  if (!endsWithKm) {
    formattedMileage += "km";
  }

  return formattedMileage;
}

std::vector<std::string> scanSerialDevices() {
  std::vector<std::string> devices;
  const std::vector<std::string> prefixes = {"ttyS", "ttyUSB", "ttyACM", "ttyAMA", "rfcomm", "stty"};

  for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
    if (!entry.is_character_file() && !entry.is_symlink()) {
      continue;
    }

    const std::string filename = entry.path().filename().string();
    const bool matchesPrefix = std::any_of(prefixes.begin(), prefixes.end(), [&filename](const std::string& prefix) {
      return filename.rfind(prefix, 0) == 0;
    });
    if (!matchesPrefix) {
      continue;
    }
    devices.push_back(entry.path().string());
  }

  if (std::filesystem::exists("/dev/serial/by-id")) {
    for (const auto& entry : std::filesystem::directory_iterator("/dev/serial/by-id")) {
      devices.push_back(entry.path().string());
    }
  }

  std::sort(devices.begin(), devices.end());
  devices.erase(std::unique(devices.begin(), devices.end()), devices.end());
  return devices;
}
} // namespace

App::App(std::string databasePath) {
  m_Log = std::make_shared<Logger>(LogVerbosity::Information);
  m_Log->information("[App Info]: Initializing BrakeTesterHub application.");

  if (sqlite3_open(databasePath.c_str(), &m_DatabaseHandle) != SQLITE_OK) {
    throw std::runtime_error("Failed to open sqlite database");
  }
  m_Log->information("[App Info]: Opened sqlite database at path: " + databasePath);

  m_SettingsRepository = std::make_unique<SettingsRepository>(m_DatabaseHandle, m_Log);
  m_VehicleRepository = std::make_unique<VehicleRepository>(m_DatabaseHandle, m_Log);
  m_PrintSettingsRepository = std::make_unique<PrintSettingsRepository>(m_DatabaseHandle, m_Log);
  m_LptRepository = std::make_unique<LptRepository>(m_DatabaseHandle, m_Log);
  m_CurrentTestAxleDataStore = std::make_unique<CurrentTestAxleDataStore>();
  m_PrnPayloadStore = std::make_unique<PrnPayloadStore>();
  m_SelectedVehicleStore = std::make_unique<SelectedVehicleStore>();
  m_LptStore = std::make_unique<LptStore>();
  m_SerialDeviceStore = std::make_unique<SerialDeviceStore>();
  m_PrintStatusStore = std::make_unique<PrintStatusStore>();

  auto listener = std::make_unique<LptListener>(*m_SettingsRepository, *m_LptStore, m_Log);
  auto patcher = std::make_unique<PrnPatcher>(*m_SelectedVehicleStore, m_Log);
  auto prnValidator = std::make_unique<PrnValidator>(m_Log);
  patcher->addPatch(0x2671, [this](const VehicleSelection&) { // userLine1 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x26c5, [this](const VehicleSelection&) { // userLine2 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x2715, [this](const VehicleSelection&) { // userLine3 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });

  patcher->addPatch(0x2744, [this](const VehicleSelection& selectedVehicle) { // licence length 27
    if (m_LptStore->isLptTestEnabled()) {
      return formatPrnField("TEST", 27);
    }
    return formatPrnField(selectedVehicle.reg, 27);
  });
  patcher->addPatch(0x2794, [this](const VehicleSelection& selectedVehicle) { // make length 27
    if (m_LptStore->isLptTestEnabled()) {
      return formatPrnField("TEST", 27);
    }
    return formatPrnField(selectedVehicle.make, 27);
  });
  patcher->addPatch(0x27e4, [this](const VehicleSelection& selectedVehicle) { // model length 27
    if (m_LptStore->isLptTestEnabled()) {
      return formatPrnField("TEST", 27);
    }
    return formatPrnField(selectedVehicle.model, 27);
  });
  patcher->addPatch(0x2834, [this](const VehicleSelection& selectedVehicle) { // mileage length 27
    if (m_LptStore->isLptTestEnabled()) {
      return formatPrnField("TEST", 27);
    }
    return formatPrnField(formatMileageForPrn(selectedVehicle.mileage), 27);
  });

  patcher->addPatch(0x27c1, [](const VehicleSelection&) { // testDate length 8
    return formatCurrentUtc("%d/%m/%y");
  });
  patcher->addPatch(0x2811, [](const VehicleSelection&) { // testTime length 8
    return formatCurrentUtc("%H:%M:%S");
  });

  CupsPrinterClient cupsPrinterClient(m_Log);
  m_PrintManager = std::make_unique<PrintManager>(std::move(cupsPrinterClient),
                                                  *m_PrintSettingsRepository,
                                                  *m_PrintStatusStore,
                                                  m_Log);

  auto renderer = std::make_unique<PrnRenderer>(m_Log);
  auto prnWriter = std::make_unique<PrnWriter>(".", m_Log);

  m_LptManager = std::make_unique<LptManager>(std::move(listener),
                                              std::move(patcher),
                                              std::move(prnValidator),
                                              std::move(renderer),
                                              std::move(prnWriter),
                                              *m_LptRepository,
                                              *m_CurrentTestAxleDataStore,
                                              *m_PrnPayloadStore,
                                              *m_SelectedVehicleStore,
                                              *m_LptStore,
                                              *m_SettingsRepository,
                                              *m_PrintSettingsRepository,
                                              *m_PrintManager,
                                              m_Log);

  m_HttpServer = std::make_unique<BrakeTesterHttpServer>(
      *m_LptStore,
      *m_LptRepository,
      *m_SettingsRepository,
      *m_PrintSettingsRepository,
      *m_SerialDeviceStore,
      *m_PrintStatusStore,
      *m_VehicleRepository,
      *m_SelectedVehicleStore,
      *m_LptManager,
      *m_PrintManager,
      m_Log,
      "0.0.0.0",
      80,
      "www");
  m_Log->information("[App Info]: Runtime modules constructed successfully.");
}

App::~App() {
  shutdown();
}

void App::run() {
  if (m_Log) {
    m_Log->information("[App Info]: Starting runtime modules.");
  }
  m_PrintManager->start();
  m_LptManager->start();
  m_HttpServer->start();
  startSerialDeviceRefreshLoop();
  if (m_Log) {
    m_Log->information("[App Info]: Runtime modules started.");
  }
}

void App::shutdown() {
  if (m_Log) {
    m_Log->information("[App Info]: Shutdown requested. Beginning staged shutdown.");
  }

  if (m_HttpServer) {
    if (m_Log) {
      m_Log->information("[App Info]: Stopping HTTP server module.");
    }
    m_HttpServer->stop();
    if (m_Log) {
      m_Log->information("[App Info]: HTTP server module stopped.");
    }
  }

  m_IsSerialDeviceRefreshRunning = false;
  if (m_SerialDeviceRefreshThread.joinable()) {
    if (m_Log) {
      m_Log->information("[App Info]: Waiting for serial device refresh thread to exit.");
    }
    m_SerialDeviceRefreshThread.join();
    if (m_Log) {
      m_Log->information("[App Info]: Serial device refresh thread exited.");
    }
  }

  if (m_PrintManager) {
    if (m_Log) {
      m_Log->information("[App Info]: Stopping print manager module.");
    }
    m_PrintManager->stop();
    if (m_Log) {
      m_Log->information("[App Info]: Print manager module stopped.");
    }
  }

  if (m_LptManager) {
    if (m_Log) {
      m_Log->information("[App Info]: Stopping LPT manager module.");
    }
    m_LptManager->stop();
    if (m_Log) {
      m_Log->information("[App Info]: LPT manager module stopped.");
    }
  }

  if (m_DatabaseHandle != nullptr) {
    if (m_Log) {
      m_Log->information("[App Info]: Closing sqlite database handle.");
    }
    sqlite3_close(m_DatabaseHandle);
    m_DatabaseHandle = nullptr;
    if (m_Log) {
      m_Log->information("[App Info]: Closed sqlite database handle.");
    }
  }
  if (m_Log) {
    m_Log->information("[App Info]: Shutdown completed.");
  }
}

void App::startSerialDeviceRefreshLoop() {
  if (m_IsSerialDeviceRefreshRunning.exchange(true)) {
    return;
  }

  if (m_Log) {
    m_Log->information("[App Info]: Starting serial device refresh thread.");
  }
  m_SerialDeviceRefreshThread = std::thread([this]() {
    if (m_Log) {
      m_Log->information("[App Info]: Serial device refresh thread started.");
    }
    while (m_IsSerialDeviceRefreshRunning) {
      try {
        m_SerialDeviceStore->setDevices(scanSerialDevices());
      } catch (const std::exception& exception) {
        if (m_Log) {
          m_Log->warning(std::string("[App Warning]: Failed to refresh serial devices. ") + exception.what());
        }
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (m_Log) {
      m_Log->information("[App Info]: Serial device refresh thread stopping.");
    }
  });
}

} // namespace brake_tester
