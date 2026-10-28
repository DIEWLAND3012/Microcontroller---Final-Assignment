// ClockUI.hpp - UI state machine: maps button events to engines and renders the OLED
#pragma once
#include <ctime>
#include "AlarmEngine.hpp"
#include "ButtonManager.hpp"
#include "SSD1306.hpp"
#include "StopwatchEngine.hpp"
#include "TimeManager.hpp"

namespace Clock {

enum class Mode : uint8_t { CLOCK, ALARM_SET, STOPWATCH, TIME_SET, COUNT };

class ClockUI {
public:
    ClockUI(SSD1306& display, TimeManager& time, AlarmEngine& alarm, StopwatchEngine& stopwatch)
        : display_(display), time_(time), alarm_(alarm), stopwatch_(stopwatch) {}

    // FreeRTOS task body: handle button events, redraw at 10 FPS
    [[noreturn]] void taskLoop(QueueHandle_t buttonEvents);

    void handle(const ButtonEvent& ev);
    void render();
    Mode mode() const { return mode_; }

    static Mode nextMode(Mode m);
    static const char* modeName(Mode m);

private:
    void enterMode(Mode m);
    void adjustField(int& hour, int& minute, int step) const;

    void renderHeader();
    void renderClock();
    void renderAlarmSet(bool blink);
    void renderStopwatch();
    void renderTimeSet(bool blink);
    void renderRinging(bool blink);
    void drawHourMinuteEditor(int hour, int minute, bool blink);

    SSD1306& display_;
    TimeManager& time_;
    AlarmEngine& alarm_;
    StopwatchEngine& stopwatch_;

    Mode mode_{Mode::CLOCK};
    int editField_{0};  // 0 = hour, 1 = minute
    struct tm editTm_{};
};

}  // namespace Clock
