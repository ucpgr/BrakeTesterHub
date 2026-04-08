#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <libserial/SerialPort.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"
#include "brake_tester/ESCP2Renderer.h"

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
    const auto endOfTransmissionSilenceTimeout = serialSettings.silenceTimeout;
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
      std::uint8_t capturedByte = 0;
      bool didReadByte = false;
      try {
        char readByte = '\0';
        m_SerialPort.ReadByte(readByte, serialSettings.silenceTimeout.count());
        capturedByte = static_cast<std::uint8_t>(readByte);
        didReadByte = true;
      } catch (const std::exception& readException) {
        const std::string readErrorReason = readException.what();
        const bool isReadTimeout =
            (readErrorReason.find("timeout") != std::string::npos || readErrorReason.find("Timeout") != std::string::npos);

        if (isReadTimeout) {
          if (hasCapturedData &&
              (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= endOfTransmissionSilenceTimeout) {
            if (m_Log) {
              const auto captureElapsedMilliseconds =
                  std::chrono::duration_cast<std::chrono::milliseconds>(lastCapturedDataTimestamp - captureStartTimestamp)
                      .count();
              m_Log->information("[LptListener Info]: Byte capture ended. Total bytes: " +
                                 std::to_string(transmissionBuffer.size()) + ", elapsedMs: " +
                                 std::to_string(captureElapsedMilliseconds) + ", endSilenceTimeoutMs: " +
                                 std::to_string(endOfTransmissionSilenceTimeout.count()));
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
      const std::size_t bytesRead = didReadByte ? 1 : 0;

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
        transmissionBuffer.push_back(capturedByte);
        continue;
      }

      if (hasCapturedData &&
          (std::chrono::steady_clock::now() - lastCapturedDataTimestamp) >= endOfTransmissionSilenceTimeout) {
        if (m_Log) {
          const auto captureElapsedMilliseconds =
              std::chrono::duration_cast<std::chrono::milliseconds>(lastCapturedDataTimestamp - captureStartTimestamp)
                  .count();
          m_Log->information("[LptListener Info]: Byte capture ended. Total bytes: " +
                             std::to_string(transmissionBuffer.size()) + ", elapsedMs: " +
                             std::to_string(captureElapsedMilliseconds) + ", endSilenceTimeoutMs: " +
                             std::to_string(endOfTransmissionSilenceTimeout.count()));
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
    std::vector<RenderedPage> pages;
    std::vector<std::vector<std::uint8_t>> currentPageLines;
    std::optional<std::size_t> lastAxleSplitLineIndex;

    const auto lines = splitIntoLines(patchedBytes);

    for (const auto& line : lines) {
      if (isAxleStartLine(line) && !currentPageLines.empty()) {
        lastAxleSplitLineIndex = currentPageLines.size();
      }

      currentPageLines.push_back(line);

      if (measureCursorY(currentPageLines) <= kPageMaxHeightPixels) {
        continue;
      }

      if (lastAxleSplitLineIndex.has_value() && *lastAxleSplitLineIndex > 0) {
        const std::size_t splitLineIndex = *lastAxleSplitLineIndex;
        const std::vector<std::vector<std::uint8_t>> finishedPageLines(
            currentPageLines.begin(),
            currentPageLines.begin() + static_cast<std::ptrdiff_t>(splitLineIndex));
        pages.push_back(renderPage(finishedPageLines, pages.size()));

        std::vector<std::vector<std::uint8_t>> overflowLines(
            currentPageLines.begin() + static_cast<std::ptrdiff_t>(splitLineIndex),
            currentPageLines.end());
        currentPageLines = std::move(overflowLines);
      } else {
        pages.push_back(renderPage(currentPageLines, pages.size()));
        currentPageLines.clear();
      }

      lastAxleSplitLineIndex = findLastAxleSplitLineIndex(currentPageLines);
    }

    if (!currentPageLines.empty()) {
      pages.push_back(renderPage(currentPageLines, pages.size()));
    }

    if (pages.empty()) {
      pages.push_back(renderPage({}, 0));
    }

    return pages;
  }

private:
  static constexpr std::size_t kPageMaxHeightPixels = 800;
  SharedLogger m_Log;

  static std::vector<std::vector<std::uint8_t>> splitIntoLines(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::vector<std::uint8_t>> lines;
    std::vector<std::uint8_t> currentLine;
    currentLine.reserve(256);

    for (const auto byte : bytes) {
      currentLine.push_back(byte);
      if (byte == 0x0A) {
        lines.push_back(std::move(currentLine));
        currentLine.clear();
      }
    }

    if (!currentLine.empty()) {
      lines.push_back(std::move(currentLine));
    }

    return lines;
  }

  static bool isAxleStartLine(const std::vector<std::uint8_t>& line) {
    return line.size() >= 2 && line[0] == 0x1B && line[1] == 0x45;
  }

  static std::size_t measureCursorY(const std::vector<std::vector<std::uint8_t>>& lines) {
    ESCP2Renderer measurementRenderer([](size_t, size_t, uint8_t) {});
    for (const auto& line : lines) {
      measurementRenderer.addBytes(line.begin(), line.end());
    }
    return measurementRenderer.cursorY();
  }

  static std::optional<std::size_t> findLastAxleSplitLineIndex(const std::vector<std::vector<std::uint8_t>>& lines) {
    std::optional<std::size_t> splitLineIndex;
    for (std::size_t lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
      if (isAxleStartLine(lines[lineIndex])) {
        splitLineIndex = lineIndex;
      }
    }
    return splitLineIndex;
  }

  static RenderedPage renderPage(const std::vector<std::vector<std::uint8_t>>& lines, std::size_t pageIndex) {
    RenderedPage renderedPage;
    renderedPage.pageIndex = pageIndex;

    ESCP2Renderer escRenderer([&renderedPage](size_t x, size_t y, uint8_t colour) {
      setPixelPacked(renderedPage.pixels, 1088, static_cast<int>(x), static_cast<int>(y), (colour == 0));
    });

    for (const auto& line : lines) {
      escRenderer.addBytes(line.begin(), line.end());
    }

    return renderedPage;
  }


  static void setPixelPacked(std::vector<uint8_t>& buffer,
                      uint16_t width,
                      int x,
                      int y,
                      bool set)
  {
    if (x < 0 || y < 0 || width == 0) {
      return;
    }

    const int bytesPerRow = (width + 7) / 8;

    if (x >= width) {
      return;
    }

    // Ensure buffer is large enough for row y
    const size_t requiredSize = static_cast<size_t>(y + 1) * bytesPerRow;
    if (buffer.size() < requiredSize) {
      buffer.resize(requiredSize, 0xFF);
    }

    const int byteIndex = x / 8;
    const int bitIndex  = x % 8;

    const size_t offset = static_cast<size_t>(y) * bytesPerRow + byteIndex;

    const uint8_t mask = static_cast<uint8_t>(1u << bitIndex);

    if (set) {
      buffer[offset] &= static_cast<uint8_t>(~mask);
    }
  }
};

class RenderedDocumentWriter final : public IRenderedDocumentWriter {
public:
  RenderedDocumentWriter(std::filesystem::path outputDirectory, SharedLogger log)
      : m_OutputDirectory(std::move(outputDirectory)), m_Log(std::move(log)) {}

  void writePages(const std::vector<RenderedPage>& pages, const std::string& documentId) override {
    for (const auto& renderedPage : pages) {
      std::ostringstream relativePathStream;
      relativePathStream << documentId << "_" << renderedPage.pageIndex << ".bin";
      const std::filesystem::path fullPath = m_OutputDirectory / relativePathStream.str();
      std::filesystem::create_directories(fullPath.parent_path());

      if (m_Log) {
        m_Log->information("[RenderedDocumentWriter Info]: Writing rendered pages at: " + fullPath.string());
      }

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
