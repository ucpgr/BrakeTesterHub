#include "brake_tester/app.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "brake_tester/components.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "brake_tester/repositories.hpp"
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
      *m_VehicleRepository,
      *m_SelectedVehicleStore,
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
      if (std::cin.rdbuf()->in_avail() <= 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if (!std::getline(std::cin, consoleInput)) {
        std::cin.clear();
        continue;
      }

      if (consoleInput == "t") {
        m_LptStore->setLptTestEnabled(true);
        m_LptManager->sendTestSignal();
      }
    }
  });
}

} // namespace brake_tester
