// BSP/Inc/GuiEngine.hpp
#pragma once
#include "ILcdDisplay.hpp"
#include <stdint.h>

namespace Bsp {

class GuiEngine {
public:
    explicit GuiEngine(App::ILcdDisplay& lcd) : m_lcd(lcd) {}

    uint16_t width() const { return m_lcd.getWidth(); }
    uint16_t height() const { return m_lcd.getHeight(); }

    // 画点（透传到硬件层）
    void drawPixel(uint16_t x, uint16_t y, uint16_t color);

    // 画线（Bresenham 整数算法）
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

    // 画圆（中点画圆 + 8-way 对称；fill=true 时内部填充）
    void drawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color, bool fill);

    // 矩形边框（4 条 1 像素线）
    void drawRectBorder(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

    // 三角形（border: 3×drawLine; fill: edge-scan）
    void drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                      uint16_t x2, uint16_t y2, uint16_t color, bool fill);

private:
    App::ILcdDisplay& m_lcd;

    void drawCirclePoints(int16_t cx, int16_t cy, int16_t x, int16_t y, uint16_t color);
    void drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color);
    void drawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);
};

} // namespace Bsp
