//
// Created by didal on 06/09/2025.
//

#ifndef BRAKETESTERRESULTMEMORYWRITER_FRAME_H
#define BRAKETESTERRESULTMEMORYWRITER_FRAME_H
#include <vector>
#include <cstdint>
#include <variant>
#include <functional>

namespace BrakeTester
{
    enum class FrameTypes : uint8_t
    {
        RegisterReadRequest = 0x82,
        RegisterReadReply = 0x83,
        RegisterWriteRequest = 0x84,
        RegisterWriteReply = 0x85
    };

#pragma pack(push, 1)
    struct FrameHeader
    {
        uint8_t startByte;
        uint8_t lengthByte;
        FrameTypes frameType;
        uint8_t dataLength;
        uint32_t reg;
    };
    struct FrameFooter
    {
        uint8_t endByte;
        uint8_t crc;
    };

#pragma pack(pop)

    struct Frame
    {
        FrameHeader header;
        std::vector<uint8_t> payload;
        FrameFooter footer;
    };


    struct ReadRequestFrame : public Frame
    {
        static constexpr auto c_FrameType{FrameTypes::RegisterReadRequest};

       static ReadRequestFrame ReadRequestFrameFactory(const uint32_t addr, const uint8_t size)
        {
            return {FrameHeader{0x2, static_cast<uint8_t>(sizeof(FrameHeader) + sizeof(FrameFooter)) - 2, c_FrameType, size, addr},
                {},
                FrameFooter{0x3, 0x00} };
        }
    };

    struct WriteRequestFrame : public Frame
    {
        static constexpr auto c_FrameType{FrameTypes::RegisterWriteRequest};
        static WriteRequestFrame WriteRequestFrameFactory(const uint32_t addr, const std::span<const uint8_t> payload)
        {
            WriteRequestFrame frame{
                FrameHeader{0x2, static_cast<uint8_t>(sizeof(FrameHeader) + payload.size()), c_FrameType, static_cast<uint8_t>(payload.size()), addr},
                {},
                FrameFooter{0x3, 0x00} };

            frame.payload = std::vector<uint8_t>(payload.begin(), payload.end());
            return frame;
        }
    };

    struct ReadReplyFrame : public Frame
    {
        static constexpr auto c_FrameType{FrameTypes::RegisterReadReply};
        static WriteRequestFrame ReadReplyFrameFactory(const uint32_t addr, const std::span<const uint8_t> payload)
        {
            WriteRequestFrame frame{
                FrameHeader{0x2, static_cast<uint8_t>(sizeof(FrameHeader) + payload.size()), c_FrameType, static_cast<uint8_t>(payload.size()), addr},
                {},
                FrameFooter{0x3, 0x00} };

            frame.payload = std::vector<uint8_t>(payload.begin(), payload.end());
            return frame;
        }
    };

    struct WriteReplyFrame : public Frame
    {
        static constexpr auto c_FrameType{FrameTypes::RegisterWriteReply};
        static ReadRequestFrame WriteReplyFrameFactory(const uint32_t addr, const uint8_t size)
        {
            return {FrameHeader{0x2, static_cast<uint8_t>(sizeof(FrameHeader) + sizeof(FrameFooter)) - 2, c_FrameType, size, addr},
                {},
                FrameFooter{0x3, 0x00} };
        }
    };

    using FrameVariant = std::variant<
        ReadReplyFrame,
        WriteReplyFrame,
        ReadRequestFrame,
        WriteRequestFrame
    >;

    struct FrameHandlers
    {
        std::function<void(const ReadReplyFrame &)> onReadReply = [](const ReadReplyFrame &) {};
        std::function<void(const WriteReplyFrame &)> onWriteReply = [](const WriteReplyFrame &) {};
        std::function<void(const ReadRequestFrame &)> onReadRequest = [](const ReadRequestFrame &) {};
        std::function<void(const WriteRequestFrame &)> onWriteRequest = [](const WriteRequestFrame &) {};

        FrameHandlers(
        std::function<void(const ReadReplyFrame &)> readReply = [](const ReadReplyFrame &) {},
        std::function<void(const WriteReplyFrame &)> writeReply = [](const WriteReplyFrame &) {},
        std::function<void(const ReadRequestFrame &)> readRequest = [](const ReadRequestFrame &) {},
        std::function<void(const WriteRequestFrame &)> writeRequest = [](const WriteRequestFrame &) {})
        : onReadReply(std::move(readReply)),
          onWriteReply(std::move(writeReply)),
          onReadRequest(std::move(readRequest)),
          onWriteRequest(std::move(writeRequest))
        {}

        void operator ()(const ReadReplyFrame &frame) const { onReadReply(frame); }
        void operator ()(const WriteReplyFrame &frame) const { onWriteReply(frame); }
        void operator ()(const ReadRequestFrame &frame) const { onReadRequest(frame); }
        void operator ()(const WriteRequestFrame &frame) const { onWriteRequest(frame); }
    };
} // BrakeTester
#endif //BRAKETESTERRESULTMEMORYWRITER_FRAME_H
