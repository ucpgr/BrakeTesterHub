#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
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
  LptListener(const ISettingsRepository& settingsRepository, SharedLogger log)
      : m_SettingsRepository(settingsRepository), m_Log(std::move(log)) {}

  std::vector<std::uint8_t> captureTransmission() override {
    const SerialSettings serialSettings = m_SettingsRepository.getSerialSettings();

    std::vector<std::uint8_t> transmissionBuffer;
    transmissionBuffer.reserve(2048);

    LibSerial::SerialPort serialPort;
    if (serialSettings.devicePath.empty() && m_Log) {
      m_Log->Critical("Serial port path is empty");
    }
    serialPort.Open(serialSettings.devicePath);
    serialPort.SetBaudRate(toBaudRate(serialSettings.baudRate));

    bool hasSeenData = false;
    auto lastDataTimestamp = std::chrono::steady_clock::now();

    while (true) {
      LibSerial::DataBuffer chunkBuffer;
      serialPort.Read(chunkBuffer, serialSettings.readChunkSize, 25);
      const std::size_t bytesRead = chunkBuffer.size();

      if (bytesRead > 0) {
        hasSeenData = true;
        lastDataTimestamp = std::chrono::steady_clock::now();
        transmissionBuffer.reserve(transmissionBuffer.size() + bytesRead);
        for (std::size_t chunkByteIndex = 0; chunkByteIndex < bytesRead; ++chunkByteIndex) {
          transmissionBuffer.push_back(static_cast<std::uint8_t>(chunkBuffer[chunkByteIndex]));
        }
      } else if (hasSeenData &&
                 (std::chrono::steady_clock::now() - lastDataTimestamp) >= serialSettings.silenceTimeout) {
        break;
      }
    }

    serialPort.Close();
    return transmissionBuffer;
  }

private:
  const ISettingsRepository& m_SettingsRepository;
  SharedLogger m_Log;
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

} // namespace brake_tester
