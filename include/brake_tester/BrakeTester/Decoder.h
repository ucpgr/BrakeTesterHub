//
// Created by didal on 06/09/2025.
//

#ifndef BRAKETESTERRESULTMEMORYWRITER_DECODER_H
#define BRAKETESTERRESULTMEMORYWRITER_DECODER_H

#include <span>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "Frame.h"

namespace BrakeTester
{
    class Decoder
    {
    public:
        FrameVariant operator ()(std::span<const uint8_t> data)
        {
            if (data.size() < 10 || data[1] > data.size() - 2)
                throw(std::runtime_error("Decoder Error"));


            FrameHeader header{};
            FrameFooter footer{};

            std::memcpy(&header, data.data(), sizeof(FrameHeader));
            std::memcpy(&footer, data.subspan(header.lengthByte, 2).data(), sizeof(FrameFooter));

            if (header.startByte != 0x2 || footer.endByte != 0x3)
                throw (std::runtime_error("Decoder Error"));

            switch (header.frameType)
            {
                case FrameTypes::RegisterReadRequest:
                {
                    ReadRequestFrame result{header, {}, footer};
                    return result;
                }
                case FrameTypes::RegisterWriteRequest:
                {
                    auto payloadSpan = data.subspan(sizeof(FrameHeader), header.dataLength);
                    WriteRequestFrame result{header, {payloadSpan.begin(), payloadSpan.end()}, footer};
                    return result;
                }
                case FrameTypes::RegisterReadReply:
                {
                    auto payloadSpan = data.subspan(sizeof(FrameHeader), header.dataLength);
                    ReadReplyFrame result{header, {payloadSpan.begin(), payloadSpan.end()}, footer};
                    return result;
                }
                case FrameTypes::RegisterWriteReply:
                {
                    WriteReplyFrame result{header, {}, footer};
                    return result;
                }
                default:
                        throw(std::runtime_error("Decoder Error") );
            }
        }
    };

}
#endif //BRAKETESTERRESULTMEMORYWRITER_DECODER_H