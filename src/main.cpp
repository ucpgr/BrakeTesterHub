#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <thread>

#include "brake_tester/app.hpp"

namespace {
std::atomic_bool g_KeepRunning{true};

void handleInterruptSignal(int signalValue) {
  if (signalValue == SIGINT || signalValue == SIGTERM) {
    g_KeepRunning = false;
  }
}
} // namespace

int main() {
  try {
    std::signal(SIGINT, handleInterruptSignal);
    std::signal(SIGTERM, handleInterruptSignal);

    brake_tester::App app("brake_tester.db");
    app.run();

    while (g_KeepRunning.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    app.shutdown();
    return 0;
  } catch (const std::exception& startupException) {
    std::cerr << "Application startup failure: " << startupException.what() << '\n';
    return 1;
  }
}
