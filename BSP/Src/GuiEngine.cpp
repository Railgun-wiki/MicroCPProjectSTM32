// BSP/Src/GuiEngine.cpp
#include "GuiEngine.hpp"
#include <stdlib.h>

namespace Bsp {

void GuiEngine::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    m_lcd.drawPixel(x, y, color);
}

void GuiEngine::drawCirclePoints(int16_t cx, int16_t cy, int16_t x, int16_t y, uint16_t color)
{
    m_lcd.drawPixel(cx + x, cy + y, color);
    m_lcd.drawPixel(cx - x, cy + y, color);
    m_lcd.drawPixel(cx + x, cy - y, color);
    m_lcd.drawPixel(cx - x, cy - y, color);
    m_lcd.drawPixel(cx + y, cy + x, color);
    m_lcd.drawPixel(cx - y, cy + x, color);
    m_lcd.drawPixel(cx + y, cy - x, color);
    m_lcd.drawPixel(cx - y, cy - x, color);
}

void GuiEngine::drawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color, bool fill)
{
    int16_t x = 0;
    int16_t y = static_cast<int16_t>(r);
    int16_t d = 1 - static_cast<int16_t>(r);

    while (x <= y) {
        if (fill) {
            // 填充：每对对称点画水平条带
            m_lcd.fillRect(xc - y, yc - x, xc + y, yc - x, color);
            m_lcd.fillRect(xc - y, yc + x, xc + y, yc + x, color);
            m_lcd.fillRect(xc - x, yc - y, xc + x, yc - y, color);
            m_lcd.fillRect(xc - x, yc + y, xc + x, yc + y, color);
        } else {
            drawCirclePoints(xc, yc, x, y, color);
        }
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void GuiEngine::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int16_t dx = abs(static_cast<int16_t>(x1) - static_cast<int16_t>(x0));
    int16_t dy = -abs(static_cast<int16_t>(y1) - static_cast<int16_t>(y0));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;
    int16_t cx = static_cast<int16_t>(x0);
    int16_t cy = static_cast<int16_t>(y0);

    while (true) {
        m_lcd.drawPixel(static_cast<uint16_t>(cx), static_cast<uint16_t>(cy), color);
        if (cx == static_cast<int16_t>(x1) && cy == static_cast<int16_t>(y1)) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }
}

void GuiEngine::drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint16_t color)
{
    m_lcd.fillRect(x0, y, x1, y, color);
}

void GuiEngine::drawVLine(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
    m_lcd.fillRect(x, y0, x, y1, color);
}

void GuiEngine::drawRectBorder(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    drawHLine(x0, x1, y0, color);           // 顶边
    drawHLine(x0, x1, y1, color);           // 底边
    drawVLine(x0, y0 + 1, y1 - 1, color);   // 左边
    drawVLine(x1, y0 + 1, y1 - 1, color);   // 右边
}

static void swapU16(uint16_t* a, uint16_t* b)
{
    uint16_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void GuiEngine::drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                              uint16_t x2, uint16_t y2, uint16_t color, bool fill)
{
    if (!fill) {
        drawLine(x0, y0, x1, y1, color);
        drawLine(x1, y1, x2, y2, color);
        drawLine(x2, y2, x0, y0, color);
        return;
    }

    // 按 Y 排序顶点，y0 <= y1 <= y2
    if (y0 > y1) { swapU16(&y0, &y1); swapU16(&x0, &x1); }
    if (y1 > y2) { swapU16(&y2, &y1); swapU16(&x2, &x1); }
    if (y0 > y1) { swapU16(&y0, &y1); swapU16(&x0, &x1); }

    if (y0 == y2) return; // 退化三角

    int32_t dx01 = static_cast<int32_t>(x1) - static_cast<int32_t>(x0);
    int32_t dy01 = static_cast<int32_t>(y1) - static_cast<int32_t>(y0);
    int32_t dx02 = static_cast<int32_t>(x2) - static_cast<int32_t>(x0);
    int32_t dy02 = static_cast<int32_t>(y2) - static_cast<int32_t>(y0);
    int32_t dx12 = static_cast<int32_t>(x2) - static_cast<int32_t>(x1);
    int32_t dy12 = static_cast<int32_t>(y2) - static_cast<int32_t>(y1);

    int32_t sa = 0, sb = 0;
    uint16_t last = (y1 == y2) ? y1 : static_cast<uint16_t>(y1 - 1);

    // 上半部分
    for (uint16_t y = y0; y <= last; y++) {
        int32_t a = static_cast<int32_t>(x0) + sa / dy01;
        int32_t b = static_cast<int32_t>(x0) + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) { int32_t t = a; a = b; b = t; }
        drawHLine(a, b, y, color);
    }

    // 下半部分
    sa = dx12 * static_cast<int32_t>(last + 1 - y1);
    sb = dx02 * static_cast<int32_t>(last + 1 - y0);
    for (uint16_t y = last + 1; y <= y2; y++) {
        int32_t a = static_cast<int32_t>(x1) + sa / dy12;
        int32_t b = static_cast<int32_t>(x0) + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) { int32_t t = a; a = b; b = t; }
        drawHLine(a, b, y, color);
    }
}

} // namespace Bsp
