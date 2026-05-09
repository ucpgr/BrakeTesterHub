#include "brake_tester/components/BrakeTesterSerialDispatcher.hpp"

#include <stdexcept>

namespace brake_tester {
namespace {
LibSerial::BaudRate toBaudRate(const std::uint32_t baudRateValue) {
  switch (baudRateValue) {
    case 1200: return LibSerial::BaudRate::BAUD_1200;
    case 2400: return LibSerial::BaudRate::BAUD_2400;
    case 4800: return LibSerial::BaudRate::BAUD_4800;
    case 9600: return LibSerial::BaudRate::BAUD_9600;
    case 19200: return LibSerial::BaudRate::BAUD_19200;
    case 38400: return LibSerial::BaudRate::BAUD_38400;
    case 57600: return LibSerial::BaudRate::BAUD_57600;
    case 115200: return LibSerial::BaudRate::BAUD_115200;
    default: return LibSerial::BaudRate::BAUD_9600;
  }
}
} // namespace

BrakeTesterSerialDispatcher::BrakeTesterSerialDispatcher(SettingsRepository& settingsRepository)
    : m_SettingsRepository(settingsRepository) {}

BrakeTesterSerialDispatcher::~BrakeTesterSerialDispatcher() { stop(); }

void BrakeTesterSerialDispatcher::start() {
  bool expected = false;
  if (!m_IsRunning.compare_exchange_strong(expected, true)) {
    return;
  }

  m_WorkerThread = std::thread([this]() { workerLoop(); });
}

void BrakeTesterSerialDispatcher::stop() {
  bool expected = true;
  if (!m_IsRunning.compare_exchange_strong(expected, false)) {
    return;
  }

  m_QueueCv.notify_all();
  if (m_WorkerThread.joinable()) {
    m_WorkerThread.join();
  }

  failPendingRequests(std::make_exception_ptr(std::runtime_error("Dispatcher stopped.")));

  if (m_IsSerialPortOpen) {
    m_SerialPort.Close();
    m_IsSerialPortOpen = false;
    m_OpenDevicePath.clear();
  }
}

std::future<BrakeTester::Frame> BrakeTesterSerialDispatcher::submit(BrakeTester::Frame frame) {
  std::promise<BrakeTester::Frame> promise;
  auto future = promise.get_future();

  std::lock_guard<std::mutex> lock(m_QueueMutex);
  if (!m_IsRunning) {
    promise.set_exception(std::make_exception_ptr(std::runtime_error("Dispatcher is not running.")));
    return future;
  }

  m_PendingRequests.push(PendingBrakeTesterRequest{m_NextRequestId++, std::move(frame), std::move(promise), std::chrono::steady_clock::now()});
  m_QueueCv.notify_one();
  return future;
}

void BrakeTesterSerialDispatcher::workerLoop() {
  while (m_IsRunning) {
    auto pendingRequest = waitForNextRequest();
    if (!pendingRequest.has_value()) {
      continue;
    }

    try {
      auto reply = executeRequest(*pendingRequest);
      pendingRequest->promise.set_value(std::move(reply));
    } catch (...) {
      pendingRequest->promise.set_exception(std::current_exception());
    }
  }
}

std::optional<BrakeTesterSerialDispatcher::PendingBrakeTesterRequest> BrakeTesterSerialDispatcher::waitForNextRequest() {
  std::unique_lock<std::mutex> lock(m_QueueMutex);
  m_QueueCv.wait(lock, [this]() { return !m_IsRunning || !m_PendingRequests.empty(); });

  if (!m_IsRunning || m_PendingRequests.empty()) {
    return std::nullopt;
  }

  PendingBrakeTesterRequest request = std::move(m_PendingRequests.front());
  m_PendingRequests.pop();
  return request;
}

BrakeTester::Frame BrakeTesterSerialDispatcher::executeRequest(const PendingBrakeTesterRequest& request) {
  (void)request;
  ensureSerialPortOpen();

  auto requestBytes = BrakeTester::serializeFrame(request.frame);
  m_SerialPort.Write(requestBytes);

  auto rawReply = readRawReplyFrame();
  return BrakeTester::decodeFrame(rawReply);
}

void BrakeTesterSerialDispatcher::ensureSerialPortOpen() {
  const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
  if (serialSettings.brakeTesterDevicePath.empty()) {
    throw std::runtime_error("Brake tester serial device path is empty.");
  }

  const bool shouldReopenPort = !m_IsSerialPortOpen || m_OpenDevicePath != serialSettings.brakeTesterDevicePath;
  if (!shouldReopenPort) {
    return;
  }

  if (m_IsSerialPortOpen) {
    m_SerialPort.Close();
    m_IsSerialPortOpen = false;
  }

  m_SerialPort.Open(serialSettings.brakeTesterDevicePath);
  m_SerialPort.SetBaudRate(toBaudRate(serialSettings.baudRate));
  m_IsSerialPortOpen = true;
  m_OpenDevicePath = serialSettings.brakeTesterDevicePath;
}

std::vector<uint8_t> BrakeTesterSerialDispatcher::readRawReplyFrame() {
  BrakeTester::Framer framer(BrakeTester::calculateChecksum);
  const auto deadline = std::chrono::steady_clock::now() + c_ReplyTimeout;

  std::vector<uint8_t> frameData;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if (remainingMs.count() <= 0) {
      break;
    }

    unsigned char readByte{0};
    try {
      m_SerialPort.ReadByte(readByte, remainingMs.count());
      framer.consume(std::span<const uint8_t>(&readByte, 1));
      frameData = framer.tryGetFrame();
      if (!frameData.empty()) {
        return frameData;
      }
    } catch (const LibSerial::ReadTimeout&) {
      break;
    }
  }

  flushSerialInput();
  throw std::runtime_error("Timed out waiting for brake tester reply frame.");
}

void BrakeTesterSerialDispatcher::flushSerialInput() {
  if (!m_IsSerialPortOpen) {
    return;
  }

  try {
    m_SerialPort.FlushInputBuffer();
  } catch (...) {
  }
}

void BrakeTesterSerialDispatcher::failPendingRequests(const std::exception_ptr& exceptionPtr) {
  std::queue<PendingBrakeTesterRequest> pending;
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    pending.swap(m_PendingRequests);
  }

  while (!pending.empty()) {
    pending.front().promise.set_exception(exceptionPtr);
    pending.pop();
  }
}

} // namespace brake_tester
