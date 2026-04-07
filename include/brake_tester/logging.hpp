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
    write(LogVerbosity::Warning, messageText);
  }

  void information(const std::string& messageText) {
    write(LogVerbosity::Information, messageText);
  }

  void Error(const std::string& messageText) {
    write(LogVerbosity::Error, messageText);
  }

  void Critical(const std::string& messageText) {
    write(LogVerbosity::Critical, messageText);
  }

private:
  void write(LogVerbosity messageVerbosity, const std::string& messageText) {
    if (static_cast<int>(messageVerbosity) > static_cast<int>(m_Verbosity)) {
      return;
    }

    std::scoped_lock lock(m_Mutex);
    std::cerr << messageText << '\n';
  }

  LogVerbosity m_Verbosity;
  std::mutex m_Mutex;
};

using SharedLogger = std::shared_ptr<Logger>;

} // namespace brake_tester
