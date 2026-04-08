#pragma once
#include <functional>
#include <array>
#include <cstdint>
#include <iterator>
#include "brake_tester/RomanS8pt7b.h"

class ESCP2Renderer
{
    enum class RasterDPI : uint8_t
    {
        DPI_180 = 0x20,
        DPI_360 = 0x21,
        DPI_180_Variant = 0x26,
        DPI_360_Variant = 0x27
    };

    struct State
    {
        std::function<void(size_t, size_t, uint8_t)> setPixel;

        size_t cursorX{20};
        size_t cursorY{20};

        uint8_t lineSpacing{24};

        bool boldEnabled{false};
        bool doubleWidthEnabled{false};
        bool doubleHeightEnabled{false};
        size_t leftMargin{0};
        uint8_t colour{0};
        char pageLengthInches{11};

        RasterDPI rasterDPI{RasterDPI::DPI_360_Variant};
        uint16_t rasterColumns{0};
    };

    State m_State;

    template <size_t ArgumentCount>
    struct Command
    {
        uint8_t name;
        std::function<void(State &, std::array<uint8_t, ArgumentCount>)> execute;
    };

    std::array<Command<0>, 3> m_Commands{
        {
            {'@', [](State &state, std::array<uint8_t, 0>)
            {state.cursorX = 0; state.cursorY = 0;}},                               // Initialize

        {'E', [](State &state, std::array<uint8_t, 0>)
        {state.boldEnabled = true;}},                                           // Enable bold

    {'F', [](State &state, std::array<uint8_t, 0>)
    {state.boldEnabled = false;}}                                           // Disable bold
        }};

    std::array<Command<1>, 6> m_Commands1{
        {
            {'A', [](State &state, std::array<uint8_t, 1> args)
            {state.lineSpacing = static_cast<uint8_t>(args[0]) * 3;}},              //Set n/60-inch line spacing

        {'W', [](State &state, std::array<uint8_t, 1> args)
        {state.doubleWidthEnabled = (args[0] == 0 ? false : true); }},     //Double width

    {'w', [](State &state, std::array<uint8_t, 1> args)
    {state.doubleHeightEnabled = (args[0] == 0 ? false : true); }},    //Double height

{'l', [](State &state, std::array<uint8_t, 1> args)
{state.leftMargin = static_cast<size_t>(args[0]) * 14; }},              //Set left margin

{'r', [](State &state, std::array<uint8_t, 1> args)
{state.colour = (args[0] == 0x80 ? 0 : args[0]); }},                  //Set colour

{'J', [](State &state, std::array<uint8_t, 1> args)
{state.cursorY += static_cast<size_t>(args[0]);  } }                      //Advance 1/180 inch vertically
        }};

    std::array<Command<2>, 3> m_Commands2{
        {
            {'C', [](State &state, std::array<uint8_t, 2> args)
            { state.pageLengthInches = args[1]; }},                             // Set page length inches

        {'$', [](State &state, std::array<uint8_t, 2> args)
        { state.cursorX = static_cast<size_t>(static_cast<uint16_t>(args[0]) | (static_cast<uint16_t>(args[1]) << 8)); }},// Set absolute horizontal position

        }};

    std::array<Command<3>, 1> m_Commands3{{{
            '*', [](State &state, std::array<uint8_t, 3> args)
            {state.rasterDPI = static_cast<RasterDPI>(args[0]); state.rasterColumns = static_cast<uint16_t>(static_cast<uint16_t>(args[1]) | (static_cast<uint16_t>(args[2]) << 8));}} // Print graphics (not implemented)

        }};

    std::array<Command<0>, 2> m_ControlCommands{{

        { 0xA, [](State &state, std::array<uint8_t, 0>)
        {state.cursorY += state.lineSpacing * (state.doubleHeightEnabled ? 2 : 1); }}, // Line feed)

        {0xD, [](State &state, std::array<uint8_t, 0>)
            {state.cursorX = state.leftMargin; } } // Carriage return
        }};

public:
    explicit ESCP2Renderer(const std::function<void(size_t, size_t, uint8_t)> &setPixel) : m_State({setPixel}) {}

    [[nodiscard]] size_t cursorY() const
    {
        return m_State.cursorY;
    }

    template <typename IteratorType>
    void addBytes(IteratorType begin, IteratorType end)
    {
        while (begin != end)
        {
            const auto remainingBytes = std::distance(begin, end);

            if (m_State.rasterColumns > 0)
            {
                if (remainingBytes < 3)
                {
                    break;
                }
                m_State.rasterColumns--;

                rasterLine(m_State, *(begin++)); m_State.cursorY += 8;
                rasterLine(m_State, *(begin++)); m_State.cursorY += 8;
                rasterLine(m_State, *(begin++)); m_State.cursorY += 8;
                m_State.cursorY -= 24;
                m_State.cursorX += 1;
                continue;
            }


            bool matched = false;
            if (*begin == 0x1B) // ESC
            {
                ++begin;
                if (begin == end)
                {
                    break;
                }

                for (auto &command : m_Commands)
                {
                    if (*begin == command.name)
                    {
                        command.execute(m_State, {});
                        matched = true;
                        ++begin;
                        break;
                    }
                }

                if (matched) continue;

                for (auto &command : m_Commands1)
                {
                    if (*begin == command.name)
                    {
                        if (std::distance(begin, end) < 2)
                        {
                            return;
                        }
                        command.execute(m_State, {*(++begin)});
                        matched = true;
                        ++begin;
                        break;
                    }
                }

                if (matched) continue;

                for (auto &command : m_Commands2)
                {
                    if (*begin == command.name)
                    {
                        if (std::distance(begin, end) < 3)
                        {
                            return;
                        }
                        command.execute(m_State, {*(++begin), *(++begin)});
                        matched = true;
                        ++begin;
                        break;
                    }
                }

                if (matched) continue;

                for (auto &command : m_Commands3)
                {
                    if (*begin == command.name)
                    {
                        if (std::distance(begin, end) < 4)
                        {
                            return;
                        }
                        command.execute(m_State, {*(++begin), *(++begin), *(++begin)});
                        matched = true;
                        ++begin;
                        break;
                    }
                }

                if (matched) continue;
            }

            for (auto &command : m_ControlCommands)
            {
                if (*begin == command.name)
                {
                    command.execute(m_State, {});
                    matched = true;
                    ++begin;
                    break;
                }
            }

            if (matched) continue;

            if (*begin < 0x20 || *begin > 0xCE)
            {
                ++begin;
                continue; // Ignore unsupported characters
            }

            auto glyph = RomanS8pt7bGlyphs[*(begin++) - 0x20];

            if (m_State.doubleHeightEnabled && m_State.doubleWidthEnabled)
            {
                renderDoubleSizeGlyph(m_State, glyph, m_State.cursorX, m_State.cursorY, m_State.boldEnabled);
            }
            else
            {
                renderGlyph(m_State, glyph, m_State.cursorX, m_State.cursorY, m_State.boldEnabled);
            }

            m_State.cursorX += glyph.xAdvance * (m_State.doubleWidthEnabled ? 2 : 1);
        }
    }

private:
    static void renderGlyph(State &state, const GFXglyph &glyph, const int x, const int y, bool bold = false)
    {
        for (uint8_t row = 0; row < glyph.height; row++)
        {
            for (uint8_t col = 0; col < glyph.width; col++)
            {
                // Calculate bit index
                const uint32_t bitIndex = glyph.bitmapOffset * 8 + row * glyph.width + col;
                const uint32_t byteIndex = bitIndex / 8;

                // Check if the bit is set
                if (const uint8_t bitInByte = 7 - (bitIndex % 8); RomanS8pt7bBitmaps[byteIndex] & (1 << bitInByte))
                {
                    state.setPixel(x + col + glyph.xOffset, y + row + glyph.yOffset, state.colour);
                        if (bold)
                            state.setPixel(x + col + glyph.xOffset + 1, y + row + glyph.yOffset, state.colour);
                }
            }
        }
    }

    static void renderDoubleSizeGlyph(State &state, const GFXglyph &glyph, const int x, const int y, bool bold = false)
    {
        for (uint8_t row = 0; row < glyph.height; row++)
        {
            for (uint8_t col = 0; col < glyph.width; col++)
            {
                // Calculate bit index
                const uint32_t bitIndex = glyph.bitmapOffset * 8 + row * glyph.width + col;
                const uint32_t byteIndex = bitIndex / 8;

                // Check if the bit is set
                if (const uint8_t bitInByte = 7 - (bitIndex % 8); RomanS8pt7bBitmaps[byteIndex] & (1 << bitInByte))
                {
                    // Draw 2x2 block for double size
                    state.setPixel(x + (col * 2) + glyph.xOffset, y + (row * 2) + glyph.yOffset, state.colour);
                    state.setPixel(x + (col * 2) + 1 + glyph.xOffset, y + (row * 2) + glyph.yOffset, state.colour);
                    state.setPixel(x + (col * 2) + glyph.xOffset, y + (row * 2) + 1 + glyph.yOffset, state.colour);
                    state.setPixel(x + (col * 2) + 1 + glyph.xOffset, y + (row * 2) + 1 + glyph.yOffset, state.colour);

                    if (bold)
                    {
                        // Extra pixels for bold effect
                        state.setPixel(x + (col * 2) + 2 + glyph.xOffset, y + (row * 2) + glyph.yOffset, state.colour);
                        state.setPixel(x + (col * 2) + 2 + glyph.xOffset, y + (row * 2) + 1 + glyph.yOffset, state.colour);
                    }
                }
            }
        }
    }

    static void rasterLine(State &state, const uint8_t byte)
    {
        for (int i = 0; i < 8; i++)
        {
            if (byte & (1 << (7 - i)))
            {
                state.setPixel(state.cursorX, state.cursorY + i, state.colour);
            }
        }
    }
};



