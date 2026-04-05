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

namespace brake_tester {

inline LibSerial::BaudRate toBaudRate(std::uint32_t baud_rate) {
  switch (baud_rate) {
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
  explicit LptListener(const ISettingsRepository& settings_repository)
      : settings_repository_(settings_repository) {}

  std::vector<std::uint8_t> captureTransmission() override {
    const SerialSettings settings = settings_repository_.getSerialSettings();

    std::vector<std::uint8_t> buffer;
    buffer.reserve(2048);

    LibSerial::SerialPort serial_port;
    serial_port.Open(settings.device_path);
    serial_port.SetBaudRate(toBaudRate(settings.baud_rate));

    bool seen_data = false;
    auto last_data_time = std::chrono::steady_clock::now();

    while (true) {
      std::vector<std::uint8_t> chunk(settings.read_chunk_size, 0);
      std::size_t bytes_read = 0;
      serial_port.Read(chunk.data(), chunk.size(), 25, bytes_read);

      if (bytes_read > 0) {
        seen_data = true;
        last_data_time = std::chrono::steady_clock::now();
        chunk.resize(bytes_read);
        buffer.insert(buffer.end(), chunk.begin(), chunk.end());
      } else if (seen_data && (std::chrono::steady_clock::now() - last_data_time) >= settings.silence_timeout) {
        break;
      }
    }

    serial_port.Close();
    return buffer;
  }

private:
  const ISettingsRepository& settings_repository_;
};

class PrnPatcher final : public IPrnPatcher {
public:
  using PatchGenerator = std::function<std::string(const VehicleSelection&)>;

  explicit PrnPatcher(const ISelectedVehicleStore& selected_vehicle_store)
      : selected_vehicle_store_(selected_vehicle_store) {}

  void addPatch(std::size_t offset, PatchGenerator generator) {
    patches_.emplace_back(offset, std::move(generator));
  }

  std::vector<std::uint8_t> patch(const std::vector<std::uint8_t>& input_bytes) override {
    std::vector<std::uint8_t> output = input_bytes;
    const VehicleSelection selected_vehicle = selected_vehicle_store_.getSelectedVehicle();

    for (const auto& [offset, generator] : patches_) {
      const std::string replacement = generator(selected_vehicle);
      if (offset >= output.size()) {
        continue;
      }

      const auto replace_count = std::min(replacement.size(), output.size() - offset);
      for (std::size_t i = 0; i < replace_count; ++i) {
        output[offset + i] = static_cast<std::uint8_t>(replacement[i]);
      }
    }

    return output;
  }

private:
  const ISelectedVehicleStore& selected_vehicle_store_;
  std::vector<std::pair<std::size_t, PatchGenerator>> patches_;
};

class PrnRenderer final : public IPrnRenderer {
public:
  std::vector<RenderedPage> render(const std::vector<std::uint8_t>& patched_bytes) override {
    RenderedPage page;
    page.page_index = 0;
    page.width = 1;
    page.height = patched_bytes.size();
    page.pixels = patched_bytes;
    return {std::move(page)};
  }
};

class RenderedDocumentWriter final : public IRenderedDocumentWriter {
public:
  explicit RenderedDocumentWriter(std::filesystem::path output_directory)
      : output_directory_(std::move(output_directory)) {}

  void writePages(const std::vector<RenderedPage>& pages, const std::string& document_id) override {
    std::filesystem::create_directories(output_directory_);

    for (const auto& page : pages) {
      std::ostringstream filename;
      filename << document_id << "_page_" << page.page_index << ".bin";
      const std::filesystem::path full_path = output_directory_ / filename.str();

      std::ofstream stream(full_path, std::ios::binary);
      stream.write(reinterpret_cast<const char*>(page.pixels.data()), static_cast<std::streamsize>(page.pixels.size()));
    }
  }

private:
  std::filesystem::path output_directory_;
};

} // namespace brake_tester
