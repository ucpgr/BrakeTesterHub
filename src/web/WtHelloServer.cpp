#include "brake_tester/web/WtHelloServer.hpp"

#include <chrono>
#include <vector>

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WEnvironment.h>
#include <Wt/WServer.h>
#include <Wt/WText.h>

namespace brake_tester {
namespace {
std::unique_ptr<Wt::WApplication> createHelloApplication(const Wt::WEnvironment& environment) {
  auto app = std::make_unique<Wt::WApplication>(environment);
  app->setTitle("BrakeTesterHub Wt");

  auto page = app->root()->addWidget(std::make_unique<Wt::WContainerWidget>());
  page->addStyleClass("wt-page");

  const auto topBarMarkup = R"(
    <style>
      .wt-page {
        font-family: Inter, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
        color: #111827;
        background: #f8fafc;
        min-height: 100vh;
      }

      .topbar {
        width: 100%;
        box-sizing: border-box;
        display: flex;
        align-items: center;
        gap: 1rem;
        padding: 0.5rem 1rem;
        border-bottom: 1px solid #e5e7eb;
        background: #ffffff;
      }

      .online-pill {
        display: inline-flex;
        align-items: center;
        gap: 0.5rem;
        font-size: 0.95rem;
        font-weight: 500;
        color: #166534;
        white-space: nowrap;
      }

      .online-pill::before {
        content: "";
        width: 0.65rem;
        height: 0.65rem;
        border-radius: 999px;
        background: #22c55e;
        box-shadow: 0 0 0 2px rgba(34, 197, 94, 0.2);
      }

      .status {
        flex: 1;
        min-width: 0;
        color: #6b7280;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
        font-size: 0.92rem;
      }

      .nav {
        display: inline-flex;
        align-items: center;
        gap: 0.4rem;
      }

      .nav-item {
        border: 1px solid #d1d5db;
        border-radius: 0.45rem;
        background: #ffffff;
        color: #111827;
        font-size: 0.85rem;
        font-weight: 500;
        padding: 0.4rem 0.75rem;
        white-space: nowrap;
      }
    </style>

    <header class="topbar">
      <div class="online-pill">Connected</div>
      <div class="status">status messages</div>
      <nav class="nav">
        <div class="nav-item">live</div>
        <div class="nav-item">settings</div>
        <div class="nav-item">history</div>
      </nav>
    </header>
  )";

  auto topBar = page->addWidget(std::make_unique<Wt::WText>(topBarMarkup, Wt::TextFormat::UnsafeXHTML));
  topBar->setInline(false);

  return app;
}
} // namespace

struct WtHelloServer::Impl {
  std::atomic_bool isRunning{false};
  std::thread serverThread;
};

WtHelloServer::WtHelloServer(SharedLogger log, std::string host, int port, std::string docroot)
    : m_Log(std::move(log)), m_Host(std::move(host)), m_Port(port), m_Docroot(std::move(docroot)),
      m_Impl(std::make_unique<Impl>()) {
  if (m_Log) {
    m_Log->information("[WtHelloServer Info]: Constructed for " + m_Host + ":" + std::to_string(m_Port));
  }
}

WtHelloServer::~WtHelloServer() {
  stop();
}

void WtHelloServer::start() {
  if (m_Impl->isRunning.exchange(true)) {
    if (m_Log) {
      m_Log->warning("[WtHelloServer Warning]: Start requested while server already running.");
    }
    return;
  }

  m_Impl->serverThread = std::thread([this]() {
    try {
      std::vector<std::string> arguments = {
          "BrakeTesterHub",
          "--http-address", m_Host,
          "--http-port", std::to_string(m_Port),
          "--docroot", m_Docroot};

      std::vector<char*> argv;
      argv.reserve(arguments.size());
      for (std::string& argument : arguments) {
        argv.push_back(argument.data());
      }

      Wt::WServer server(argv.front());
      server.setServerConfiguration(static_cast<int>(argv.size()), argv.data(), WTHTTP_CONFIGURATION);
      server.addEntryPoint(Wt::EntryPointType::Application, createHelloApplication);

      if (!server.start()) {
        if (m_Log) {
          m_Log->Error("[WtHelloServer Error]: Failed to start Wt server.");
        }
        m_Impl->isRunning = false;
        return;
      }

      if (m_Log) {
        m_Log->information("[WtHelloServer Info]: Listening on " + m_Host + ":" + std::to_string(m_Port));
      }

      while (m_Impl->isRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      server.stop();
      if (m_Log) {
        m_Log->information("[WtHelloServer Info]: Wt server stopped.");
      }
    } catch (const std::exception& exception) {
      if (m_Log) {
        m_Log->Error(std::string("[WtHelloServer Error]: Wt thread terminated: ") + exception.what());
      }
      m_Impl->isRunning = false;
    }
  });
}

void WtHelloServer::stop() {
  if (!m_Impl->isRunning.exchange(false)) {
    if (m_Log) {
      m_Log->warning("[WtHelloServer Warning]: Stop requested while server not running.");
    }
    return;
  }

  if (m_Impl->serverThread.joinable()) {
    m_Impl->serverThread.join();
  }
}

} // namespace brake_tester
