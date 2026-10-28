// ButtonManager.hpp - GPIO ISR + task-level debounce with short/long press detection
#pragma once
#include <cstdint>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace Clock {

enum class ButtonId : uint8_t { MODE, SET, ALARM, LAP, COUNT };
enum class PressType : uint8_t { SHORT, LONG };

struct ButtonEvent {
    ButtonId id;
    PressType type;
};

class ButtonManager {
public:
    static constexpr int NUM_BUTTONS = static_cast<int>(ButtonId::COUNT);

    // pins[] is indexed by ButtonId
    explicit ButtonManager(const gpio_num_t (&pins)[NUM_BUTTONS]);

    bool init();
    [[noreturn]] void taskLoop();  // FreeRTOS task body; must never return
    QueueHandle_t eventQueue() const { return eventQueue_; }

    static PressType classify(int64_t heldMs);
    static const char* name(ButtonId id);

private:
    struct IsrArg {
        ButtonManager* self;
        uint8_t index;
    };
    static void IRAM_ATTR isrHandler(void* arg);
    bool isPressed(int index) const { return gpio_get_level(pins_[index]) == 0; }
    void emit(int index, PressType type);

    gpio_num_t pins_[NUM_BUTTONS];
    IsrArg isrArgs_[NUM_BUTTONS];
    QueueHandle_t isrQueue_{nullptr};    // raw edges from the ISR (button index)
    QueueHandle_t eventQueue_{nullptr};  // debounced ButtonEvent for the UI
};

}  // namespace Clock
