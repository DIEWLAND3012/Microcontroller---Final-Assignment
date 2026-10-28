// AlarmEngine.hpp - Alarm comparator + LEDC piezo buzzer driver
#pragma once
#include <atomic>
#include <cstdint>
#include "driver/gpio.h"

namespace Clock {

class TimeManager;

class AlarmEngine {
public:
    AlarmEngine(gpio_num_t pin, uint32_t toneHz) : pin_(pin), toneHz_(toneHz) {}

    bool init();
    [[noreturn]] void taskLoop(TimeManager& time);  // FreeRTOS task body

    void setTime(uint8_t hour, uint8_t minute) { hour_ = hour % 24; minute_ = minute % 60; }
    void setEnabled(bool on) { enabled_ = on; }
    void toggleEnabled() { enabled_ = !enabled_; }
    void stop();  // silence a ringing alarm

    uint8_t hour() const { return hour_; }
    uint8_t minute() const { return minute_; }
    bool enabled() const { return enabled_; }
    bool ringing() const { return ringing_; }

    // Pure comparator, unit-tested in Spiral 4
    static bool matches(int hour, int minute, int alarmHour, int alarmMinute) {
        return hour == alarmHour && minute == alarmMinute;
    }

private:
    void tone(bool on);

    gpio_num_t pin_;
    uint32_t toneHz_;
    std::atomic<uint8_t> hour_{7};
    std::atomic<uint8_t> minute_{0};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> ringing_{false};
    bool toneOn_{false};
    int lastTriggerStamp_{-1};  // minute-of-year that last fired (no re-trigger after stop)
    int64_t ringStartUs_{0};
};

}  // namespace Clock
