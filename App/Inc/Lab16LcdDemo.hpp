#pragma once

#include "GuiEngine.hpp"
#include "LcdBsp.hpp"
#include "TouchBsp.hpp"

namespace App {

class Lab16LcdDemo {
public:
    Lab16LcdDemo(Bsp::LcdBsp& lcd, Bsp::GuiEngine& gui, Bsp::TouchBsp& touch);

    void init();
    void loop();

private:
    Bsp::LcdBsp& m_lcd;
    Bsp::GuiEngine& m_gui;
    Bsp::TouchBsp& m_touch;

    uint16_t m_penColor{0xF800};
    uint8_t m_colorIndex{0};
    bool m_wasTouched{false};

    void drawChrome();
    void resetCanvas();
    void handlePoint(uint16_t x, uint16_t y, bool newPress);
    void drawBrushPreview();
};

} // namespace App
