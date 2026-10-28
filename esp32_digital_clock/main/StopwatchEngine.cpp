// StopwatchEngine.cpp - stopwatch implementation
#include "StopwatchEngine.hpp"
#include <cstdio>

namespace Clock {

void StopwatchEngine::start() {
    if (running_) return;
    startUs_ = clock_();
    running_ = true;
}

void StopwatchEngine::pause() {
    if (!running_) return;
    accumulatedUs_ += clock_() - startUs_;
    running_ = false;
}

void StopwatchEngine::reset() {
    running_ = false;
    accumulatedUs_ = 0;
    lapCount_ = 0;
}

bool StopwatchEngine::recordLap() {
    if (!running_ || lapCount_ >= MAX_LAPS) return false;
    laps_[lapCount_++] = elapsedUs();  // live value, not the last paused total
    return true;
}

int64_t StopwatchEngine::elapsedUs() const {
    return accumulatedUs_ + (running_ ? clock_() - startUs_ : 0);
}

int64_t StopwatchEngine::splitUs(size_t i) const {
    if (i >= lapCount_) return 0;
    return i == 0 ? laps_[0] : laps_[i] - laps_[i - 1];
}

void StopwatchEngine::format(int64_t us, char* out, size_t len) {
    const int64_t cs = us / 10000;  // centiseconds
    snprintf(out, len, "%02d:%02d.%02d", static_cast<int>((cs / 6000) % 100),
             static_cast<int>((cs / 100) % 60), static_cast<int>(cs % 100));
}

}  // namespace Clock
