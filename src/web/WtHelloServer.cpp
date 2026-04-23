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
  app->root()->addWidget(std::make_unique<Wt::WText>("Hello, world!"));
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
