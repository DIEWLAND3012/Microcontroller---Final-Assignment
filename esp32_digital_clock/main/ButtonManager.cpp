// ButtonManager.cpp - falling-edge ISR feeds a task that debounces and times presses
#include "ButtonManager.hpp"
#include "Config.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace Clock {

static const char* TAG = "Buttons";

ButtonManager::ButtonManager(const gpio_num_t (&pins)[NUM_BUTTONS]) {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        pins_[i] = pins[i];
        isrArgs_[i] = {this, static_cast<uint8_t>(i)};
    }
}

bool ButtonManager::init() {
    isrQueue_ = xQueueCreate(16, sizeof(uint8_t));
    eventQueue_ = xQueueCreate(10, sizeof(ButtonEvent));
    if (!isrQueue_ || !eventQueue_) return false;

    gpio_config_t io = {};
    for (gpio_num_t pin : pins_) io.pin_bit_mask |= 1ULL << pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_NEGEDGE;  // active-low: press = falling edge
    ESP_ERROR_CHECK(gpio_config(&io));

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;  // already installed is fine

    for (int i = 0; i < NUM_BUTTONS; ++i) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(pins_[i], isrHandler, &isrArgs_[i]));
        ESP_LOGI(TAG, "%-5s on GPIO %d", name(static_cast<ButtonId>(i)), pins_[i]);
    }
    return true;
}

void IRAM_ATTR ButtonManager::isrHandler(void* arg) {
    auto* a = static_cast<IsrArg*>(arg);
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(a->self->isrQueue_, &a->index, &woken);
    if (woken) portYIELD_FROM_ISR();
}

PressType ButtonManager::classify(int64_t heldMs) {
    return heldMs >= Config::LONG_PRESS_MS ? PressType::LONG : PressType::SHORT;
}

void ButtonManager::emit(int index, PressType type) {
    ButtonEvent ev{static_cast<ButtonId>(index), type};
    ESP_LOGI(TAG, "%s %s", name(ev.id), type == PressType::LONG ? "LONG" : "SHORT");
    xQueueSend(eventQueue_, &ev, 0);
}

void ButtonManager::taskLoop() {
    uint8_t index;
    for (;;) {
        if (xQueueReceive(isrQueue_, &index, portMAX_DELAY) != pdTRUE) continue;
        if (index >= NUM_BUTTONS) continue;

        // Debounce: the level must still be low after the bounce window
        vTaskDelay(pdMS_TO_TICKS(Config::DEBOUNCE_MS));
        if (!isPressed(index)) continue;

        // Track the hold time; LONG fires as soon as the threshold is reached
        const int64_t t0 = esp_timer_get_time();
        bool longSent = false;
        while (isPressed(index)) {
            int64_t heldMs = (esp_timer_get_time() - t0) / 1000;
            if (!longSent && classify(heldMs) == PressType::LONG) {
                emit(index, PressType::LONG);
                longSent = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (!longSent) emit(index, PressType::SHORT);

        // Let the release bounce settle, then drop the edges it produced
        vTaskDelay(pdMS_TO_TICKS(Config::DEBOUNCE_MS));
        xQueueReset(isrQueue_);
    }
}

const char* ButtonManager::name(ButtonId id) {
    switch (id) {
        case ButtonId::MODE: return "MODE";
        case ButtonId::SET: return "SET";
        case ButtonId::ALARM: return "ALARM";
        case ButtonId::LAP: return "LAP";
        default: return "?";
    }
}

}  // namespace Clock
