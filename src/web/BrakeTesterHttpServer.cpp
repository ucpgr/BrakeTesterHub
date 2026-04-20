#include "brake_tester/web/BrakeTesterHttpServer.hpp"
#include "web/BrakeTesterHttpServerInternal.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {


BrakeTesterHttpServer::BrakeTesterHttpServer(ILptStore& lptStore,
                                             ILptRepository& lptRepository,
                                             ISettingsRepository& settingsRepository,
                                             IPrintSettingsRepository& printSettingsRepository,
                                             ISerialDeviceStore& serialDeviceStore,
                                             IPrintStatusStore& printStatusStore,
                                             IVehicleRepository& vehicleRepository,
                                             ISelectedVehicleStore& selectedVehicleStore,
                                             LptManager& lptManager,
                                             PrintManager& printManager,
                                             SharedLogger log,
                                             std::string host,
                                             int port,
                                             std::string staticRoot)
    : m_LptStore(lptStore),
      m_LptRepository(lptRepository),
      m_SettingsRepository(settingsRepository),
      m_PrintSettingsRepository(printSettingsRepository),
      m_SerialDeviceStore(serialDeviceStore),
      m_PrintStatusStore(printStatusStore),
      m_VehicleRepository(vehicleRepository),
      m_SelectedVehicleStore(selectedVehicleStore),
      m_LptManager(lptManager),
      m_PrintManager(printManager),
      m_Log(std::move(log)),
      m_Host(std::move(host)),
      m_Port(port),
      m_StaticRoot(std::move(staticRoot)),
      m_Impl(std::make_unique<Impl>()) {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Constructed for " + m_Host + ":" + std::to_string(m_Port) +
                       ", staticRoot=" + m_StaticRoot);
  }
}

BrakeTesterHttpServer::~BrakeTesterHttpServer() {
  stop();
}

void BrakeTesterHttpServer::start() {
  if (m_Impl->isRunning.exchange(true)) {
    if (m_Log) {
      m_Log->warning("[BrakeTesterHttpServer Warning]: Start requested while server already running.");
    }
    return;
  }

  m_Impl->server->set_mount_point("/", m_StaticRoot);
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Mounted static files at '/' from " + m_StaticRoot);
  }

  const std::filesystem::path indexPath = std::filesystem::path(m_StaticRoot) / "index.html";
  m_Impl->server->Get(R"(/(live|settings|history))", [this, indexPath](const httplib::Request& request,
                                                                        httplib::Response& response) {
    std::ifstream indexFile(indexPath, std::ios::binary);
    if (!indexFile.is_open()) {
      response.status = 500;
      response.set_content("Unable to load frontend entrypoint", "text/plain");
      if (m_Log) {
        m_Log->error("[BrakeTesterHttpServer Error]: Failed to open " + indexPath.string() + " for path " +
                     request.path);
      }
      return;
    }

    const std::string indexContent((std::istreambuf_iterator<char>(indexFile)), std::istreambuf_iterator<char>());
    response.set_content(indexContent, "text/html; charset=utf-8");
  });

  const std::string testsRoot = std::filesystem::path("tests").string();
  const bool testsMounted = m_Impl->server->set_mount_point("/tests", testsRoot);
  if (m_Log) {
    if (testsMounted) {
      m_Log->information("[BrakeTesterHttpServer Info]: Mounted test artifacts at '/tests' from " + testsRoot);
    } else {
      m_Log->warning("[BrakeTesterHttpServer Warning]: Failed to mount '/tests' from " + testsRoot);
    }
  }

  configureLptModule();
  configureVehicleModule();
  configureStatusModule();
  configureSettingsModule();
  configureHistoryModule();
  startLptBroadcastLoop();
  startSettingsBroadcastLoop();
  startPrintStatusBroadcastLoop();

  m_Impl->serverThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: HTTP server listening on " + m_Host + ":" +
                         std::to_string(m_Port));
    }
    m_Impl->server->listen(m_Host, m_Port);
  });
}

void BrakeTesterHttpServer::stop() {
  if (!m_Impl->isRunning.exchange(false)) {
    if (m_Log) {
      m_Log->warning("[BrakeTesterHttpServer Warning]: Stop requested while server not running.");
    }
    return;
  }
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Stopping HTTP server.");
  }

  if (m_Impl->server) {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Calling httplib::Server::stop().");
    }
    m_Impl->server->stop();
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: httplib::Server::stop() returned.");
    }
  }

  const auto closeSockets = [this](const char* channelName,
                                   std::mutex& clientMutex,
                                   std::unordered_set<httplib::ws::WebSocket*>& clients) {
    std::vector<httplib::ws::WebSocket*> socketsToClose;
    {
      std::scoped_lock lock(clientMutex);
      socketsToClose.reserve(clients.size());
      for (auto* socket : clients) {
        if (socket != nullptr) {
          socketsToClose.push_back(socket);
        }
      }
    }

    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Closing " + std::to_string(socketsToClose.size()) +
                         " websocket client(s) for " + channelName + ".");
    }
    for (auto* socket : socketsToClose) {
      socket->close();
    }
  };

  closeSockets("/api/lpt", m_Impl->lptClientMutex, m_Impl->lptClients);
  closeSockets("/api/vehicles", m_Impl->vehicleClientMutex, m_Impl->vehicleClients);
  closeSockets("/api/status", m_Impl->statusClientMutex, m_Impl->statusClients);
  closeSockets("/api/settings", m_Impl->settingsClientMutex, m_Impl->settingsClients);

  if (m_Impl->lptBroadcastThread.joinable()) {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Waiting for LPT broadcast thread to join.");
    }
    m_Impl->lptBroadcastThread.join();
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: LPT broadcast thread joined.");
    }
  }
  if (m_Impl->settingsBroadcastThread.joinable()) {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Waiting for settings broadcast thread to join.");
    }
    m_Impl->settingsBroadcastThread.join();
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Settings broadcast thread joined.");
    }
  }

  if (m_Impl->printStatusBroadcastThread.joinable()) {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Waiting for print status broadcast thread to join.");
    }
    m_Impl->printStatusBroadcastThread.join();
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Print status broadcast thread joined.");
    }
  }

  if (m_Impl->serverThread.joinable()) {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: Waiting for HTTP server listener thread to join.");
    }
    m_Impl->serverThread.join();
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: HTTP server listener thread joined.");
    }
  } else if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: HTTP server listener thread was not joinable during stop.");
  }

  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Stop sequence completed.");
  }
}

std::string BrakeTesterHttpServer::lptEventNameForStatus(LptProcessStatus status) const {
  switch (status) {
    case LptProcessStatus::TransferStarted: return "lpt.transfer_started";
    case LptProcessStatus::DataPatched: return "lpt.data_patched";
    case LptProcessStatus::ConversionStarted: return "lpt.conversion_started";
    case LptProcessStatus::ConversionFinished: return "lpt.conversion_finished";
    case LptProcessStatus::ThumbnailGenerated: return "lpt.thumbnail_generated";
    case LptProcessStatus::Idle:
    default: return "lpt.idle";
  }
}

} // namespace brake_tester
