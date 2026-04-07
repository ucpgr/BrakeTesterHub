#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptManager {
public:
  LptManager(std::unique_ptr<ILptListener> listener,
             std::unique_ptr<IPrnPatcher> patcher,
             std::unique_ptr<IPrnRenderer> renderer,
             std::unique_ptr<IRenderedDocumentWriter> writer,
             SharedLogger log)
      : m_Listener(std::move(listener)),
        m_Patcher(std::move(patcher)),
        m_Renderer(std::move(renderer)),
        m_Writer(std::move(writer)),
        m_Log(std::move(log)) {}

  ~LptManager() {
    stop();
  }

  void start() {
    if (m_IsRunning.exchange(true)) {
      return;
    }
    if (m_Log) {
      m_Log->information("[LptManager Info]: Starting worker thread.");
    }

    m_WorkerThread = std::thread([this] {
      while (m_IsRunning) {
        try {
          auto incomingBytes = m_Listener->captureTransmission(m_IsRunning);
          if (incomingBytes.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
          }

          auto patchedBytes = m_Patcher->patch(incomingBytes);
          auto renderedPages = m_Renderer->render(patchedBytes);
          m_Writer->writePages(renderedPages, "capture");
        } catch (const std::exception& processingException) {
          if (m_Log) {
            m_Log->Error(processingException.what());
            m_Log->information("[LptManager Info]: Trying again in 10 seconds.");
          }

          constexpr auto retryWaitDuration = std::chrono::seconds(10);
          constexpr auto retryPollInterval = std::chrono::milliseconds(100);
          auto waitedDuration = std::chrono::milliseconds(0);
          while (m_IsRunning && waitedDuration < retryWaitDuration) {
            std::this_thread::sleep_for(retryPollInterval);
            waitedDuration += retryPollInterval;
          }
        }
      }
    });
  }

  void stop() {
    if (!m_IsRunning.exchange(false)) {
      return;
    }
    if (m_Log) {
      m_Log->information("[LptManager Info]: Stopping worker thread.");
    }

    if (m_WorkerThread.joinable()) {
      m_WorkerThread.join();
    }
  }

  void sendTestSignal() {
    try {
      m_Listener->test();
    } catch (const std::exception& testException) {
      if (m_Log) {
        m_Log->Error(testException.what());
      }
    }
  }

private:
  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;

  std::unique_ptr<ILptListener> m_Listener;
  std::unique_ptr<IPrnPatcher> m_Patcher;
  std::unique_ptr<IPrnRenderer> m_Renderer;
  std::unique_ptr<IRenderedDocumentWriter> m_Writer;
  SharedLogger m_Log;
};

} // namespace brake_tester
