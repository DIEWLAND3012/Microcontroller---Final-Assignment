// SSD1306.cpp - OLED init sequence, drawing primitives and frame flush
#include "SSD1306.hpp"
#include <cstring>
#include "Font5x7.hpp"
#include "esp_log.h"

namespace Clock {

static const char* TAG = "SSD1306";

bool SSD1306::init() {
    if (!bus_.probe(addr_)) {
        ESP_LOGW(TAG, "No SSD1306 found at 0x%02X", addr_);
        return false;
    }
    dev_ = bus_.addDevice(addr_);
    if (!dev_) return false;

    static const uint8_t initSeq[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide ratio
        0xA8, 0x3F,  // multiplex = 64 rows
        0xD3, 0x00,  // display offset 0
        0x40,        // start line 0
        0x8D, 0x14,  // enable charge pump
        0x20, 0x00,  // horizontal addressing mode
        0xA1,        // segment remap (column 127 -> SEG0)
        0xC8,        // COM scan descending
        0xDA, 0x12,  // COM pins config for 128x64
        0x81, 0xCF,  // contrast
        0xD9, 0xF1,  // pre-charge period
        0xDB, 0x40,  // VCOMH deselect level
        0xA4,        // display follows RAM
        0xA6,        // normal (not inverted)
        0x2E,        // scrolling off
        0xAF,        // display on
    };
    if (!sendCommands(initSeq, sizeof(initSeq))) return false;

    tx_[0] = 0x40;
    clear();
    flush();
    ESP_LOGI(TAG, "SSD1306 128x64 ready at 0x%02X", addr_);
    return true;
}

bool SSD1306::sendCommands(const uint8_t* cmds, size_t len) {
    uint8_t tx[32];
    if (len + 1 > sizeof(tx)) return false;
    tx[0] = 0x00;  // control byte: command stream
    memcpy(tx + 1, cmds, len);
    return bus_.write(dev_, tx, len + 1) == ESP_OK;
}

void SSD1306::clear() {
    memset(buffer_, 0, BUFFER_SIZE);
}

void SSD1306::setPixel(int x, int y, bool on) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    uint8_t& cell = buffer_[x + (y / 8) * WIDTH];
    const uint8_t mask = 1u << (y % 8);
    if (on) cell |= mask;
    else cell &= ~mask;
}

bool SSD1306::getPixel(int x, int y) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
    return buffer_[x + (y / 8) * WIDTH] & (1u << (y % 8));
}

void SSD1306::fillRect(int x, int y, int w, int h, bool on) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) setPixel(xx, yy, on);
}

void SSD1306::drawChar(int x, int y, char c, int scale, bool on) {
    uint8_t code = static_cast<uint8_t>(c);
    if (code < FONT_FIRST || code > FONT_LAST) code = '?';
    const uint8_t* glyph = FONT5X7[code - FONT_FIRST];
    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 8; ++row) {
            if (glyph[col] & (1u << row)) {
                fillRect(x + col * scale, y + row * scale, scale, scale, on);
            }
        }
    }
}

void SSD1306::drawString(int x, int y, const char* str, int scale, bool on) {
    for (; *str; ++str, x += CHAR_W * scale) drawChar(x, y, *str, scale, on);
}

void SSD1306::drawStringCentered(int y, const char* str, int scale, bool on) {
    drawString((WIDTH - textWidth(str, scale)) / 2, y, str, scale, on);
}

int SSD1306::textWidth(const char* str, int scale) {
    int n = static_cast<int>(strlen(str));
    return n > 0 ? (n * CHAR_W - 1) * scale : 0;  // no gap after the last glyph
}

bool SSD1306::flush() {
    if (!dev_) return false;
    static const uint8_t window[] = {
        0x21, 0x00, 0x7F,  // column address 0..127
        0x22, 0x00, 0x07,  // page address 0..7
    };
    if (!sendCommands(window, sizeof(window))) return false;
    return bus_.write(dev_, tx_, sizeof(tx_)) == ESP_OK;
}

}  // namespace Clock
