#include "brake_tester/components/LptListener.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <stdexcept>
#include <thread>

namespace brake_tester {

namespace {
constexpr std::size_t kMaxTransmissionBytes = 1024 * 1024;

LibSerial::BaudRate toBaudRate(std::uint32_t baudRateValue) {
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

bool isReadTimeoutError(std::string errorReason) {
  std::transform(errorReason.begin(), errorReason.end(), errorReason.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return errorReason.find("timeout") != std::string::npos;
}
} // namespace

LptListener::LptListener(const ISettingsRepository& settingsRepository, ILptStore& lptStore, SharedLogger log)
    : m_SettingsRepository(settingsRepository), m_LptStore(lptStore), m_Log(std::move(log)) {
  m_LptStore.setListenerStatus(LptListenerStatus::Idle);
  if (m_Log) {
    m_Log->information("[LptListener Info]: Initialized and listener status set to Idle.");
  }
}

LptListener::~LptListener() {
  closeSerialPortIfOpen();
}

std::vector<std::uint8_t> LptListener::captureTransmission(const std::atomic_bool& shouldKeepRunning) {
  const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
  const auto endOfTransmissionSilenceTimeout = serialSettings.silenceTimeout;
  std::vector<std::uint8_t> transmissionBuffer;
  transmissionBuffer.reserve(2048);

  ensureSerialPortOpen(serialSettings);
  if (m_Log) {
    m_Log->information("[LptListener Info]: Waiting for serial bytes on " + serialSettings.devicePath + ".");
  }

  bool hasCapturedData = false;
  auto captureStartTimestamp = std::chrono::steady_clock::now();
  auto lastCapturedDataTimestamp = std::chrono::steady_clock::now();

  while (shouldKeepRunning) {
    std::uint8_t capturedByte = 0;
    bool didReadByte = false;
    try {
      char readByte = '\0';
      m_SerialPort.ReadByte(readByte, serialSettings.silenceTimeout.count());
      capturedByte = static_cast<std::uint8_t>(readByte);
      didReadByte = true;
    } catch (const std::exception& readException) {
      const std::string readErrorReason = readException.what();
      const bool isReadTimeout = isReadTimeoutError(readErrorReason);

      if (isReadTimeout) {
        if (hasCapturedData &&
            (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= endOfTransmissionSilenceTimeout) {
          if (m_Log) {
            m_Log->information("[LptListener Info]: Capture complete after silence timeout. Bytes: " +
                               std::to_string(transmissionBuffer.size()));
          }
          m_LptStore.setListenerStatus(LptListenerStatus::Idle);
          return transmissionBuffer;
        }

        if (!m_SerialPort.IsOpen()) {
          m_IsSerialPortOpen = false;
          ensureSerialPortOpen(serialSettings);
        }
        continue;
      }

      if (!m_SerialPort.IsOpen()) {
        m_IsSerialPortOpen = false;
        ensureSerialPortOpen(serialSettings);
        continue;
      }

      throw std::runtime_error("[LptManager Error]: Serial read failed on device '" + serialSettings.devicePath +
                               "'. Reason: " + readErrorReason);
    }

    if (didReadByte) {
      if (!hasCapturedData) {
        captureStartTimestamp = std::chrono::steady_clock::now();
        m_LptStore.setListenerStatus(LptListenerStatus::CaptureStarted);
        if (m_Log) {
          m_Log->information("[LptListener Info]: Capture started.");
        }
      }
      hasCapturedData = true;
      lastCapturedDataTimestamp = std::chrono::steady_clock::now();

      if (transmissionBuffer.size() >= kMaxTransmissionBytes) {
        throw std::runtime_error("[LptListener Error]: Capture exceeded maximum buffer size of " +
                                 std::to_string(kMaxTransmissionBytes) + " bytes.");
      }
      transmissionBuffer.push_back(capturedByte);
      continue;
    }

    if (hasCapturedData &&
        (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= endOfTransmissionSilenceTimeout) {
      if (m_Log) {
        m_Log->information("[LptListener Info]: Capture complete after inactivity. Bytes: " +
                           std::to_string(transmissionBuffer.size()));
      }
      m_LptStore.setListenerStatus(LptListenerStatus::Idle);
      return transmissionBuffer;
    }

    if (!m_SerialPort.IsOpen()) {
      m_IsSerialPortOpen = false;
      ensureSerialPortOpen(serialSettings);
    }
  }

  if (hasCapturedData && m_Log) {
    const auto captureElapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - captureStartTimestamp)
            .count();
    m_Log->information("[LptListener Info]: Byte capture ended due to stop. Total bytes: " +
                       std::to_string(transmissionBuffer.size()) + ", elapsedMs: " +
                       std::to_string(captureElapsedMilliseconds));
  }
  m_LptStore.setListenerStatus(LptListenerStatus::Idle);
  return {};
}

void LptListener::test() {
  const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
  ensureSerialPortOpen(serialSettings);
  const char testByte = 't';
  m_SerialPort.WriteByte(testByte);
  if (m_Log) {
    m_Log->information("[LptListener Info]: Wrote test byte 't' to serial device.");
  }
}

void LptListener::ensureSerialPortOpen(const SerialSettings& serialSettings) {
  const bool shouldReopenPort = (!m_IsSerialPortOpen || m_OpenDevicePath != serialSettings.devicePath);
  if (!shouldReopenPort) {
    return;
  }

  closeSerialPortIfOpen();
  if (m_Log) {
    m_Log->information("[LptListener Info]: Opening serial device: " + serialSettings.devicePath);
  }
  m_SerialPort.Open(serialSettings.devicePath);
  m_SerialPort.SetBaudRate(toBaudRate(serialSettings.baudRate));
  m_IsSerialPortOpen = true;
  m_OpenDevicePath = serialSettings.devicePath;
  if (m_Log) {
    m_Log->information("[LptListener Info]: Serial device opened.");
  }
}

void LptListener::closeSerialPortIfOpen() {
  if (!m_IsSerialPortOpen) {
    return;
  }
  m_SerialPort.Close();
  m_IsSerialPortOpen = false;
  m_OpenDevicePath.clear();
  if (m_Log) {
    m_Log->information("[LptListener Info]: Serial device closed.");
  }
}

} // namespace brake_tester
