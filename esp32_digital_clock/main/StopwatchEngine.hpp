// StopwatchEngine.hpp - Stopwatch with lap/split recording (microsecond resolution)
#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_timer.h"

namespace Clock {

class StopwatchEngine {
public:
    using ClockFn = int64_t (*)();  // injectable time source for unit tests
    static constexpr size_t MAX_LAPS = 5;

    explicit StopwatchEngine(ClockFn clock = esp_timer_get_time) : clock_(clock) {}

    void start();
    void pause();
    void toggle() { running_ ? pause() : start(); }
    void reset();
    bool recordLap();

    int64_t elapsedUs() const;
    bool running() const { return running_; }
    size_t lapCount() const { return lapCount_; }
    int64_t lapUs(size_t i) const { return i < lapCount_ ? laps_[i] : 0; }
    int64_t splitUs(size_t i) const;  // time between lap i-1 and lap i

    // "MM:SS.cc" (centiseconds)
    static void format(int64_t us, char* out, size_t len);

private:
    ClockFn clock_;
    int64_t startUs_{0};
    int64_t accumulatedUs_{0};
    bool running_{false};
    int64_t laps_[MAX_LAPS]{};
    size_t lapCount_{0};
};

}  // namespace Clock
