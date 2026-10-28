// Config.hpp - Board pin map and system constants (single source of truth)
#pragma once
#include "driver/gpio.h"
#include "sdkconfig.h"

namespace Clock::Config {

// I2C bus 0 shared by DS1307 (0x68) and SSD1306 (0x3C)
constexpr int        I2C_PORT     = 0;
constexpr gpio_num_t I2C_SDA      = GPIO_NUM_21;
constexpr gpio_num_t I2C_SCL      = GPIO_NUM_22;
constexpr uint32_t   I2C_SPEED_HZ = 400000;
constexpr uint8_t    OLED_ADDR    = 0x3C;

// Push buttons (active-low, internal pull-up). GPIO12 is avoided on purpose:
// it is the MTDI strapping pin that selects flash voltage at boot.
constexpr gpio_num_t BTN_MODE  = GPIO_NUM_18;
constexpr gpio_num_t BTN_SET   = GPIO_NUM_19;
constexpr gpio_num_t BTN_ALARM = GPIO_NUM_4;
constexpr gpio_num_t BTN_LAP   = GPIO_NUM_5;

// Piezo buzzer driven by LEDC PWM
constexpr gpio_num_t BUZZER     = GPIO_NUM_25;
constexpr uint32_t   BUZZER_HZ  = 2000;

// Network / time
constexpr const char* WIFI_SSID    = CONFIG_CLOCK_WIFI_SSID;
constexpr const char* WIFI_PASS    = CONFIG_CLOCK_WIFI_PASSWORD;
constexpr uint8_t     WIFI_CHANNEL = CONFIG_CLOCK_WIFI_CHANNEL;
constexpr const char* NTP_SERVER   = CONFIG_CLOCK_NTP_SERVER;
constexpr const char* TIMEZONE     = CONFIG_CLOCK_TIMEZONE;

// Input timing
constexpr int DEBOUNCE_MS   = 30;
constexpr int LONG_PRESS_MS = 1000;

// Alarm rings at most this long if nobody presses a button
constexpr int ALARM_RING_TIMEOUT_S = 60;

}  // namespace Clock::Config
