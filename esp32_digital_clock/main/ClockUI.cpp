// ClockUI.cpp - button handling and screen layouts for each mode
#include "ClockUI.hpp"
#include <cstdio>
#include "esp_log.h"
#include "esp_timer.h"

namespace Clock {

static const char* TAG = "UI";

static const char* const WEEKDAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char* const MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

Mode ClockUI::nextMode(Mode m) {
    return static_cast<Mode>((static_cast<int>(m) + 1) % static_cast<int>(Mode::COUNT));
}

const char* ClockUI::modeName(Mode m) {
    switch (m) {
        case Mode::CLOCK: return "CLOCK";
        case Mode::ALARM_SET: return "ALARM SET";
        case Mode::STOPWATCH: return "STOPWATCH";
        case Mode::TIME_SET: return "TIME SET";
        default: return "?";
    }
}

void ClockUI::taskLoop(QueueHandle_t buttonEvents) {
    for (;;) {
        ButtonEvent ev;
        if (xQueueReceive(buttonEvents, &ev, pdMS_TO_TICKS(100)) == pdTRUE) handle(ev);
        render();
    }
}

void ClockUI::enterMode(Mode m) {
    mode_ = m;
    editField_ = 0;
    if (m == Mode::TIME_SET) time_.now(editTm_);
    ESP_LOGI(TAG, "Mode -> %s", modeName(m));
}

void ClockUI::adjustField(int& hour, int& minute, int step) const {
    if (editField_ == 0) hour = (hour + step) % 24;
    else minute = (minute + step) % 60;
}

void ClockUI::handle(const ButtonEvent& ev) {
    // Any button silences a ringing alarm and is consumed
    if (alarm_.ringing()) {
        alarm_.stop();
        return;
    }

    const bool isLong = ev.type == PressType::LONG;

    if (ev.id == ButtonId::MODE) {
        enterMode(isLong ? Mode::CLOCK : nextMode(mode_));
        return;
    }

    switch (mode_) {
        case Mode::CLOCK:
            if (ev.id == ButtonId::ALARM) {
                alarm_.toggleEnabled();
                ESP_LOGI(TAG, "Alarm %s", alarm_.enabled() ? "ON" : "OFF");
            } else if (ev.id == ButtonId::SET && isLong) {
                time_.requestSync();
            }
            break;

        case Mode::ALARM_SET:
            if (ev.id == ButtonId::SET) {
                int h = alarm_.hour(), m = alarm_.minute();
                adjustField(h, m, isLong ? 10 : 1);
                alarm_.setTime(h, m);
            } else if (ev.id == ButtonId::LAP) {
                editField_ ^= 1;
            } else if (ev.id == ButtonId::ALARM) {
                alarm_.toggleEnabled();
            }
            break;

        case Mode::STOPWATCH:
            if (ev.id == ButtonId::SET) {
                stopwatch_.toggle();
            } else if (ev.id == ButtonId::LAP) {
                if (!isLong && stopwatch_.running()) stopwatch_.recordLap();
                else stopwatch_.reset();
            }
            break;

        case Mode::TIME_SET:
            if (ev.id == ButtonId::SET) {
                adjustField(editTm_.tm_hour, editTm_.tm_min, isLong ? 10 : 1);
            } else if (ev.id == ButtonId::LAP) {
                editField_ ^= 1;
            } else if (ev.id == ButtonId::ALARM) {
                editTm_.tm_sec = 0;
                time_.setLocalTime(editTm_);
                enterMode(Mode::CLOCK);
            }
            break;

        default:
            break;
    }
}

void ClockUI::render() {
    const bool blink = (esp_timer_get_time() / 500000) % 2 == 0;
    display_.clear();

    if (alarm_.ringing()) {
        renderRinging(blink);
    } else {
        renderHeader();
        switch (mode_) {
            case Mode::CLOCK: renderClock(); break;
            case Mode::ALARM_SET: renderAlarmSet(blink); break;
            case Mode::STOPWATCH: renderStopwatch(); break;
            case Mode::TIME_SET: renderTimeSet(blink); break;
            default: break;
        }
    }
    display_.flush();
}

void ClockUI::renderHeader() {
    const char* source = time_.isNtpSynced()       ? "NTP"
                         : time_.isWifiConnected() ? "WIFI"
                         : time_.isRtcValid()      ? "RTC"
                                                   : "----";
    char status[16];
    snprintf(status, sizeof(status), "%s%s", alarm_.enabled() ? "AL " : "", source);

    display_.drawString(0, 0, modeName(mode_));
    display_.drawString(SSD1306::WIDTH - SSD1306::textWidth(status), 0, status);
    display_.fillRect(0, 10, SSD1306::WIDTH, 1);
}

void ClockUI::renderClock() {
    struct tm t;
    char buf[24];
    if (!time_.now(t)) {
        display_.drawStringCentered(18, "--:--:--", 2);
        display_.drawStringCentered(40, "NO TIME SOURCE");
        return;
    }
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    display_.drawStringCentered(16, buf, 2);

    snprintf(buf, sizeof(buf), "%s %02d %s %04d", WEEKDAYS[t.tm_wday], t.tm_mday,
             MONTHS[t.tm_mon], t.tm_year + 1900);
    display_.drawStringCentered(38, buf);

    snprintf(buf, sizeof(buf), "ALARM %02d:%02d %s", alarm_.hour(), alarm_.minute(),
             alarm_.enabled() ? "ON" : "OFF");
    display_.drawStringCentered(54, buf);
}

void ClockUI::drawHourMinuteEditor(int hour, int minute, bool blink) {
    constexpr int scale = 3;
    constexpr int y = 15;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    const int x0 = (SSD1306::WIDTH - SSD1306::textWidth(buf, scale)) / 2;
    display_.drawString(x0, y, buf, scale);

    // Underline the field being edited; it blinks to show it is selected
    if (blink) {
        const int fieldX = x0 + (editField_ == 0 ? 0 : 3) * SSD1306::CHAR_W * scale;
        display_.fillRect(fieldX, y + 8 * scale, (2 * SSD1306::CHAR_W - 1) * scale, 2);
    }
}

void ClockUI::renderAlarmSet(bool blink) {
    drawHourMinuteEditor(alarm_.hour(), alarm_.minute(), blink);
    display_.drawStringCentered(46, alarm_.enabled() ? "ALARM: ON" : "ALARM: OFF");
    display_.drawStringCentered(56, "SET:+1 LAP:FIELD");
}

void ClockUI::renderTimeSet(bool blink) {
    drawHourMinuteEditor(editTm_.tm_hour, editTm_.tm_min, blink);
    display_.drawStringCentered(46, "SET:+1 LAP:FIELD");
    display_.drawStringCentered(56, "ALARM: SAVE");
}

void ClockUI::renderStopwatch() {
    char buf[40], lap[12], split[12];
    StopwatchEngine::format(stopwatch_.elapsedUs(), buf, sizeof(buf));
    display_.drawStringCentered(15, buf, 2);
    display_.drawStringCentered(33, stopwatch_.running() ? "RUNNING" : "STOPPED");

    // Show the last two laps: "L1 00:01.50 +00:01.50"
    const size_t n = stopwatch_.lapCount();
    int y = 45;
    for (size_t i = (n > 2 ? n - 2 : 0); i < n; ++i, y += 10) {
        StopwatchEngine::format(stopwatch_.lapUs(i), lap, sizeof(lap));
        StopwatchEngine::format(stopwatch_.splitUs(i), split, sizeof(split));
        snprintf(buf, sizeof(buf), "L%u %s +%s", static_cast<unsigned>(i + 1), lap, split);
        display_.drawString(0, y, buf);
    }
}

void ClockUI::renderRinging(bool blink) {
    // Whole screen flashes inverted twice per second
    if (blink) display_.fillRect(0, 0, SSD1306::WIDTH, SSD1306::HEIGHT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_.hour(), alarm_.minute());
    display_.drawStringCentered(6, "ALARM!", 3, !blink);
    display_.drawStringCentered(36, buf, 2, !blink);
    display_.drawStringCentered(56, "PRESS ANY BUTTON", 1, !blink);
}

}  // namespace Clock
