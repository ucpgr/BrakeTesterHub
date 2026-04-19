#include "brake_tester/web/BrakeTesterHttpServer.hpp"
#include "brake_tester/lpt_manager.hpp"
#include "web/BrakeTesterHttpServerInternal.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {

void BrakeTesterHttpServer::configureLptModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring LPT websocket module at /api/lpt.");
  }
  m_Impl->server->WebSocket("/api/lpt", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_Impl->lptClientMutex);
      m_Impl->lptClients.insert(&socket);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: /api/lpt client connected. total=" +
                           std::to_string(m_Impl->lptClients.size()));
      }
    }

    std::string message;
    while (m_Impl->isRunning.load() && socket.read(message) != httplib::ws::Fail) {
    }

    {
      std::scoped_lock lock(m_Impl->lptClientMutex);
      m_Impl->lptClients.erase(&socket);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: /api/lpt client disconnected. total=" +
                           std::to_string(m_Impl->lptClients.size()));
      }
    }
  });

  m_Impl->server->Post("/api/lpt/upload-prn", [this](const httplib::Request& request, httplib::Response& response) {
    if (!request.form.has_file("prn")) {
      response.status = 400;
      response.set_content(R"({"error":"Missing multipart field 'prn'."})", "application/json");
      return;
    }

    const auto& uploadedFile = request.form.get_file("prn");
    const std::string& payload = uploadedFile.content;
    const std::vector<std::uint8_t> incomingBytes(payload.begin(), payload.end());

    if (incomingBytes.empty()) {
      response.status = 400;
      response.set_content(R"({"error":"Uploaded PRN file is empty."})", "application/json");
      return;
    }

    const bool processed = m_LptManager.ingestPrnPayload(incomingBytes);
    if (!processed) {
      response.status = 422;
      response.set_content(R"({"error":"Uploaded PRN file did not pass validation."})", "application/json");
      return;
    }

    response.status = 200;
    response.set_content(R"({"ok":true})", "application/json");
  });
}

void BrakeTesterHttpServer::startLptBroadcastLoop() {
  m_Impl->lptBroadcastThread = std::thread([this] {
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: LPT broadcast thread started.");
    }
    std::uint64_t lastSeenVersion = m_LptStore.getProcessStatusVersion();
    LptProcessStatus lastBroadcastStatus = LptProcessStatus::Idle;
    auto lastStatusBroadcastAt = std::chrono::steady_clock::now();
    constexpr auto progressStatusRefreshInterval = std::chrono::seconds(3);

    const auto broadcastTopBarStatus = [this](LptProcessStatus status) {
      switch (status) {
        case LptProcessStatus::TransferStarted:
          broadcastStatus("progress", "Capture transfer started");
          break;
        case LptProcessStatus::DataPatched:
          broadcastStatus("progress", "Captured data patched");
          break;
        case LptProcessStatus::ConversionStarted:
          broadcastStatus("progress", "Document conversion started");
          break;
        case LptProcessStatus::ConversionFinished:
          broadcastStatus("info", "Document conversion finished");
          break;
        case LptProcessStatus::ThumbnailGenerated:
          broadcastStatus("info", "Thumbnail generated");
          break;
        default:
          broadcastStatus("idle", "Idle");
          break;
      }
    };

    while (m_Impl->isRunning.load()) {
      LptProcessStatus status = LptProcessStatus::Idle;
      std::uint64_t version = lastSeenVersion;

      if (!m_LptStore.waitForProcessStatusAfter(lastSeenVersion, std::chrono::milliseconds(250), status, version)) {
        const auto now = std::chrono::steady_clock::now();
        const bool shouldRefreshProgressStatus =
            (lastBroadcastStatus == LptProcessStatus::TransferStarted ||
             lastBroadcastStatus == LptProcessStatus::ConversionStarted) &&
            (now - lastStatusBroadcastAt >= progressStatusRefreshInterval);
        if (shouldRefreshProgressStatus) {
          broadcastTopBarStatus(lastBroadcastStatus);
          lastStatusBroadcastAt = now;
        }
        continue;
      }

      lastSeenVersion = version;
      lastBroadcastStatus = status;
      nlohmann::json payload{{"event", lptEventNameForStatus(status)}};
      const auto payloadText = payload.dump();

      std::scoped_lock lock(m_Impl->lptClientMutex);
      if (m_Log) {
        m_Log->information("[BrakeTesterHttpServer Info]: Broadcasting LPT event to " +
                           std::to_string(m_Impl->lptClients.size()) + " client(s): " + payloadText);
      }
      for (auto* socket : m_Impl->lptClients) {
        if (socket != nullptr) {
          socket->send(payloadText);
        }
      }

      broadcastTopBarStatus(status);
      lastStatusBroadcastAt = std::chrono::steady_clock::now();
    }
    if (m_Log) {
      m_Log->information("[BrakeTesterHttpServer Info]: LPT broadcast thread stopping.");
    }
  });
}

void BrakeTesterHttpServer::configureStatusModule() {
  if (m_Log) {
    m_Log->information("[BrakeTesterHttpServer Info]: Configuring status websocket module at /api/status.");
  }

  m_Impl->server->WebSocket("/api/status", [this](const httplib::Request&, httplib::ws::WebSocket& socket) {
    {
      std::scoped_lock lock(m_Impl->statusClientMutex);
      m_Impl->statusClients.insert(&socket);
    }

    socket.send(nlohmann::json{{"event", "status.update"}, {"status", {{"level", "idle"}, {"text", "Idle"}}}}.dump());

    std::string message;
    while (m_Impl->isRunning.load() && socket.read(message) != httplib::ws::Fail) {
    }

    {
      std::scoped_lock lock(m_Impl->statusClientMutex);
      m_Impl->statusClients.erase(&socket);
    }
  });
}

void BrakeTesterHttpServer::broadcastStatus(const std::string& level, const std::string& text) {
  const nlohmann::json payload = {
      {"event", "status.update"},
      {"status", {{"level", level}, {"text", text}}},
  };

  const std::string payloadText = payload.dump();
  std::scoped_lock lock(m_Impl->statusClientMutex);
  for (auto* socket : m_Impl->statusClients) {
    if (socket != nullptr) {
      socket->send(payloadText);
    }
  }
}

} // namespace brake_tester
