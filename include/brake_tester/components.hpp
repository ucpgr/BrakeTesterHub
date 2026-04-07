#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <libserial/SerialPort.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

inline LibSerial::BaudRate toBaudRate(std::uint32_t baudRateValue) {
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

class LptListener final : public ILptListener {
public:
  LptListener(const ISettingsRepository& settingsRepository, ILptStore& lptStore, SharedLogger log)
      : m_SettingsRepository(settingsRepository), m_LptStore(lptStore), m_Log(std::move(log)) {
    m_LptStore.setListenerStatus(LptListenerStatus::Idle);
  }

  ~LptListener() override {
    closeSerialPortIfOpen();
  }

  std::vector<std::uint8_t> captureTransmission(const std::atomic_bool& shouldKeepRunning) override {
    const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
    std::vector<std::uint8_t> transmissionBuffer;
    transmissionBuffer.reserve(2048);

    if (serialSettings.devicePath.empty() && m_Log) {
      m_Log->Critical("Serial port path is empty");
    }
    ensureSerialPortOpen(serialSettings);

    bool hasCapturedData = false;
    auto captureStartTimestamp = std::chrono::steady_clock::now();
    auto lastCapturedDataTimestamp = std::chrono::steady_clock::now();

    while (shouldKeepRunning) {
      LibSerial::DataBuffer chunkBuffer;
      try {
        m_SerialPort.Read(chunkBuffer, serialSettings.readChunkSize, serialSettings.silenceTimeout.count());
      } catch (const std::exception& readException) {
        const std::string readErrorReason = readException.what();
        const bool isReadTimeout =
            (readErrorReason.find("timeout") != std::string::npos || readErrorReason.find("Timeout") != std::string::npos);

        if (isReadTimeout) {
          if (hasCapturedData &&
              (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= serialSettings.silenceTimeout) {
            if (m_Log) {
              const auto captureElapsedMilliseconds =
                  std::chrono::duration_cast<std::chrono::milliseconds>(lastCapturedDataTimestamp - captureStartTimestamp)
                      .count();
              m_Log->information("[LptListener Info]: Byte capture ended. Total bytes: " +
                                 std::to_string(transmissionBuffer.size()) + ", elapsedMs: " +
                                 std::to_string(captureElapsedMilliseconds));
            }
            m_LptStore.setListenerStatus(LptListenerStatus::Idle);
            return transmissionBuffer;
          }

          if (!m_SerialPort.IsOpen()) {
            m_IsSerialPortOpen = false;
            if (m_Log) {
              m_Log->warning("[LptListener Warning]: Serial port closed during read timeout. Reopening.");
            }
            ensureSerialPortOpen(serialSettings);
          }
          continue;
        }

        if (!m_SerialPort.IsOpen()) {
          m_IsSerialPortOpen = false;
          if (m_Log) {
            m_Log->warning("[LptListener Warning]: Serial port closed during read. Reopening.");
          }
          ensureSerialPortOpen(serialSettings);
          continue;
        }

        throw std::runtime_error("[LptManager Error]: Serial read failed on device '" + serialSettings.devicePath +
                                 "'. Reason: " + readErrorReason);
      }
      const std::size_t bytesRead = chunkBuffer.size();

      if (bytesRead > 0) {
        if (!hasCapturedData && m_Log) {
          captureStartTimestamp = std::chrono::steady_clock::now();
          m_Log->information("[LptListener Info]: Byte capture started.");
        }
        if (!hasCapturedData) {
          m_LptStore.setListenerStatus(LptListenerStatus::CaptureStarted);
        }
        hasCapturedData = true;
        lastCapturedDataTimestamp = std::chrono::steady_clock::now();
        transmissionBuffer.reserve(transmissionBuffer.size() + bytesRead);
        for (std::size_t chunkByteIndex = 0; chunkByteIndex < bytesRead; ++chunkByteIndex) {
          transmissionBuffer.push_back(static_cast<std::uint8_t>(chunkBuffer[chunkByteIndex]));
        }
        continue;
      }

      if (hasCapturedData &&
          (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= serialSettings.silenceTimeout) {
        if (m_Log) {
          const auto captureElapsedMilliseconds =
              std::chrono::duration_cast<std::chrono::milliseconds>(lastCapturedDataTimestamp - captureStartTimestamp)
                  .count();
          m_Log->information("[LptListener Info]: Byte capture ended. Total bytes: " +
                             std::to_string(transmissionBuffer.size()) + ", elapsedMs: " +
                             std::to_string(captureElapsedMilliseconds));
        }
        m_LptStore.setListenerStatus(LptListenerStatus::Idle);
        return transmissionBuffer;
      }

      if (!m_SerialPort.IsOpen()) {
        m_IsSerialPortOpen = false;
        if (m_Log) {
          m_Log->warning("[LptListener Warning]: Serial port is no longer open. Reopening.");
        }
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

  void test() override {
    const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();
    ensureSerialPortOpen(serialSettings);

    try {
      const char testByte = 't';
      m_SerialPort.WriteByte(testByte);
      if (m_Log) {
        m_Log->information("[LptListener Info]: Wrote test byte 't' to serial port.");
      }
    } catch (const std::exception& writeException) {
      throw std::runtime_error("[LptManager Error]: Failed to write test byte to serial device '" +
                               serialSettings.devicePath + "'. Reason: " + writeException.what());
    }
  }

private:
  void ensureSerialPortOpen(const SerialSettings& serialSettings) {
    const bool shouldReopenPort = (!m_IsSerialPortOpen || m_OpenDevicePath != serialSettings.devicePath);
    if (!shouldReopenPort) {
      return;
    }

    closeSerialPortIfOpen();

    try {
      if (m_Log) {
        m_Log->information("[LptListener Info]: Opening serial device: " + serialSettings.devicePath);
      }
      m_SerialPort.Open(serialSettings.devicePath);
      m_SerialPort.SetBaudRate(toBaudRate(serialSettings.baudRate));
      m_IsSerialPortOpen = true;
      m_OpenDevicePath = serialSettings.devicePath;
      if (m_Log) {
        m_Log->information("[LptListener Info]: Serial device opened successfully.");
      }
    } catch (const std::exception& openException) {
      std::string errorReason = openException.what();
      if (errorReason.find("busy") != std::string::npos || errorReason.find("Device or resource busy") != std::string::npos) {
        errorReason += " The serial device may already be open by another process.";
      }
      throw std::runtime_error(
          "[LptManager Error]: Failed to open serial device '" + serialSettings.devicePath + "'. Reason: " + errorReason);
    }
  }

  void closeSerialPortIfOpen() {
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

  const ISettingsRepository& m_SettingsRepository;
  ILptStore& m_LptStore;
  SharedLogger m_Log;
  LibSerial::SerialPort m_SerialPort;
  bool m_IsSerialPortOpen{false};
  std::string m_OpenDevicePath;
};

class PrnPatcher final : public IPrnPatcher {
public:
  using PatchGenerator = std::function<std::string(const VehicleSelection&)>;

  PrnPatcher(const ISelectedVehicleStore& selectedVehicleStore, SharedLogger log)
      : m_SelectedVehicleStore(selectedVehicleStore), m_Log(std::move(log)) {}

  void addPatch(std::size_t patchOffset, PatchGenerator patchGenerator) {
    m_Patches.emplace_back(patchOffset, std::move(patchGenerator));
  }

  std::vector<std::uint8_t> patch(const std::vector<std::uint8_t>& inputBytes) override {
    if (m_Log) {
      m_Log->information("[PrnPatcher Info]: Applying PRN patches to incoming bytes.");
    }
    std::vector<std::uint8_t> patchedOutputBytes = inputBytes;
    const VehicleSelection selectedVehicle = m_SelectedVehicleStore.getSelectedVehicle();

    for (const auto& [patchOffset, patchGenerator] : m_Patches) {
      const std::string replacementText = patchGenerator(selectedVehicle);
      if (patchOffset >= patchedOutputBytes.size()) {
        continue;
      }

      const auto replacementByteCount = std::min(replacementText.size(), patchedOutputBytes.size() - patchOffset);
      for (std::size_t byteIndex = 0; byteIndex < replacementByteCount; ++byteIndex) {
        patchedOutputBytes[patchOffset + byteIndex] = static_cast<std::uint8_t>(replacementText[byteIndex]);
      }
    }

    return patchedOutputBytes;
  }

private:
  const ISelectedVehicleStore& m_SelectedVehicleStore;
  std::vector<std::pair<std::size_t, PatchGenerator>> m_Patches;
  SharedLogger m_Log;
};

class PrnRenderer final : public IPrnRenderer {
public:
  explicit PrnRenderer(SharedLogger log) : m_Log(std::move(log)) {}

  std::vector<RenderedPage> render(const std::vector<std::uint8_t>& patchedBytes) override {
    if (m_Log) {
      m_Log->information("[PrnRenderer Info]: Rendering patched bytes into page buffer.");
    }
    RenderedPage renderedPage;
    renderedPage.pageIndex = 0;
    renderedPage.width = 1;
    renderedPage.height = patchedBytes.size();
    renderedPage.pixels = patchedBytes;
    return {std::move(renderedPage)};
  }

private:
  SharedLogger m_Log;
};

class RenderedDocumentWriter final : public IRenderedDocumentWriter {
public:
  RenderedDocumentWriter(std::filesystem::path outputDirectory, SharedLogger log)
      : m_OutputDirectory(std::move(outputDirectory)), m_Log(std::move(log)) {}

  void writePages(const std::vector<RenderedPage>& pages, const std::string& documentId) override {
    if (m_Log) {
      m_Log->information("[RenderedDocumentWriter Info]: Writing rendered pages to disk.");
    }
    std::filesystem::create_directories(m_OutputDirectory);

    for (const auto& renderedPage : pages) {
      std::ostringstream filenameStream;
      filenameStream << documentId << "_page_" << renderedPage.pageIndex << ".bin";
      const std::filesystem::path fullPath = m_OutputDirectory / filenameStream.str();

      std::ofstream outputStream(fullPath, std::ios::binary);
      outputStream.write(reinterpret_cast<const char*>(renderedPage.pixels.data()),
                         static_cast<std::streamsize>(renderedPage.pixels.size()));
    }
  }

private:
  std::filesystem::path m_OutputDirectory;
  SharedLogger m_Log;
};

class PrnWriter final : public IPrnWriter {
public:
  PrnWriter(std::filesystem::path rootDirectory, SharedLogger log)
      : m_RootDirectory(std::move(rootDirectory)), m_Log(std::move(log)) {}

  void writePrn(const std::vector<std::uint8_t>& patchedBytes, const std::string& filenameWithoutExtension) override {
    if (filenameWithoutExtension.empty()) {
      if (m_Log) {
        m_Log->warning("[PrnWriter Warning]: No filename provided for prn output.");
      }
      return;
    }

    std::filesystem::path relativePath(filenameWithoutExtension);
    relativePath += ".prn";
    const auto fullPath = m_RootDirectory / relativePath;
    std::filesystem::create_directories(fullPath.parent_path());

    std::ofstream outputStream(fullPath, std::ios::binary);
    outputStream.write(reinterpret_cast<const char*>(patchedBytes.data()),
                       static_cast<std::streamsize>(patchedBytes.size()));
    if (m_Log) {
      m_Log->information("[PrnWriter Info]: Wrote prn file to " + fullPath.string());
    }
  }

private:
  std::filesystem::path m_RootDirectory;
  SharedLogger m_Log;
};

} // namespace brake_tester
