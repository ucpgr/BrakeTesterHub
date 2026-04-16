#include "brake_tester/app.hpp"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

#include "brake_tester/components.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/repositories.hpp"
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

bool hasConsoleInputAvailable() {
#if defined(_WIN32)
  return _kbhit() != 0;
#else
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(STDIN_FILENO, &readSet);
  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  const int selectResult = select(STDIN_FILENO + 1, &readSet, nullptr, nullptr, &timeout);
  return selectResult > 0 && FD_ISSET(STDIN_FILENO, &readSet);
#endif
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
  m_LptRepository = std::make_unique<LptRepository>(m_DatabaseHandle, m_Log);
  m_SelectedVehicleStore = std::make_unique<SelectedVehicleStore>();
  m_LptStore = std::make_unique<LptStore>();
  m_SerialDeviceStore = std::make_unique<SerialDeviceStore>();

  auto listener = std::make_unique<LptListener>(*m_SettingsRepository, *m_LptStore, m_Log);
  auto patcher = std::make_unique<PrnPatcher>(*m_SelectedVehicleStore, m_Log);
  patcher->addPatch(0x2671, [this](const VehicleSelection&) { // userLine1 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x26c5, [this](const VehicleSelection&) { // userLine2 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x2715, [this](const VehicleSelection&) { // userLine3 length 34
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });

  patcher->addPatch(0x2744, [this](const VehicleSelection&) { // licence length 27
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x2794, [this](const VehicleSelection&) { // make length 27
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x27e4, [this](const VehicleSelection&) { // model length 27
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });
  patcher->addPatch(0x2834, [this](const VehicleSelection&) { // mileage length 27
    return m_LptStore->isLptTestEnabled() ? std::string("TEST") : std::string();
  });

  patcher->addPatch(0x27c1, [](const VehicleSelection&) { // testDate length 8
    return formatCurrentUtc("%d/%m/%y");
  });
  patcher->addPatch(0x2811, [](const VehicleSelection&) { // testTime length 8
    return formatCurrentUtc("%H:%M:%S");
  });

  auto renderer = std::make_unique<PrnRenderer>(m_Log);
  auto prnWriter = std::make_unique<PrnWriter>(".", m_Log);

  m_LptManager = std::make_unique<LptManager>(std::move(listener),
                                              std::move(patcher),
                                              std::move(renderer),
                                              std::move(prnWriter),
                                              *m_LptStore,
                                              *m_SettingsRepository,
                                              m_Log);

  m_HttpServer = std::make_unique<BrakeTesterHttpServer>(
      *m_LptStore,
      *m_SettingsRepository,
      *m_SerialDeviceStore,
      *m_VehicleRepository,
      *m_SelectedVehicleStore,
      *m_LptManager,
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
  m_LptManager->start();
  m_HttpServer->start();
  startSerialDeviceRefreshLoop();
  startInputListener();
}

void App::shutdown() {
  m_IsInputListening = false;
  if (m_Log) {
    m_Log->information("[App Info]: Shutting down runtime modules.");
  }

  if (m_InputThread.joinable()) {
    m_InputThread.join();
  }

  if (m_HttpServer) {
    m_HttpServer->stop();
  }

  m_IsSerialDeviceRefreshRunning = false;
  if (m_SerialDeviceRefreshThread.joinable()) {
    m_SerialDeviceRefreshThread.join();
  }

  if (m_LptManager) {
    m_LptManager->stop();
  }

  if (m_DatabaseHandle != nullptr) {
    sqlite3_close(m_DatabaseHandle);
    m_DatabaseHandle = nullptr;
    if (m_Log) {
      m_Log->information("[App Info]: Closed sqlite database handle.");
    }
  }
}

void App::startInputListener() {
  if (m_IsInputListening.exchange(true)) {
    return;
  }

  const bool isInteractiveConsole =
#if defined(_WIN32)
      _isatty(_fileno(stdin)) != 0;
#else
      isatty(fileno(stdin)) != 0;
#endif

  if (!isInteractiveConsole) {
    m_IsInputListening = false;
    if (m_Log) {
      m_Log->information("[App Info]: Input listener disabled (stdin is not an interactive terminal).");
    }
    return;
  }

  m_InputThread = std::thread([this]() {
    if (m_Log) {
      m_Log->information("[App Info]: Input listener active. Type 't' and press Enter for test signal.");
    }
    std::string consoleInput;
    while (m_IsInputListening) {
      if (!hasConsoleInputAvailable()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if (!std::getline(std::cin, consoleInput)) {
        std::cin.clear();
        continue;
      }

      if (consoleInput == "t") {
        m_LptManager->sendTestSignal(true);
      }
    }
  });
}

void App::startSerialDeviceRefreshLoop() {
  if (m_IsSerialDeviceRefreshRunning.exchange(true)) {
    return;
  }

  m_SerialDeviceRefreshThread = std::thread([this]() {
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
  });
}

} // namespace brake_tester
