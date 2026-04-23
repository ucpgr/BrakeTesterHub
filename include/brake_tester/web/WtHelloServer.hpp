#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "brake_tester/logging.hpp"

namespace brake_tester {

class WtHelloServer {
public:
  explicit WtHelloServer(SharedLogger log,
                         std::string host = "0.0.0.0",
                         int port = 8080,
                         std::string docroot = ".");
  ~WtHelloServer();

  void start();
  void stop();

private:
  struct Impl;

  SharedLogger m_Log;
  std::string m_Host;
  int m_Port;
  std::string m_Docroot;
  std::unique_ptr<Impl> m_Impl;
};

} // namespace brake_tester
