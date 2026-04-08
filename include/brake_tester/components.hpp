#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

  void render(const std::filesystem::path& prnFilePath) override {
    const std::filesystem::path pdfFolder = std::filesystem::path("tests") / "pdf";
    cleanupPdfFolder(pdfFolder);

    const std::string prnFilePathString = prnFilePath.string();
    const std::string renderCommand =
        "printerToPDF -8 -o tests/ -f /opt/font2/Epson-PC437-US.C16 " + shellQuote(prnFilePathString);
    runCommand(renderCommand, "[PrnRenderer Error]: Failed to render prn with printerToPDF.");

    const auto pagePdfPaths = collectGeneratedPages(pdfFolder);
    if (pagePdfPaths.empty()) {
      throw std::runtime_error("[PrnRenderer Error]: No page PDFs generated in tests/pdf.");
    }

    std::string mergeCommand = "pdfunite";
    for (const auto& pagePdfPath : pagePdfPaths) {
      mergeCommand += " " + shellQuote(pagePdfPath.string());
    }
    mergeCommand += " " + shellQuote(prnFilePathString + ".pdf");
    runCommand(mergeCommand, "[PrnRenderer Error]: Failed to merge rendered PDFs with pdfunite.");

    cleanupPdfFolder(pdfFolder);
  }

private:
  SharedLogger m_Log;

  static std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char character : value) {
      if (character == '\'') {
        quoted += "'\\''";
      } else {
        quoted += character;
      }
    }
    quoted += "'";
    return quoted;
  }

  static std::vector<std::filesystem::path> collectGeneratedPages(const std::filesystem::path& pdfFolder) {
    if (!std::filesystem::exists(pdfFolder) || !std::filesystem::is_directory(pdfFolder)) {
      return {};
    }

    std::vector<std::filesystem::path> pdfPages;
    for (const auto& entry : std::filesystem::directory_iterator(pdfFolder)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      if (entry.path().extension() == ".pdf") {
        pdfPages.push_back(entry.path());
      }
    }
    std::sort(pdfPages.begin(), pdfPages.end());
    return pdfPages;
  }

  static void cleanupPdfFolder(const std::filesystem::path& pdfFolder) {
    std::error_code removalError;
    std::filesystem::remove_all(pdfFolder, removalError);
  }

  void runCommand(const std::string& command, const std::string& failureMessage) const {
    if (m_Log) {
      m_Log->information("[PrnRenderer Info]: Executing command: " + command);
    }
    const int commandExitCode = std::system(command.c_str());
    if (commandExitCode != 0) {
      throw std::runtime_error(failureMessage + " Exit code: " + std::to_string(commandExitCode));
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
