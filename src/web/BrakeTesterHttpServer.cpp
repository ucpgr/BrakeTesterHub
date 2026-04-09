#include "brake_tester/web/BrakeTesterHttpServer.hpp"

#include <chrono>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {

BrakeTesterHttpServer::BrakeTesterHttpServer(ILptStore& lptStore,
                                             SharedLogger log,
                                             std::string host,
                                             int port,
                                             std::string staticRoot)
    : m_LptStore(lptStore),
      m_Log(std::move(log)),
      m_Host(std::move(host)),
      m_Port(port),
      m_StaticRoot(std::move(staticRoot)),
      m_Server(std::make_unique<httplib::Server>()) {}

BrakeTesterHttpServer::~BrakeTesterHttpServer() {
  stop();
}

void BrakeTesterHttpServer::start() {
  if (m_IsRunning.exchange(true)) {
    return;
  }

  m_Server->set_mount_point("/", m_StaticRoot);
  configureLptModule();
  startLptBroadcastLoop();

  m_ServerThread = std::thread([this] {
    m_Server->listen(m_Host, m_Port);
  });
}

void BrakeTesterHttpServer::stop() {
  if (!m_IsRunning.exchange(false)) {
    return;
  }

  if (m_Server) {
    m_Server->stop();
  }

  if (m_LptBroadcastThread.joinable()) {
    m_LptBroadcastThread.join();
  }

  if (m_ServerThread.joinable()) {
    m_ServerThread.join();
  }
}

void BrakeTesterHttpServer::configureLptModule() {
  m_Server->WebSocket("/lpt", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_LptClientMutex);
      m_LptClients.insert(&socket);
    }

    std::string message;
    while (m_IsRunning.load() && socket.read(message) != httplib::ws::Fail) {
    }

    {
      std::scoped_lock lock(m_LptClientMutex);
      m_LptClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::startLptBroadcastLoop() {
  m_LptBroadcastThread = std::thread([this] {
    std::uint64_t lastSeenVersion = m_LptStore.getProcessStatusVersion();

    while (m_IsRunning.load()) {
      LptProcessStatus status = LptProcessStatus::Idle;
      std::uint64_t version = lastSeenVersion;

      if (!m_LptStore.waitForProcessStatusAfter(lastSeenVersion, std::chrono::milliseconds(250), status, version)) {
        continue;
      }

      lastSeenVersion = version;
      nlohmann::json payload{{"event", lptEventNameForStatus(status)}};
      const auto payloadText = payload.dump();

      std::scoped_lock lock(m_LptClientMutex);
      for (auto* socket : m_LptClients) {
        if (socket != nullptr) {
          socket->send(payloadText);
        }
      }
    }
  });
}

std::string BrakeTesterHttpServer::lptEventNameForStatus(LptProcessStatus status) const {
  switch (status) {
    case LptProcessStatus::TransferStarted: return "lpt.transfer_started";
    case LptProcessStatus::DataPatched: return "lpt.data_patched";
    case LptProcessStatus::ConversionStarted: return "lpt.conversion_started";
    case LptProcessStatus::ConversionFinished: return "lpt.conversion_finished";
    case LptProcessStatus::Idle:
    default: return "lpt.idle";
  }
}

} // namespace brake_tester
