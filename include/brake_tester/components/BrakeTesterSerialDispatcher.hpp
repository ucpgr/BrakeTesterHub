#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

#include <libserial/SerialPort.h>

#include "brake_tester/BrakeTester/Codec.hpp"
#include "brake_tester/BrakeTester/Framer.h"
#include "brake_tester/repositories/SettingsRepository.hpp"

namespace brake_tester {

class BrakeTesterSerialDispatcher {
public:
  explicit BrakeTesterSerialDispatcher(SettingsRepository& settingsRepository);
  ~BrakeTesterSerialDispatcher();

  void start();
  void stop();

  std::future<BrakeTester::Frame> submit(BrakeTester::Frame frame);

private:
  struct PendingBrakeTesterRequest {
    std::uint64_t id{0};
    BrakeTester::Frame frame{};
    std::promise<BrakeTester::Frame> promise{};
    std::chrono::steady_clock::time_point queuedAt{};
  };

  static constexpr std::chrono::milliseconds c_ReplyTimeout{750};

  void workerLoop();
  std::optional<PendingBrakeTesterRequest> waitForNextRequest();
  BrakeTester::Frame executeRequest(const PendingBrakeTesterRequest& request);
  void failPendingRequests(const std::exception_ptr& exceptionPtr);
  void ensureSerialPortOpen();
  void flushSerialInput();
  std::vector<uint8_t> readRawReplyFrame();

  SettingsRepository& m_SettingsRepository;

  std::atomic_bool m_IsRunning{false};
  std::thread m_WorkerThread;

  std::mutex m_QueueMutex;
  std::condition_variable m_QueueCv;
  std::queue<PendingBrakeTesterRequest> m_PendingRequests;
  std::uint64_t m_NextRequestId{1};

  LibSerial::SerialPort m_SerialPort;
  bool m_IsSerialPortOpen{false};
  std::string m_OpenDevicePath{};
};

} // namespace brake_tester
