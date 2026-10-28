// main.cpp - ESP32 Modular Digital Clock (ESP-IDF v6, C++ OOP, FreeRTOS)
//
//  Layer 5  FreeRTOS tasks : vTaskUI, vTaskButton, vTaskAlarm, vTaskNTP
//  Layer 4  Engines        : TimeManager, AlarmEngine, StopwatchEngine
//  Layer 3  Presentation   : ClockUI (state machine), ButtonManager
//  Layer 2  Drivers        : DS1307, SSD1306
//  Layer 1  Bus            : I2CBus (thread-safe i2c_master wrapper)

#include "AlarmEngine.hpp"
#include "ButtonManager.hpp"
#include "ClockUI.hpp"
#include "Config.hpp"
#include "DS1307.hpp"
#include "I2CBus.hpp"
#include "SSD1306.hpp"
#include "SelfTest.hpp"
#include "StopwatchEngine.hpp"
#include "TimeManager.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

using namespace Clock;

static const char* TAG = "MAIN";

static const gpio_num_t BUTTON_PINS[ButtonManager::NUM_BUTTONS] = {
    Config::BTN_MODE, Config::BTN_SET, Config::BTN_ALARM, Config::BTN_LAP};

// Constructors only store references/values; all hardware work happens in app_main
static I2CBus i2cBus(Config::I2C_PORT, Config::I2C_SDA, Config::I2C_SCL, Config::I2C_SPEED_HZ);
static DS1307 rtc(i2cBus);
static SSD1306 display(i2cBus, Config::OLED_ADDR);
static ButtonManager buttons(BUTTON_PINS);
static TimeManager timeMgr(rtc);
static AlarmEngine alarmEngine(Config::BUZZER, Config::BUZZER_HZ);
static StopwatchEngine stopwatch;
static ClockUI ui(display, timeMgr, alarmEngine, stopwatch);

// Task entry points (FreeRTOS needs plain functions; each loop never returns)
static void vTaskButton(void*) { buttons.taskLoop(); }
static void vTaskNTP(void*) { timeMgr.taskNtpLoop(); }
static void vTaskAlarm(void*) { alarmEngine.taskLoop(timeMgr); }
static void vTaskUI(void*) { ui.taskLoop(buttons.eventQueue()); }

static void initNvs() {
    esp_err_t err = nvs_flash_init();  // required by the Wi-Fi driver
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32 Modular Digital Clock starting");
    initNvs();

    // Spiral 1: bus and drivers
    ESP_ERROR_CHECK(i2cBus.init());
    rtc.init();
    display.init();

#if CONFIG_CLOCK_RUN_SELF_TEST
    runSelfTests(display);
#endif

    // Spiral 2: time subsystem (RTC first, NTP refines it when online)
    timeMgr.initSystemTime(Config::TIMEZONE);
#if CONFIG_CLOCK_ENABLE_WIFI
    if (!timeMgr.startWifi(Config::WIFI_SSID, Config::WIFI_PASS, Config::WIFI_CHANNEL,
                           Config::NTP_SERVER)) {
        ESP_LOGW(TAG, "Wi-Fi unavailable, running on DS1307 time only");
    }
#else
    ESP_LOGW(TAG, "Wi-Fi disabled in menuconfig, running on DS1307 time only");
#endif

    // Spiral 3 & 4: input and engines
    ESP_ERROR_CHECK(buttons.init() ? ESP_OK : ESP_FAIL);
    if (!alarmEngine.init()) ESP_LOGW(TAG, "Buzzer init failed");

    // Spiral 5: FreeRTOS tasks       name           stack  arg      prio  handle   core
    xTaskCreatePinnedToCore(vTaskButton, "vTaskButton", 3072, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(vTaskNTP,    "vTaskNTP",    4096, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(vTaskAlarm,  "vTaskAlarm",  3072, nullptr, 4, nullptr, 1);
    xTaskCreatePinnedToCore(vTaskUI,     "vTaskUI",     4096, nullptr, 3, nullptr, 1);

    ESP_LOGI(TAG, "All tasks running, free heap %u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)));
    // app_main returns here; the main task is deleted and the four tasks keep running
}
