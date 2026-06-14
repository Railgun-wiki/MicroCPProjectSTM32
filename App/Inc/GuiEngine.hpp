#pragma once
#include "ILcdDisplay.hpp"
#include <stdint.h>

namespace App {

class GuiEngine {
public:
    explicit GuiEngine(ILcdDisplay& lcd) : m_lcd(lcd) {}

    uint16_t width() const { return m_lcd.getWidth(); }
    uint16_t height() const { return m_lcd.getHeight(); }

    void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void drawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color, bool fill);
    void drawRectBorder(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                      uint16_t x2, uint16_t y2, uint16_t color, bool fill);

private:
    ILcdDisplay& m_lcd;

    void drawCirclePoints(int16_t cx, int16_t cy, int16_t x, int16_t y, uint16_t color);
    void drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color);
    void drawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);
};

} // namespace App
