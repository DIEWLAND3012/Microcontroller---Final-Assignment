// I2CBus.hpp - Thread-safe wrapper around the ESP-IDF v6 i2c_master driver
#pragma once
#include <cstddef>
#include <cstdint>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Clock {

class I2CBus {
public:
    static constexpr int TIMEOUT_MS = 100;

    I2CBus(i2c_port_num_t port, gpio_num_t sda, gpio_num_t scl, uint32_t speedHz);

    // The mutex is created here, not in the constructor: global objects are
    // constructed before app_main and must not depend on FreeRTOS state.
    esp_err_t init();

    i2c_master_dev_handle_t addDevice(uint8_t addr);
    bool probe(uint8_t addr);

    esp_err_t write(i2c_master_dev_handle_t dev, const uint8_t* data, size_t len);
    esp_err_t writeRead(i2c_master_dev_handle_t dev, const uint8_t* tx, size_t txLen,
                        uint8_t* rx, size_t rxLen);

private:
    // RAII guard so every early return releases the bus
    class Lock {
    public:
        explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
        ~Lock() { xSemaphoreGive(m_); }
    private:
        SemaphoreHandle_t m_;
    };

    i2c_port_num_t port_;
    gpio_num_t sdaPin_;
    gpio_num_t sclPin_;
    uint32_t speedHz_;
    i2c_master_bus_handle_t busHandle_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};
};

}  // namespace Clock
