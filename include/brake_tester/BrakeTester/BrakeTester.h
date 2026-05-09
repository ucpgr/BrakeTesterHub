//
// Created by didal on 06/09/2025.
//

#ifndef BRAKETESTERRESULTMEMORYWRITER_BRAKETESTER_H
#define BRAKETESTERRESULTMEMORYWRITER_BRAKETESTER_H

#include <iostream>
#include <istream>
#include <numeric>
#include <optional>

#include "Framer.h"
#include "Decoder.h"
#include "Dispatcher.h"

namespace BrakeTester
{
    class BrakeTester
    {
        static uint8_t calculateChecksum(std::span<const uint8_t> data)
        {
            return std::accumulate(data.begin(), data.end(), uint8_t{0}, std::plus<uint8_t>{});
        }




        Framer m_Framer{calculateChecksum};
        Decoder m_Decoder{};
        Dispatcher m_Dispatcher{};

    public:
        BrakeTester() = delete;
        explicit BrakeTester(const Dispatcher &dispatcher) : m_Dispatcher{dispatcher} {}

        void resetFramer()
        {
            m_Framer.reset();
        }

        void update(const std::function<int64_t(uint8_t *, std::size_t)> &producer)
        {
            std::vector<uint8_t> buffer(1024);

            if (const int64_t bytesRead = producer(reinterpret_cast<uint8_t *>(buffer.data()), static_cast<std::size_t>(buffer.size())); bytesRead > 0)
            {
                m_Framer.consume({buffer.data(), static_cast<std::size_t>(bytesRead)});
                for(int tryCount{0}; tryCount < 16; tryCount++)
                {
                    auto frameData = m_Framer.tryGetFrame();
                    if (frameData.empty())
                        break;

                    try
                    {
                        auto frame = m_Decoder(frameData);
                        m_Dispatcher(frame);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << e.what() << std::endl;
                        return;
                    }
                }
            }
        }
    };
} // BrakeTester

#endif //BRAKETESTERRESULTMEMORYWRITER_BRAKETESTER_H
