// AlarmEngine.cpp - checks the alarm every 250 ms and beeps the buzzer while ringing
#include "AlarmEngine.hpp"
#include "Config.hpp"
#include "TimeManager.hpp"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Clock {

static const char* TAG = "Alarm";

static constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
static constexpr ledc_channel_t LEDC_CH = LEDC_CHANNEL_0;
static constexpr uint32_t DUTY_50 = 512;  // 50 % of 10-bit resolution

bool AlarmEngine::init() {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER;
    timer.freq_hz = toneHz_;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) return false;

    ledc_channel_config_t ch = {};
    ch.gpio_num = pin_;
    ch.speed_mode = LEDC_MODE;
    ch.channel = LEDC_CH;
    ch.timer_sel = LEDC_TIMER;
    ch.duty = 0;
    ch.hpoint = 0;
    if (ledc_channel_config(&ch) != ESP_OK) return false;

    ESP_LOGI(TAG, "Buzzer on GPIO %d (%lu Hz PWM)", pin_, (unsigned long)toneHz_);
    return true;
}

void AlarmEngine::tone(bool on) {
    if (on == toneOn_) return;
    toneOn_ = on;
    ledc_set_duty(LEDC_MODE, LEDC_CH, on ? DUTY_50 : 0);
    ledc_update_duty(LEDC_MODE, LEDC_CH);
}

void AlarmEngine::stop() {
    if (ringing_.exchange(false)) ESP_LOGI(TAG, "Alarm stopped");
}

void AlarmEngine::taskLoop(TimeManager& time) {
    uint32_t tick = 0;
    for (;;) {
        struct tm now;
        time.now(now);
        const int stamp = now.tm_yday * 1440 + now.tm_hour * 60 + now.tm_min;

        if (!ringing_ && enabled_ && stamp != lastTriggerStamp_ &&
            matches(now.tm_hour, now.tm_min, hour_, minute_)) {
            lastTriggerStamp_ = stamp;
            ringStartUs_ = esp_timer_get_time();
            ringing_ = true;
            ESP_LOGW(TAG, "ALARM %02d:%02d ringing!", hour_.load(), minute_.load());
        }

        if (ringing_ &&
            esp_timer_get_time() - ringStartUs_ > Config::ALARM_RING_TIMEOUT_S * 1000000LL) {
            ESP_LOGI(TAG, "Alarm timed out");
            ringing_ = false;
        }

        // Beep pattern: 250 ms on / 250 ms off while ringing
        tone(ringing_ && (tick % 2 == 0));
        ++tick;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

}  // namespace Clock
