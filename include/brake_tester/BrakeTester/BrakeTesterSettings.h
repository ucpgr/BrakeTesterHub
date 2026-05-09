#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace BrakeTester {

namespace detail {

constexpr std::uint16_t bswap16(std::uint16_t v) noexcept {
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}

constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

template <typename T>
T fromBigEndian(T value) noexcept {
    static_assert(std::is_integral_v<T>);

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return value;
#else
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        using U = std::make_unsigned_t<T>;
        return static_cast<T>(bswap16(static_cast<std::uint16_t>(static_cast<U>(value))));
    } else if constexpr (sizeof(T) == 4) {
        using U = std::make_unsigned_t<T>;
        return static_cast<T>(bswap32(static_cast<std::uint32_t>(static_cast<U>(value))));
    } else {
        static_assert(sizeof(T) <= 4, "Unsupported integer size");
    }
#endif
}

template <typename T>
T toBigEndian(T value) noexcept {
    return fromBigEndian(value);
}

inline void checkRange(std::span<std::uint8_t> data, std::size_t offset, std::size_t length) {
    if (offset + length > data.size()) {
        throw std::out_of_range("Setting outside data block");
    }
}

} // namespace detail

template <typename T>
class SettingValue {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                  "Generic SettingValue<T> only supports non-bool integral types. Use a specialization for other types.");
public:
    SettingValue(std::span<std::uint8_t> data,
                 std::size_t offset,
                 T minValue,
                 T maxValue,
                 T defaultValue)
        : m_data(data),
          m_offset(offset),
          m_minValue(minValue),
          m_maxValue(maxValue),
          m_defaultValue(defaultValue)
    {
        detail::checkRange(m_data, m_offset, sizeof(T));
    }

    T getValue() const {
        T stored{};
        std::memcpy(&stored, m_data.data() + m_offset, sizeof(T));
        return detail::fromBigEndian(stored);
    }

    void setValue(T value) {
        value = std::clamp(value, m_minValue, m_maxValue);
        T stored = detail::toBigEndian(value);
        std::memcpy(m_data.data() + m_offset, &stored, sizeof(T));
    }

    T getMinValue() const { return m_minValue; }
    T getMaxValue() const { return m_maxValue; }
    T getDefaultValue() const { return m_defaultValue; }

    void resetToDefault() { setValue(m_defaultValue); }

private:
    std::span<std::uint8_t> m_data;
    std::size_t m_offset{};
    T m_minValue{};
    T m_maxValue{};
    T m_defaultValue{};
};

template <>
class SettingValue<bool> {
public:
    SettingValue(std::span<std::uint8_t> data,
                 std::size_t offset,
                 std::uint8_t bit,
                 bool defaultValue)
        : m_data(data),
          m_offset(offset),
          m_bit(bit),
          m_defaultValue(defaultValue)
    {
        detail::checkRange(m_data, m_offset, 1);
        if (m_bit > 7) {
            throw std::out_of_range("Bit setting bit index must be 0-7");
        }
    }

    bool getValue() const {
        return (m_data[m_offset] & mask()) != 0;
    }

    void setValue(bool value) {
        if (value) {
            m_data[m_offset] |= mask();
        } else {
            m_data[m_offset] &= static_cast<std::uint8_t>(~mask());
        }
    }

    bool getMinValue() const { return false; }
    bool getMaxValue() const { return true; }
    bool getDefaultValue() const { return m_defaultValue; }

    void resetToDefault() { setValue(m_defaultValue); }

private:
    std::uint8_t mask() const {
        return static_cast<std::uint8_t>(1u << m_bit);
    }

    std::span<std::uint8_t> m_data;
    std::size_t m_offset{};
    std::uint8_t m_bit{};
    bool m_defaultValue{};
};

template <>
class SettingValue<float> {
public:
    SettingValue(std::span<std::uint8_t> data,
                 std::size_t offset,
                 float minValue,
                 float maxValue,
                 float defaultValue)
        : m_data(data),
          m_offset(offset),
          m_minValue(minValue),
          m_maxValue(maxValue),
          m_defaultValue(defaultValue)
    {
        detail::checkRange(m_data, m_offset, sizeof(std::uint16_t));
    }

    float getValue() const {
        std::uint16_t stored{};
        std::memcpy(&stored, m_data.data() + m_offset, sizeof(stored));
        return static_cast<float>(detail::fromBigEndian(stored)) / 10.0f;
    }

    void setValue(float value) {
        value = std::clamp(value, m_minValue, m_maxValue);
        auto scaled = static_cast<std::uint16_t>(value * 100.0f + 0.5f);
        scaled = detail::toBigEndian(scaled);
        std::memcpy(m_data.data() + m_offset, &scaled, sizeof(scaled));
    }

    float getMinValue() const { return m_minValue; }
    float getMaxValue() const { return m_maxValue; }
    float getDefaultValue() const { return m_defaultValue; }

    void resetToDefault() { setValue(m_defaultValue); }

private:
    std::span<std::uint8_t> m_data;
    std::size_t m_offset{};
    float m_minValue{};
    float m_maxValue{};
    float m_defaultValue{};
};

template <>
class SettingValue<std::string> {
public:
    SettingValue(std::span<std::uint8_t> data,
                 std::size_t offset,
                 std::size_t maxLength,
                 std::string defaultValue = {})
        : m_data(data),
          m_offset(offset),
          m_maxLength(maxLength),
          m_defaultValue(std::move(defaultValue))
    {
        detail::checkRange(m_data, m_offset, m_maxLength);
    }

    std::string getValue() const {
        const auto* begin = reinterpret_cast<const char*>(m_data.data() + m_offset);
        const auto* end = begin + m_maxLength;

        while (end != begin && *(end - 1) == ' ') {
            --end;
        }

        return std::string(begin, end);
    }

    void setValue(std::string_view value) {
        std::fill(m_data.begin() + static_cast<std::ptrdiff_t>(m_offset),
                  m_data.begin() + static_cast<std::ptrdiff_t>(m_offset + m_maxLength),
                  static_cast<std::uint8_t>(' '));

        const auto length = std::min(value.size(), m_maxLength);
        std::memcpy(m_data.data() + m_offset, value.data(), length);
    }

    std::size_t getMinValue() const { return 0; }
    std::size_t getMaxValue() const { return m_maxLength; }
    std::string getDefaultValue() const { return m_defaultValue; }

    void resetToDefault() { setValue(m_defaultValue); }

private:
    std::span<std::uint8_t> m_data;
    std::size_t m_offset{};
    std::size_t m_maxLength{};
    std::string m_defaultValue;
};

class BrakeTesterSettings {
public:
    static constexpr std::size_t Size = 360;

    class Page0 {
    public:
        explicit Page0(std::span<std::uint8_t> data)
            : forceUnitKgf(data, 245, 0, true),
              enableExternalCommand(data, 245, 2, true),
              individualSkidStop(data, 245, 4, true),
              automaticStartTest(data, 245, 5, false)
        {}

        // 00001: Force &Unit kgf (default: N); offset 245; bit 0
        SettingValue<bool> forceUnitKgf;

        // 00002: &Enable External Command; offset 245; bit 2
        SettingValue<bool> enableExternalCommand;

        // 00003: &Individual Skid Stop; offset 245; bit 4
        SettingValue<bool> individualSkidStop;

        // 00004: Au&tomatic start test; offset 245; bit 5
        SettingValue<bool> automaticStartTest;

    };

    class Page1 {
    public:
        explicit Page1(std::span<std::uint8_t> data)
            : leftMotorStartTime(data, 0, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(2000)),
              rightMotorStartTime(data, 2, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(2500)),
              testStartTime(data, 4, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(3500)),
              forceIndicatorRangeHigh(data, 6, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(60000), static_cast<std::uint16_t>(40000)),
              forceIndicatorRangeLow(data, 8, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(50000), static_cast<std::uint16_t>(6000)),
              centralIndicatorRange(data, 10, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(92)),
              forceSensorRange(data, 12, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(60000), static_cast<std::uint16_t>(40000)),
              pedalSensorRange(data, 14, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(60000), static_cast<std::uint16_t>(1000)),
              weightSensorRange(data, 16, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(65000), static_cast<std::uint16_t>(9000)),
              skidValue(data, 76, static_cast<std::uint8_t>(5), static_cast<std::uint8_t>(99), static_cast<std::uint8_t>(25)),
              encoderPeriodMax(data, 18, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              applyBrakeTestEncPerMax(data, 40, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              applyBrakeTestStartTime(data, 48, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(500))
        {}

        // 01001: Left Motor Start Time (ms):; offset 0
        SettingValue<std::uint16_t> leftMotorStartTime;

        // 01002: Right Motor Start Time (ms):; offset 2
        SettingValue<std::uint16_t> rightMotorStartTime;

        // 01003: Test Start Time (ms):; offset 4
        SettingValue<std::uint16_t> testStartTime;

        // 01004: Force Indicator Range High (N):; offset 6
        SettingValue<std::uint16_t> forceIndicatorRangeHigh;

        // 01005: Force Indicator Range Low (N):; offset 8
        SettingValue<std::uint16_t> forceIndicatorRangeLow;

        // 01006: Central Indicator Range (%):; offset 10
        SettingValue<std::uint16_t> centralIndicatorRange;

        // 01007: Force Sensor Range (N):; offset 12
        SettingValue<std::uint16_t> forceSensorRange;

        // 01008: Pedal Sensor Range (N):; offset 14
        SettingValue<std::uint16_t> pedalSensorRange;

        // 01009: Weight Sensor Range (kg):; offset 16
        SettingValue<std::uint16_t> weightSensorRange;

        // 01010: Skid Value (%):; offset 76
        SettingValue<std::uint8_t> skidValue;

        // 01011: Encoder Period Max. (ms):; offset 18
        SettingValue<std::uint16_t> encoderPeriodMax;

        // 01012: Apply Brake Test Enc. Per. Max. (ms):; offset 40
        SettingValue<std::uint16_t> applyBrakeTestEncPerMax;

        // 01013: Apply Brake Test Start Time (ms):; offset 48
        SettingValue<std::uint16_t> applyBrakeTestStartTime;

    };

    class Page2 {
    public:
        explicit Page2(std::span<std::uint8_t> data)
            : carDiffEnForceTresh(data, 20, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(400)),
              truckDiffEnForceTresh(data, 274, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(10000), static_cast<std::uint16_t>(1000)),
              carDifferenceMaxSb(data, 22, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              carDifferenceMaxPb(data, 24, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              truckDifferenceMaxSb(data, 26, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              truckDifferenceMaxPb(data, 28, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25))
        {}

        // 02001: Car Diff. En. Force Tresh. (N):; offset 20
        SettingValue<std::uint16_t> carDiffEnForceTresh;

        // 02002: Truck Diff. En. Force Tresh. (N):; offset 274
        SettingValue<std::uint16_t> truckDiffEnForceTresh;

        // 02003: Car Difference Max. SB (%):; offset 22
        SettingValue<std::uint16_t> carDifferenceMaxSb;

        // 02004: Car Difference Max. PB (%):; offset 24
        SettingValue<std::uint16_t> carDifferenceMaxPb;

        // 02005: Truck Difference Max. SB (%):; offset 26
        SettingValue<std::uint16_t> truckDifferenceMaxSb;

        // 02006: Truck Difference Max. PB (%):; offset 28
        SettingValue<std::uint16_t> truckDifferenceMaxPb;

    };

    class Page3 {
    public:
        explicit Page3(std::span<std::uint8_t> data)
            : carAxleEffMinSb(data, 30, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              carVehicleEffMinSb(data, 34, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              carVehicleEffMinPb(data, 36, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              truckAxleEffMinSb(data, 38, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              truckVehicleEffMinSb(data, 42, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              truckVehicleEffMinPb(data, 44, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              truckVehicleEffMinEb(data, 254, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(25)),
              carAxleEffMinSb2(data, 46, 0.0f, 100.0f, 5.0f),
              carVehicleEffMinSb2(data, 50, 0.0f, 100.0f, 5.0f),
              carVehicleEffMinPb2(data, 52, 0.0f, 100.0f, 2.5f),
              truckAxleEffMinSb2(data, 54, 0.0f, 100.0f, 5.0f),
              truckVehicleEffMinSb2(data, 58, 0.0f, 100.0f, 2.5f),
              truckVehicleEffMinPb2(data, 60, 0.0f, 100.0f, 2.5f),
              truckVehicleEffMinEb2(data, 256, 0.0f, 100.0f, 2.5f)
        {}

        // 03001: Car Axle Eff. Min. SB (%):; offset 30
        SettingValue<std::uint16_t> carAxleEffMinSb;

        // 03002: Car Vehicle Eff. Min. SB (%):; offset 34
        SettingValue<std::uint16_t> carVehicleEffMinSb;

        // 03003: Car Vehicle Eff. Min. PB (%):; offset 36
        SettingValue<std::uint16_t> carVehicleEffMinPb;

        // 03004: Truck Axle Eff. Min. SB (%):; offset 38
        SettingValue<std::uint16_t> truckAxleEffMinSb;

        // 03005: Truck Vehicle Eff. Min. SB (%):; offset 42
        SettingValue<std::uint16_t> truckVehicleEffMinSb;

        // 03006: Truck Vehicle Eff. Min. PB (%):; offset 44
        SettingValue<std::uint16_t> truckVehicleEffMinPb;

        // 03007: Truck Vehicle Eff. Min. EB (%):; offset 254
        SettingValue<std::uint16_t> truckVehicleEffMinEb;

        // 03009: Car Axle Eff. Min. SB (m/s2):; offset 46
        SettingValue<float> carAxleEffMinSb2;

        // 03010: Car Vehicle Eff. Min. SB (m/s2):; offset 50
        SettingValue<float> carVehicleEffMinSb2;

        // 03011: Car Vehicle Eff. Min. PB (m/s2):; offset 52
        SettingValue<float> carVehicleEffMinPb2;

        // 03012: Truck Axle Eff. Min. SB (m/s2):; offset 54
        SettingValue<float> truckAxleEffMinSb2;

        // 03013: Truck Vehicle Eff. Min. SB (m/s2):; offset 58
        SettingValue<float> truckVehicleEffMinSb2;

        // 03014: Truck Vehicle Eff. Min. PB (m/s2):; offset 60
        SettingValue<float> truckVehicleEffMinPb2;

        // 03015: Truck Vehicle Eff. Min. EB (m/s2):; offset 256
        SettingValue<float> truckVehicleEffMinEb2;

    };

    class Page4 {
    public:
        explicit Page4(std::span<std::uint8_t> data)
            : pmPressureOffset(data, 258, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(5)),
              px1PressureOffset(data, 260, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(5)),
              px2PressureOffset(data, 262, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(5)),
              px3PressureOffset(data, 264, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(5)),
              pmPressureGain(data, 266, 0.0f, 40.0f, 5.0f),
              px1PressureGain(data, 268, 0.0f, 40.0f, 5.0f),
              px2PressureGain(data, 270, 0.0f, 40.0f, 5.0f),
              px3PressureGain(data, 272, 0.0f, 40.0f, 5.0f),
              pressureIndicatorRange(data, 32, 0.0f, 1000.0f, 10.0f),
              numberOfPxSensors(data, 57, static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(3), static_cast<std::uint8_t>(0)),
              pressurePrintTrigger(data, 250, 0.0f, 1000.0f, 0.5f)
        {}

        // 04001: Pm  Pressure Offset (Digit):; offset 258
        SettingValue<std::uint16_t> pmPressureOffset;

        // 04002: Px1 Pressure Offset (Digit):; offset 260
        SettingValue<std::uint16_t> px1PressureOffset;

        // 04003: Px2 Pressure Offset (Digit):; offset 262
        SettingValue<std::uint16_t> px2PressureOffset;

        // 04004: Px3 Pressure Offset (Digit):; offset 264
        SettingValue<std::uint16_t> px3PressureOffset;

        // 04005: Pm  Pressure Gain:; offset 266
        SettingValue<float> pmPressureGain;

        // 04006: Px1 Pressure Gain:; offset 268
        SettingValue<float> px1PressureGain;

        // 04007: Px2 Pressure Gain:; offset 270
        SettingValue<float> px2PressureGain;

        // 04008: Px3 Pressure Gain:; offset 272
        SettingValue<float> px3PressureGain;

        // 04009: Pressure Indicator Range (bar):; offset 32
        SettingValue<float> pressureIndicatorRange;

        // 04010: Number of Px Sensors:; offset 57
        SettingValue<std::uint8_t> numberOfPxSensors;

        // 04011: Pressure Print Trigger (bar):; offset 250
        SettingValue<float> pressurePrintTrigger;

    };

    class Page5 {
    public:
        explicit Page5(std::span<std::uint8_t> data)
            : carOvalityMax(data, 62, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(60000), static_cast<std::uint16_t>(200)),
              carOvalityMax2(data, 64, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(20)),
              truckOvalityMax(data, 66, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(60000), static_cast<std::uint16_t>(200)),
              truckOvalityMax2(data, 68, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(20)),
              ovalityPrintTrigger(data, 70, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              pedalPrintTrigger(data, 72, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(50)),
              weightPrintTrigger(data, 74, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(100), static_cast<std::uint16_t>(10)),
              truckWeightLimit(data, 252, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(30000), static_cast<std::uint16_t>(1000)),
              language(data, 77, static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(5), static_cast<std::uint8_t>(2)),
              remoteControlType(data, 56, static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(10), static_cast<std::uint8_t>(0)),
              remoteControlId(data, 78, static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(15), static_cast<std::uint8_t>(0)),
              brakeTestCounter(data, 246, static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(std::numeric_limits<std::uint32_t>::max()), static_cast<std::uint32_t>(0)),
              tachoTestSetPoint(data, 358, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(9999), static_cast<std::uint16_t>(0))
        {}

        // 05001: Car Ovality Max. (N):; offset 62
        SettingValue<std::uint16_t> carOvalityMax;

        // 05002: Car Ovality Max. (%):; offset 64
        SettingValue<std::uint16_t> carOvalityMax2;

        // 05003: Truck Ovality Max. (N):; offset 66
        SettingValue<std::uint16_t> truckOvalityMax;

        // 05004: Truck Ovality Max. (%):; offset 68
        SettingValue<std::uint16_t> truckOvalityMax2;

        // 05005: Ovality Print Trigger (N):; offset 70
        SettingValue<std::uint16_t> ovalityPrintTrigger;

        // 05006: Pedal Print Trigger (N):; offset 72
        SettingValue<std::uint16_t> pedalPrintTrigger;

        // 05007: Weight Print Trigger (kg):; offset 74
        SettingValue<std::uint16_t> weightPrintTrigger;

        // 05008: Truck Weight Limit (kg):; offset 252
        SettingValue<std::uint16_t> truckWeightLimit;

        // 05009: Language (0-5):; offset 77
        SettingValue<std::uint8_t> language;

        // 05010: Remote Control Type (0-1):; offset 56
        SettingValue<std::uint8_t> remoteControlType;

        // 05011: Remote Control ID:; offset 78
        SettingValue<std::uint8_t> remoteControlId;

        // 05012: Brake Test Counter:; offset 246
        SettingValue<std::uint32_t> brakeTestCounter;

        // 05013: Tacho Test Set Point (pulse/20m):; offset 358
        SettingValue<std::uint16_t> tachoTestSetPoint;

    };

    class Page6 {
    public:
        explicit Page6(std::span<std::uint8_t> data)
            : balance(data, 244, 1, true),
              modoItalia(data, 244, 2, false),
              ovalityMode(data, 244, 3, false),
              efficiencyMode(data, 244, 5, false),
              bidirectionalMode(data, 244, 6, false),
              setting06007(data, 79, 30),
              setting06008(data, 110, 30),
              setting06009(data, 141, 30)
        {}

        // 06001: Balance (0=No, 1=Yes):; offset 244; bit 1
        SettingValue<bool> balance;

        // 06002: Modo Italia (0=No, 1=Yes):; offset 244; bit 2
        SettingValue<bool> modoItalia;

        // 06003: Ovality Mode (0=Force, 1=%):; offset 244; bit 3
        SettingValue<bool> ovalityMode;

        // 06004: Efficiency Mode (0=%, 1=m/s2):; offset 244; bit 5
        SettingValue<bool> efficiencyMode;

        // 06005: BiDirectional Mode (0=No, 1=Yes):; offset 244; bit 6
        SettingValue<bool> bidirectionalMode;

        // 06007: ; offset 79
        SettingValue<std::string> setting06007;

        // 06008: ; offset 110
        SettingValue<std::string> setting06008;

        // 06009: ; offset 141
        SettingValue<std::string> setting06009;

    };

    class Page7 {
    public:
        explicit Page7(std::span<std::uint8_t> data)
            : categoriaM2(data, 276, 0.0f, 100.0f, 50.0f),
              categoriaM22(data, 300, 0.0f, 100.0f, 25.0f),
              categoriaM211093(data, 278, 0.0f, 100.0f, 50.0f),
              categoriaM2110932(data, 302, 0.0f, 100.0f, 25.0f),
              categoriaM3(data, 280, 0.0f, 100.0f, 50.0f),
              categoriaM32(data, 304, 0.0f, 100.0f, 25.0f),
              categoriaN2(data, 282, 0.0f, 100.0f, 50.0f),
              categoriaN22(data, 306, 0.0f, 100.0f, 25.0f),
              categoriaN21189(data, 284, 0.0f, 100.0f, 50.0f),
              categoriaN211892(data, 308, 0.0f, 100.0f, 25.0f),
              categoriaN3(data, 286, 0.0f, 100.0f, 50.0f),
              categoriaN32(data, 310, 0.0f, 100.0f, 25.0f),
              categoriaN31189(data, 288, 0.0f, 100.0f, 50.0f),
              categoriaN311892(data, 312, 0.0f, 100.0f, 25.0f),
              categoriaO3(data, 290, 0.0f, 100.0f, 50.0f),
              categoriaO32(data, 314, 0.0f, 100.0f, 25.0f),
              categoriaO31189(data, 292, 0.0f, 100.0f, 50.0f),
              categoriaO311892(data, 316, 0.0f, 100.0f, 25.0f),
              categoriaO4(data, 294, 0.0f, 100.0f, 50.0f),
              categoriaO42(data, 318, 0.0f, 100.0f, 25.0f),
              categoriaO41189(data, 296, 0.0f, 100.0f, 50.0f),
              categoriaO411892(data, 320, 0.0f, 100.0f, 25.0f),
              categoriaM1(data, 298, 0.0f, 100.0f, 50.0f),
              categoriaM12(data, 322, 0.0f, 100.0f, 25.0f),
              fPedMaxServ(data, 328, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(500)),
              fPedMaxSocc(data, 330, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(500)),
              fPedMaxStaz(data, 332, static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1000), static_cast<std::uint16_t>(500)),
              effMinStaz(data, 324, 0.0f, 100.0f, 16.0f),
              effMinStazRimorchio(data, 326, 0.0f, 100.0f, 12.0f),
              dataScadenzaControlloPeriodico(data, 336, 10),
              matricola(data, 347, 10)
        {}

        // 07003: Categoria M2:                (Code  0); offset 276
        SettingValue<float> categoriaM2;

        // 07004: Categoria M2:                (Code  0); offset 300
        SettingValue<float> categoriaM22;

        // 07005: Categoria M2 < 1.10.93: (Code  1); offset 278
        SettingValue<float> categoriaM211093;

        // 07006: Categoria M2 < 1.10.93: (Code  1); offset 302
        SettingValue<float> categoriaM2110932;

        // 07007: Categoria M3:                (Code  2); offset 280
        SettingValue<float> categoriaM3;

        // 07008: Categoria M3:                (Code  2); offset 304
        SettingValue<float> categoriaM32;

        // 07009: Categoria N2:                (Code  3); offset 282
        SettingValue<float> categoriaN2;

        // 07010: Categoria N2:                (Code  3); offset 306
        SettingValue<float> categoriaN22;

        // 07011: Categoria N2 < 1.1.89:   (Code  4); offset 284
        SettingValue<float> categoriaN21189;

        // 07012: Categoria N2 < 1.1.89:   (Code  4); offset 308
        SettingValue<float> categoriaN211892;

        // 07013: Categoria N3:                (Code  5); offset 286
        SettingValue<float> categoriaN3;

        // 07014: Categoria N3:                (Code  5); offset 310
        SettingValue<float> categoriaN32;

        // 07015: Categoria N3 < 1.1.89:   (Code  6); offset 288
        SettingValue<float> categoriaN31189;

        // 07016: Categoria N3 < 1.1.89:   (Code  6); offset 312
        SettingValue<float> categoriaN311892;

        // 07017: Categoria O3:                (Code  7); offset 290
        SettingValue<float> categoriaO3;

        // 07018: Categoria O3:                (Code  7); offset 314
        SettingValue<float> categoriaO32;

        // 07019: Categoria O3 < 1.1.89:   (Code  8); offset 292
        SettingValue<float> categoriaO31189;

        // 07020: Categoria O3 < 1.1.89:   (Code  8); offset 316
        SettingValue<float> categoriaO311892;

        // 07021: Categoria O4:                (Code  9); offset 294
        SettingValue<float> categoriaO4;

        // 07022: Categoria O4:                (Code  9); offset 318
        SettingValue<float> categoriaO42;

        // 07023: Categoria O4 < 1.1.89:   (Code 10); offset 296
        SettingValue<float> categoriaO41189;

        // 07024: Categoria O4 < 1.1.89:   (Code 10); offset 320
        SettingValue<float> categoriaO411892;

        // 07025: Categoria M1                    (Auto); offset 298
        SettingValue<float> categoriaM1;

        // 07026: Categoria M1                    (Auto); offset 322
        SettingValue<float> categoriaM12;

        // 07027: F. Ped. Max. Serv. (N):; offset 328
        SettingValue<std::uint16_t> fPedMaxServ;

        // 07028: F. Ped. Max. Socc. (N):; offset 330
        SettingValue<std::uint16_t> fPedMaxSocc;

        // 07029: F. Ped. Max. Staz. (N):; offset 332
        SettingValue<std::uint16_t> fPedMaxStaz;

        // 07030: Eff. Min. Staz. (%):; offset 324
        SettingValue<float> effMinStaz;

        // 07031: Eff. Min. Staz. + rimorchio (%):; offset 326
        SettingValue<float> effMinStazRimorchio;

        // 06011: Data Scadenza Controllo Periodico:; offset 336
        SettingValue<std::string> dataScadenzaControlloPeriodico;

        // 06012: Matricola:; offset 347
        SettingValue<std::string> matricola;

    };

    BrakeTesterSettings()
        : page0(std::span<std::uint8_t>(m_data)),
          page1(std::span<std::uint8_t>(m_data)),
          page2(std::span<std::uint8_t>(m_data)),
          page3(std::span<std::uint8_t>(m_data)),
          page4(std::span<std::uint8_t>(m_data)),
          page5(std::span<std::uint8_t>(m_data)),
          page6(std::span<std::uint8_t>(m_data)),
          page7(std::span<std::uint8_t>(m_data))
    {}

    void updateChunk(std::size_t offset, const std::uint8_t* bytes, std::size_t length) {
        if (offset + length > m_data.size()) {
            throw std::out_of_range("Settings chunk outside data block");
        }

        std::memcpy(m_data.data() + offset, bytes, length);
    }

    std::span<const std::uint8_t> raw() const {
        return std::span<const std::uint8_t>(m_data);
    }

    std::span<std::uint8_t> raw() {
        return std::span<std::uint8_t>(m_data);
    }

    Page0 page0;
    Page1 page1;
    Page2 page2;
    Page3 page3;
    Page4 page4;
    Page5 page5;
    Page6 page6;
    Page7 page7;

private:
    std::array<std::uint8_t, Size> m_data{};
};

} // namespace brake_tester
