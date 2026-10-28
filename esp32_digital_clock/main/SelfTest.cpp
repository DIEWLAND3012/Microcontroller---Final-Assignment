// SelfTest.cpp - bottom-up tests, one group per development spiral
#include "SelfTest.hpp"
#include <cstring>
#include "AlarmEngine.hpp"
#include "ButtonManager.hpp"
#include "ClockUI.hpp"
#include "DS1307.hpp"
#include "StopwatchEngine.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace Clock {

static const char* TAG = "TEST";

static int s_failed = 0;
static int s_assertions = 0;

static void check(bool cond, const char* expr, int line) {
    ++s_assertions;
    if (!cond) {
        ++s_failed;
        ESP_LOGE(TAG, "  FAIL line %d: %s", line, expr);
    }
}
#define CHECK(cond) check((cond), #cond, __LINE__)

static void runTest(const char* name, void (*fn)()) {
    const int before = s_failed;
    fn();
    if (s_failed == before) ESP_LOGI(TAG, "[PASS] %s", name);
    else ESP_LOGE(TAG, "[FAIL] %s", name);
}

// ---------- Spiral 1: HAL & drivers ----------
static SSD1306* s_display = nullptr;

static void test_ds1307_bcd_conversion() {
    CHECK(DS1307::bcd2dec(0x23) == 23);
    CHECK(DS1307::bcd2dec(0x59) == 59);
    CHECK(DS1307::dec2bcd(45) == 0x45);
    for (int v = 0; v < 100; ++v) CHECK(DS1307::bcd2dec(DS1307::dec2bcd(v)) == v);
}

static void test_ds1307_hour_modes() {
    CHECK(DS1307::decodeHour(0x23) == 23);         // 24 h: 23
    CHECK(DS1307::decodeHour(0x40 | 0x12) == 0);   // 12 h: 12 AM -> 0
    CHECK(DS1307::decodeHour(0x60 | 0x07) == 19);  // 12 h: 7 PM -> 19
    CHECK(DS1307::decodeHour(0x60 | 0x12) == 12);  // 12 h: 12 PM -> 12
}

static void test_ds1307_tm_roundtrip() {
    DateTime in{30, 45, 13, 4, 28, 10, 2026};  // Wed 28 Oct 2026 13:45:30
    struct tm t;
    DS1307::toTm(in, t);
    DateTime out = DS1307::fromTm(t);
    CHECK(memcmp(&in, &out, sizeof(in)) == 0);
    CHECK(DS1307::isValid(in));
    DateTime bad = in;
    bad.month = 13;
    CHECK(!DS1307::isValid(bad));
}

static void test_ssd1306_buffer_bounds() {
    SSD1306& d = *s_display;
    d.clear();
    d.setPixel(-1, 0);
    d.setPixel(128, 0);
    d.setPixel(0, 64);
    d.setPixel(5000, -5000);  // must not write outside the 1 KB buffer
    d.setPixel(127, 63);
    CHECK(d.getPixel(127, 63));
    CHECK(!d.getPixel(0, 0));
    d.setPixel(127, 63, false);
    CHECK(!d.getPixel(127, 63));
    CHECK(SSD1306::textWidth("12:00:00", 2) == 94);
    d.clear();
}

// ---------- Spiral 2: time subsystem ----------
static void test_tm_normalisation() {
    // TIME_SET relies on mktime to fix the weekday after manual edits
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 28;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    mktime(&t);
    CHECK(t.tm_wday == 3);  // 28 Oct 2026 is a Wednesday
}

// ---------- Spiral 3: input & UI ----------
static void test_button_long_press_detect() {
    CHECK(ButtonManager::classify(0) == PressType::SHORT);
    CHECK(ButtonManager::classify(999) == PressType::SHORT);
    CHECK(ButtonManager::classify(1000) == PressType::LONG);
    CHECK(ButtonManager::classify(2500) == PressType::LONG);
}

static void test_ui_mode_cycle() {
    Mode m = Mode::CLOCK;
    m = ClockUI::nextMode(m);
    CHECK(m == Mode::ALARM_SET);
    m = ClockUI::nextMode(m);
    CHECK(m == Mode::STOPWATCH);
    m = ClockUI::nextMode(m);
    CHECK(m == Mode::TIME_SET);
    m = ClockUI::nextMode(m);
    CHECK(m == Mode::CLOCK);
}

// ---------- Spiral 4: business engines ----------
static int64_t s_fakeNowUs = 0;
static int64_t fakeClock() { return s_fakeNowUs; }

static void test_alarm_time_comparator() {
    CHECK(AlarmEngine::matches(7, 0, 7, 0));
    CHECK(!AlarmEngine::matches(7, 1, 7, 0));
    CHECK(!AlarmEngine::matches(19, 0, 7, 0));
}

static void test_stopwatch_lap_split_calc() {
    StopwatchEngine sw(fakeClock);
    s_fakeNowUs = 0;
    sw.start();
    s_fakeNowUs = 1500000;  // 1.5 s
    CHECK(sw.recordLap());
    s_fakeNowUs = 2500000;  // 2.5 s
    CHECK(sw.recordLap());
    CHECK(sw.lapUs(0) == 1500000);
    CHECK(sw.lapUs(1) == 2500000);
    CHECK(sw.splitUs(1) == 1000000);

    sw.pause();
    s_fakeNowUs = 9000000;  // time passing while paused must not count
    CHECK(sw.elapsedUs() == 2500000);
    CHECK(!sw.recordLap());  // no laps while paused

    char buf[12];
    StopwatchEngine::format(61230000, buf, sizeof(buf));
    CHECK(strcmp(buf, "01:01.23") == 0);

    sw.reset();
    CHECK(sw.elapsedUs() == 0 && sw.lapCount() == 0);
}

// ---------- Spiral 5: FreeRTOS integration ----------
static void test_freertos_event_queue() {
    QueueHandle_t q = xQueueCreate(2, sizeof(ButtonEvent));
    CHECK(q != nullptr);
    if (!q) return;
    ButtonEvent tx{ButtonId::LAP, PressType::LONG}, rx{};
    CHECK(xQueueSend(q, &tx, 0) == pdTRUE);
    CHECK(xQueueReceive(q, &rx, 0) == pdTRUE);
    CHECK(rx.id == ButtonId::LAP && rx.type == PressType::LONG);
    CHECK(xQueueReceive(q, &rx, 0) == pdFALSE);  // empty queue does not block
    vQueueDelete(q);
}

int runSelfTests(SSD1306& scratchDisplay) {
    s_display = &scratchDisplay;
    s_failed = 0;
    s_assertions = 0;

    ESP_LOGI(TAG, "========== Spiral unit self-test ==========");
    ESP_LOGI(TAG, "-- Spiral 1: HAL & drivers");
    runTest("test_ds1307_bcd_conversion", test_ds1307_bcd_conversion);
    runTest("test_ds1307_hour_modes", test_ds1307_hour_modes);
    runTest("test_ds1307_tm_roundtrip", test_ds1307_tm_roundtrip);
    runTest("test_ssd1306_buffer_bounds", test_ssd1306_buffer_bounds);
    ESP_LOGI(TAG, "-- Spiral 2: time subsystem");
    runTest("test_tm_normalisation", test_tm_normalisation);
    ESP_LOGI(TAG, "-- Spiral 3: input & UI");
    runTest("test_button_long_press_detect", test_button_long_press_detect);
    runTest("test_ui_mode_cycle", test_ui_mode_cycle);
    ESP_LOGI(TAG, "-- Spiral 4: engines");
    runTest("test_alarm_time_comparator", test_alarm_time_comparator);
    runTest("test_stopwatch_lap_split_calc", test_stopwatch_lap_split_calc);
    ESP_LOGI(TAG, "-- Spiral 5: FreeRTOS");
    runTest("test_freertos_event_queue", test_freertos_event_queue);

    ESP_LOGI(TAG, "%d assertions, %d failed -> %s", s_assertions, s_failed,
             s_failed == 0 ? "ALL SPIRALS PASSED" : "FAILURES");
    return s_failed;
}

}  // namespace Clock
