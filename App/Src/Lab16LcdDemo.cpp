#include "Lab16LcdDemo.hpp"

namespace App {

namespace {

constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kBlue = 0x001F;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kMagenta = 0xF81F;
constexpr uint16_t kCyan = 0x07FF;

constexpr uint16_t kPalette[] = {
    kRed,
    kGreen,
    kBlue,
    kYellow,
    kMagenta,
};

constexpr uint16_t kToolbarHeight = 24;
constexpr uint16_t kRstWidth = 28;
constexpr uint16_t kSwatchX0 = 56;
constexpr uint16_t kSwatchY0 = 2;
constexpr uint16_t kSwatchX1 = 76;
constexpr uint16_t kSwatchY1 = 18;
constexpr uint16_t kBrushRadius = 5;

} // namespace

Lab16LcdDemo::Lab16LcdDemo(Bsp::LcdBsp& lcd, Bsp::GuiEngine& gui, Bsp::TouchBsp& touch)
    : m_lcd(lcd)
    , m_gui(gui)
    , m_touch(touch)
{
}

void Lab16LcdDemo::init()
{
    m_lcd.clear(kWhite);
    drawChrome();
}

void Lab16LcdDemo::loop()
{
    TouchPoint point{};
    const bool touched = m_touch.readPosition(point) && point.valid;

    if (touched) {
        handlePoint(point.x, point.y, !m_wasTouched);
    }

    m_wasTouched = touched;
}

void Lab16LcdDemo::drawChrome()
{
    m_lcd.fillRect(0, 0, m_lcd.getWidth() - 1, kToolbarHeight - 1, kBlue);
    m_lcd.drawString(m_lcd.getWidth() - kRstWidth, 0, "RST", kRed, kBlue, 16);
    drawBrushPreview();
}

void Lab16LcdDemo::resetCanvas()
{
    m_lcd.clear(kWhite);
    drawChrome();
}

void Lab16LcdDemo::handlePoint(uint16_t x, uint16_t y, bool newPress)
{
    if (x >= m_lcd.getWidth() || y >= m_lcd.getHeight()) {
        return;
    }

    const bool inRst = (x >= m_lcd.getWidth() - kRstWidth) && (y < 16);
    if (newPress && inRst) {
        resetCanvas();
        return;
    }

    const bool inSwatch = (x >= kSwatchX0) && (x <= kSwatchX1) &&
                          (y >= kSwatchY0) && (y <= kSwatchY1);
    if (newPress && inSwatch) {
        m_colorIndex = static_cast<uint8_t>((m_colorIndex + 1) %
                                            (sizeof(kPalette) / sizeof(kPalette[0])));
        m_penColor = kPalette[m_colorIndex];
        drawBrushPreview();
        return;
    }

    if (y >= kToolbarHeight) {
        m_gui.drawCircle(x, y, kBrushRadius, m_penColor, true);
    }
}

void Lab16LcdDemo::drawBrushPreview()
{
    m_lcd.fillRect(kSwatchX0 - 4, kSwatchY0, kSwatchX1 + 4, kSwatchY1, kBlue);
    m_lcd.fillRect(kSwatchX0, kSwatchY0, kSwatchX1, kSwatchY1, m_penColor);
    m_gui.drawRectBorder(kSwatchX0, kSwatchY0, kSwatchX1, kSwatchY1, kWhite);
    m_lcd.drawString(2, 0, "PEN", kWhite, kBlue, 16);
    m_lcd.fillRect(34, 6, 46, 12, m_penColor);
    m_gui.drawRectBorder(34, 6, 46, 12, kBlack);
}

} // namespace App
