// DS1307.hpp - DS1307 real-time clock driver (I2C 0x68, BCD registers)
#pragma once
#include <cstdint>
#include <ctime>
#include "I2CBus.hpp"

namespace Clock {

struct DateTime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;       // 0-23
    uint8_t dayOfWeek;  // 1-7, 1 = Sunday
    uint8_t dayOfMonth; // 1-31
    uint8_t month;      // 1-12
    uint16_t year;      // 2000-2099
};

class DS1307 {
public:
    static constexpr uint8_t I2C_ADDR = 0x68;

    explicit DS1307(I2CBus& bus) : bus_(bus) {}

    bool init();
    bool isPresent() const { return dev_ != nullptr; }
    bool getTime(DateTime& dt);
    bool setTime(const DateTime& dt);

    static constexpr uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static constexpr uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

    // Decodes the hours register in either 24 h or 12 h (bit 6 set) format
    static uint8_t decodeHour(uint8_t reg);

    static void toTm(const DateTime& dt, struct tm& out);
    static DateTime fromTm(const struct tm& in);
    static bool isValid(const DateTime& dt);

private:
    static constexpr uint8_t REG_SECONDS = 0x00;
    static constexpr uint8_t CH_BIT = 0x80;  // Clock Halt

    I2CBus& bus_;
    i2c_master_dev_handle_t dev_{nullptr};
};

}  // namespace Clock
