// SSD1306.hpp - 128x64 monochrome OLED driver with a 1 KB frame buffer
#pragma once
#include <cstdint>
#include "I2CBus.hpp"

namespace Clock {

class SSD1306 {
public:
    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int BUFFER_SIZE = WIDTH * HEIGHT / 8;  // 1024 bytes
    static constexpr int CHAR_W = 6;                        // 5 px glyph + 1 px gap
    static constexpr int CHAR_H = 8;

    SSD1306(I2CBus& bus, uint8_t addr) : bus_(bus), addr_(addr) {}

    bool init();
    bool isPresent() const { return dev_ != nullptr; }

    void clear();
    void setPixel(int x, int y, bool on = true);
    bool getPixel(int x, int y) const;
    void fillRect(int x, int y, int w, int h, bool on = true);
    void drawChar(int x, int y, char c, int scale = 1, bool on = true);
    void drawString(int x, int y, const char* str, int scale = 1, bool on = true);
    void drawStringCentered(int y, const char* str, int scale = 1, bool on = true);
    static int textWidth(const char* str, int scale = 1);

    bool flush();  // send the frame buffer over I2C

private:
    bool sendCommands(const uint8_t* cmds, size_t len);

    I2CBus& bus_;
    uint8_t addr_;
    i2c_master_dev_handle_t dev_{nullptr};
    // [0] = 0x40 control byte (data stream), [1..1024] = frame buffer
    uint8_t tx_[BUFFER_SIZE + 1]{};
    uint8_t* const buffer_ = tx_ + 1;
};

}  // namespace Clock
