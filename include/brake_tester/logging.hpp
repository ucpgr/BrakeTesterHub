#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace brake_tester {

enum class LogVerbosity {
  Critical = 0,
  Error = 1,
  Warning = 2,
  Information = 3
};

class Logger {
public:
  explicit Logger(LogVerbosity verbosity) : m_Verbosity(verbosity) {}

  void warning(const std::string& messageText) {
    write(LogVerbosity::Warning, "[WARNING]", messageText);
  }

  void information(const std::string& messageText) {
    write(LogVerbosity::Information, "[INFO]", messageText);
  }

  void error(const std::string& messageText) {
    write(LogVerbosity::Error, "[ERROR]", messageText);
  }

  void critical(const std::string& messageText) {
    write(LogVerbosity::Critical, "[CRITICAL]", messageText);
  }

private:
  void write(LogVerbosity messageVerbosity, const char* severity, const std::string& messageText) {
    if (static_cast<int>(messageVerbosity) > static_cast<int>(m_Verbosity)) {
      return;
    }

    std::scoped_lock lock(m_Mutex);
    std::cerr << severity << ' ' << messageText << '\n';
  }

  LogVerbosity m_Verbosity;
  std::mutex m_Mutex;
};

using SharedLogger = std::shared_ptr<Logger>;

} // namespace brake_tester
