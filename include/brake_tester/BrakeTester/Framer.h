//
// Created by didal on 06/09/2025.
//

#ifndef BRAKETESTERRESULTMEMORYWRITER_FRAMER_H
#define BRAKETESTERRESULTMEMORYWRITER_FRAMER_H

#include <algorithm>
#include <span>
#include <cstdint>
#include <vector>
#include <functional>

namespace BrakeTester
{
    using ChecksumCallable = std::function<uint8_t(std::span<const uint8_t>)>;

    class Framer
    {
        static constexpr uint8_t FrameStartOpCode  {0x02};
        static constexpr uint8_t FrameEndOpCode    {0x03};

        std::vector<uint8_t> m_Buffer{};
        ChecksumCallable m_ChecksumCalculator;

    public:
        Framer() = delete;
        explicit Framer(ChecksumCallable checksumCalculator) : m_ChecksumCalculator{checksumCalculator} {}
        ~Framer() = default;


        void consume(std::span<const uint8_t> data)
        {
            m_Buffer.insert(m_Buffer.end(), data.begin(), data.end());
        }


        std::vector<uint8_t> tryGetFrame()
        {
            std::vector<uint8_t> frame;
            while (tryGetFrameSingle(frame) && frame.empty()) {}

            return frame;
        }

        void reset()
        {
            m_Buffer.clear();
        }

    private:
        bool isValidFrame(const std::span<const uint8_t> frameData)
        {
            if (frameData.size() < 10)
                return false;

            if (frameData.front() != FrameStartOpCode)
                return false;

            if (frameData[1] + 2 != frameData.size())
                return false;

            if (frameData[frameData.size() - 2] != FrameEndOpCode)
                return false;

            auto checksum = m_ChecksumCalculator(frameData.subspan(0, frameData.size() - 1));
            if (checksum != frameData.back())
                return false;

            return true;
        }
/*

        bool tryGetFrameSingle(std::vector<uint8_t> &resultData)
        {
            auto startOpCodeIt = std::ranges::find(m_Buffer, FrameStartOpCode);
            if (startOpCodeIt == m_Buffer.end())
            {
                m_Buffer.clear();
                return false;
            }

            if (startOpCodeIt != m_Buffer.begin())
                m_Buffer.erase(m_Buffer.begin(), startOpCodeIt);

            if (m_Buffer.size() < 10)
                return false;

            if (m_Buffer[1] + 2 > m_Buffer.size())
            {
                for (auto it = std::next(startOpCodeIt); (it = std::find(it, m_Buffer.end(), FrameStartOpCode)) != m_Buffer.end(); std::advance(it, 1))
                {
                    if (auto nextIt = std::next(it, 1); *nextIt <= m_Buffer.size() - std::distance(m_Buffer.begin(), it))
                    {
                        if (isValidFrame({it, it + *nextIt}))
                        {
                            m_Buffer.erase(m_Buffer.begin());
                            return true;
                        }
                    }
                }
            }

            const auto frameEndIt = std::next(m_Buffer.begin(),m_Buffer[1] + 2);
            std::span<const uint8_t> frameData{m_Buffer.begin(), frameEndIt};

            if (!isValidFrame(frameData))
            {
                m_Buffer.erase(m_Buffer.begin()); // drop start byte and try again
                return true; // spin
            }

            resultData.clear();
            std::copy(m_Buffer.begin(), frameEndIt, std::back_inserter(resultData));
            m_Buffer.erase(m_Buffer.begin(), frameEndIt);

            return true;
        }*/

static constexpr std::size_t kMinTotal = 10;   // 1+1+1+1+4+0+1+1
    std::size_t m_MaxTotalPolicy = 257;            // configurable (<= 257)

    // Optional: expose a setter/ctor param to tweak m_MaxTotalPolicy for tests.
    // explicit Framer(ChecksumCallable fn, std::size_t maxTotal = 257)
    //   : m_ChecksumCalculator(fn), m_MaxTotalPolicy(maxTotal) {}

    bool tryGetFrameSingle(std::vector<uint8_t>& out)
    {
        // Helper lambdas that DO NOT mutate the buffer
        auto total_len_at = [&](std::size_t pos) -> std::optional<std::size_t> {
            if (m_Buffer.size() - pos < kMinTotal) return std::nullopt;
            const auto lenMinus2 = static_cast<std::size_t>(m_Buffer[pos + 1]);
            const auto total     = lenMinus2 + 2;           // includes checksum
            if (total < kMinTotal || total > m_MaxTotalPolicy) return std::optional<std::size_t>{0}; // impossible
            return total;
        };
        auto is_complete_valid_at = [&](std::size_t pos, std::size_t total) -> bool {
            if (pos + total > m_Buffer.size()) return false;                 // incomplete
            const auto endMarkerIdx = pos + total - 2;                        // penultimate
            if (m_Buffer[endMarkerIdx] != FrameEndOpCode) return false;
            const auto chk = m_ChecksumCalculator(std::span<const uint8_t>(&m_Buffer[pos], total - 1));
            return chk == m_Buffer[pos + total - 1];
        };

        for (;;)
        {
            // 1) Resync to first start byte; drop pure noise in front
            auto it = std::ranges::find(m_Buffer, FrameStartOpCode);
            if (it == m_Buffer.end()) { m_Buffer.clear(); return false; }
            if (it != m_Buffer.begin()) m_Buffer.erase(m_Buffer.begin(), it);

            // 2) If not even a minimal header, wait for more
            if (m_Buffer.size() < kMinTotal) return false;

            // 3) Inspect the candidate at pos=0
            std::size_t pos = 0;
            auto totalOpt = total_len_at(pos);
            if (!totalOpt.has_value()) return false;               // need more bytes for header
            std::size_t total = *totalOpt;

            if (total == 0) {
                // impossible size (too small or > policy): drop this start, keep scanning
                m_Buffer.erase(m_Buffer.begin());
                continue;
            }

            if (pos + total > m_Buffer.size()) {
                // Incomplete candidate. Look ahead for a later COMPLETE & VALID frame.
                // If none found, DO NOT consume; return false.
                std::size_t scan = pos + 1;
                while (true) {
                    auto it2 = std::find(m_Buffer.begin() + scan, m_Buffer.end(), FrameStartOpCode);
                    if (it2 == m_Buffer.end()) return false;       // no later start visible
                    std::size_t pos2 = static_cast<std::size_t>(it2 - m_Buffer.begin());

                    auto total2Opt = total_len_at(pos2);
                    if (!total2Opt.has_value()) return false;      // not enough header for later start
                    std::size_t total2 = *total2Opt;

                    if (total2 == 0) {                             // impossible at pos2 → skip this start
                        scan = pos2 + 1;
                        continue;
                    }
                    if (pos2 + total2 > m_Buffer.size()) {
                        // later frame also incomplete → we cannot decide yet
                        return false;
                    }
                    // later frame is complete in the buffer; validate it
                    if (!is_complete_valid_at(pos2, total2)) {
                        scan = pos2 + 1;                           // not valid; try the next start
                        continue;
                    }
                    // Found a complete & valid later frame. Commit: drop bytes before it, emit it.
                    out.assign(m_Buffer.begin() + pos2, m_Buffer.begin() + pos2 + total2);
                    m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + pos2 + total2);
                    return true;
                }
            }

            // 4) Candidate at pos=0 is complete. Validate and either emit or drop-start & rescan.
            if (!is_complete_valid_at(pos, total)) {
                m_Buffer.erase(m_Buffer.begin());                  // bad end marker or checksum
                continue;
            }

            // 5) Success: emit and consume exactly one frame
            out.assign(m_Buffer.begin(), m_Buffer.begin() + total);
            m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + total);
            return true;
        }
    }
    };
} // BrakeTester

#endif //BRAKETESTERRESULTMEMORYWRITER_FRAMER_H
