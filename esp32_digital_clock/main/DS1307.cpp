// DS1307.cpp - DS1307 implementation
#include "DS1307.hpp"
#include "esp_log.h"

namespace Clock {

static const char* TAG = "DS1307";

bool DS1307::init() {
    if (!bus_.probe(I2C_ADDR)) {
        ESP_LOGW(TAG, "No DS1307 found at 0x%02X", I2C_ADDR);
        return false;
    }
    dev_ = bus_.addDevice(I2C_ADDR);
    if (!dev_) return false;

    uint8_t reg = REG_SECONDS;
    uint8_t seconds = 0;
    if (bus_.writeRead(dev_, &reg, 1, &seconds, 1) != ESP_OK) return false;

    // Oscillator is stopped after first power-up: clear CH but keep the seconds value
    if (seconds & CH_BIT) {
        uint8_t tx[2] = {REG_SECONDS, static_cast<uint8_t>(seconds & ~CH_BIT)};
        bus_.write(dev_, tx, sizeof(tx));
        ESP_LOGW(TAG, "Clock-halt bit was set, oscillator started");
    }
    ESP_LOGI(TAG, "DS1307 ready at 0x%02X", I2C_ADDR);
    return true;
}

uint8_t DS1307::decodeHour(uint8_t reg) {
    if (reg & 0x40) {  // 12-hour mode: bit 5 = PM, bits 4..0 = 1-12
        uint8_t h = bcd2dec(reg & 0x1F) % 12;
        return (reg & 0x20) ? h + 12 : h;
    }
    return bcd2dec(reg & 0x3F);
}

bool DS1307::getTime(DateTime& dt) {
    if (!dev_) return false;
    uint8_t reg = REG_SECONDS;
    uint8_t raw[7];
    if (bus_.writeRead(dev_, &reg, 1, raw, sizeof(raw)) != ESP_OK) return false;

    dt.second = bcd2dec(raw[0] & 0x7F);
    dt.minute = bcd2dec(raw[1] & 0x7F);
    dt.hour = decodeHour(raw[2]);
    dt.dayOfWeek = bcd2dec(raw[3] & 0x07);
    dt.dayOfMonth = bcd2dec(raw[4] & 0x3F);
    dt.month = bcd2dec(raw[5] & 0x1F);
    dt.year = 2000 + bcd2dec(raw[6]);
    return isValid(dt);
}

bool DS1307::setTime(const DateTime& dt) {
    if (!dev_ || !isValid(dt)) return false;
    uint8_t tx[8] = {
        REG_SECONDS,
        dec2bcd(dt.second),  // CH = 0 keeps the oscillator running
        dec2bcd(dt.minute),
        dec2bcd(dt.hour),    // bit 6 = 0 -> 24-hour mode
        dec2bcd(dt.dayOfWeek),
        dec2bcd(dt.dayOfMonth),
        dec2bcd(dt.month),
        dec2bcd(static_cast<uint8_t>(dt.year - 2000)),
    };
    return bus_.write(dev_, tx, sizeof(tx)) == ESP_OK;
}

void DS1307::toTm(const DateTime& dt, struct tm& out) {
    out = {};
    out.tm_sec = dt.second;
    out.tm_min = dt.minute;
    out.tm_hour = dt.hour;
    out.tm_mday = dt.dayOfMonth;
    out.tm_mon = dt.month - 1;
    out.tm_year = dt.year - 1900;
    out.tm_wday = dt.dayOfWeek - 1;
    out.tm_isdst = -1;
}

DateTime DS1307::fromTm(const struct tm& in) {
    DateTime dt;
    dt.second = static_cast<uint8_t>(in.tm_sec);
    dt.minute = static_cast<uint8_t>(in.tm_min);
    dt.hour = static_cast<uint8_t>(in.tm_hour);
    dt.dayOfWeek = static_cast<uint8_t>(in.tm_wday + 1);
    dt.dayOfMonth = static_cast<uint8_t>(in.tm_mday);
    dt.month = static_cast<uint8_t>(in.tm_mon + 1);
    dt.year = static_cast<uint16_t>(in.tm_year + 1900);
    return dt;
}

bool DS1307::isValid(const DateTime& dt) {
    return dt.second < 60 && dt.minute < 60 && dt.hour < 24 &&
           dt.dayOfWeek >= 1 && dt.dayOfWeek <= 7 &&
           dt.dayOfMonth >= 1 && dt.dayOfMonth <= 31 &&
           dt.month >= 1 && dt.month <= 12 &&
           dt.year >= 2000 && dt.year <= 2099;
}

}  // namespace Clock
