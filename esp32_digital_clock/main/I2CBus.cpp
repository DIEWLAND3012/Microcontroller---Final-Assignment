// I2CBus.cpp - I2C master bus implementation
#include "I2CBus.hpp"
#include "esp_log.h"

namespace Clock {

static const char* TAG = "I2CBus";

I2CBus::I2CBus(i2c_port_num_t port, gpio_num_t sda, gpio_num_t scl, uint32_t speedHz)
    : port_(port), sdaPin_(sda), sclPin_(scl), speedHz_(speedHz) {}

esp_err_t I2CBus::init() {
    if (busHandle_) return ESP_OK;

    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) return ESP_ERR_NO_MEM;

    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = port_;
    cfg.sda_io_num = sdaPin_;
    cfg.scl_io_num = sclPin_;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&cfg, &busHandle_);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "I2C%d ready (SDA=%d, SCL=%d, %lu Hz)", (int)port_, sdaPin_, sclPin_,
                 (unsigned long)speedHz_);
    } else {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    }
    return err;
}

i2c_master_dev_handle_t I2CBus::addDevice(uint8_t addr) {
    if (!busHandle_) return nullptr;

    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = addr;
    dev.scl_speed_hz = speedHz_;

    i2c_master_dev_handle_t handle = nullptr;
    if (i2c_master_bus_add_device(busHandle_, &dev, &handle) != ESP_OK) return nullptr;
    return handle;
}

bool I2CBus::probe(uint8_t addr) {
    if (!busHandle_) return false;
    Lock lock(mutex_);
    return i2c_master_probe(busHandle_, addr, TIMEOUT_MS) == ESP_OK;
}

esp_err_t I2CBus::write(i2c_master_dev_handle_t dev, const uint8_t* data, size_t len) {
    if (!dev) return ESP_ERR_INVALID_STATE;
    Lock lock(mutex_);
    return i2c_master_transmit(dev, data, len, TIMEOUT_MS);
}

esp_err_t I2CBus::writeRead(i2c_master_dev_handle_t dev, const uint8_t* tx, size_t txLen,
                            uint8_t* rx, size_t rxLen) {
    if (!dev) return ESP_ERR_INVALID_STATE;
    Lock lock(mutex_);
    return i2c_master_transmit_receive(dev, tx, txLen, rx, rxLen, TIMEOUT_MS);
}

}  // namespace Clock
