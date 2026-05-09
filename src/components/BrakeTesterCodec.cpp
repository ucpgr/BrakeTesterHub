#include "brake_tester/BrakeTester/Codec.hpp"

#include "brake_tester/BrakeTester/Decoder.h"

#include <cstring>
#include <variant>
#include <numeric>
#include <stdexcept>

namespace BrakeTester {

uint8_t calculateChecksum(const std::span<const uint8_t> bytes) {
  return std::accumulate(bytes.begin(), bytes.end(), uint8_t{0}, std::plus<uint8_t>{});
}

std::vector<uint8_t> serializeFrame(const Frame& frame) {
  std::vector<uint8_t> out;
  const std::size_t totalSize = sizeof(FrameHeader) + frame.payload.size() + sizeof(FrameFooter);

  out.reserve(totalSize);

  const auto* headerBytes = reinterpret_cast<const uint8_t*>(&frame.header);
  out.insert(out.end(), headerBytes, headerBytes + sizeof(FrameHeader));

  out.insert(out.end(), frame.payload.begin(), frame.payload.end());

  const auto* footerBytes = reinterpret_cast<const uint8_t*>(&frame.footer);
  out.insert(out.end(), footerBytes, footerBytes + sizeof(FrameFooter));

  if (!out.empty()) {
    out.back() = calculateChecksum(std::span<const uint8_t>(out.data(), out.size() - 1));
  }

  return out;
}

Frame decodeFrame(const std::span<const uint8_t> frameBytes) {
  constexpr std::size_t kMinFrameSize = sizeof(FrameHeader) + sizeof(FrameFooter);
  if (frameBytes.size() < kMinFrameSize) {
    throw std::runtime_error("BrakeTester frame too short.");
  }

  const auto expectedChecksum = calculateChecksum(frameBytes.subspan(0, frameBytes.size() - 1));
  if (expectedChecksum != frameBytes.back()) {
    throw std::runtime_error("BrakeTester frame checksum invalid.");
  }

  Decoder decoder;
  const FrameVariant decodedVariant = decoder(frameBytes);

  return std::visit([](const auto& typedFrame) -> Frame {
    return Frame{typedFrame.header, typedFrame.payload, typedFrame.footer};
  }, decodedVariant);
}

} // namespace BrakeTester
