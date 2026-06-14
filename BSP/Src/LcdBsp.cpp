// BSP/Src/LcdBsp.cpp
#include "LcdBsp.hpp"
#include "LcdFont.hpp"
#include "GuiEngine.hpp"
#include <stdio.h>

namespace Bsp {

namespace {

constexpr uint32_t kSpiWaitTimeout = 100000U;

static bool waitForSpiFlagSet(SPI_TypeDef* SPIx, uint32_t flag)
{
    uint32_t timeout = kSpiWaitTimeout;
    while ((SPIx->SR & flag) == 0U) {
        if (--timeout == 0U) {
            return false;
        }
    }
    return true;
}

// 定点数转整型字符串方法（规避对 %f 的支持限制）
static void formatScaledInt(char* dest, const char* prefix, int32_t val, const char* suffix, int divisor)
{
    int32_t absVal = val < 0 ? -val : val;
    int32_t intPart = absVal / divisor;
    int32_t fracPart = absVal % divisor;
    
    if (divisor == 10) {
        sprintf(dest, "%s%s%ld.%ld%s", prefix, val < 0 ? "-" : "", (long)intPart, (long)fracPart, suffix);
    } else if (divisor == 100) {
        sprintf(dest, "%s%s%ld.%02ld%s", prefix, val < 0 ? "-" : "", (long)intPart, (long)fracPart, suffix);
    } else {
        sprintf(dest, "%s%ld%s", prefix, (long)val, suffix);
    }
}

static void formatPressureKpa(char* dest, const char* prefix, uint32_t pressurePa, const char* suffix)
{
    const uint32_t whole = pressurePa / 1000U;
    const uint32_t fract = (pressurePa % 1000U) / 100U;
    sprintf(dest, "%s%lu.%01lu%s", prefix, (unsigned long)whole, (unsigned long)fract, suffix);
}

static uint16_t graphYForValue(int32_t value, int32_t minValue, int32_t maxValue,
                               uint16_t y0, uint16_t y1)
{
    int32_t range = maxValue - minValue;
    if (range <= 0) {
        range = 1;
    }

    const uint16_t height = y1 - y0;
    int64_t scaled = static_cast<int64_t>(value - minValue) * height / range;
    if (scaled < 0) {
        scaled = 0;
    } else if (scaled > height) {
        scaled = height;
    }
    return static_cast<uint16_t>(y1 - scaled);
}

static uint16_t graphXForIndex(uint8_t index, uint8_t count, uint16_t x0, uint16_t x1)
{
    if (count < 2) {
        return x0;
    }
    return static_cast<uint16_t>(x0 + (static_cast<uint32_t>(x1 - x0) * index) / (count - 1U));
}

static void drawBoxedButton(LcdBsp& lcd, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                            const char* label, uint16_t fg, uint16_t bg)
{
    lcd.fillRect(x0, y0, x1, y1, bg);
    lcd.gui().drawRectBorder(x0, y0, x1, y1, fg);
    lcd.drawString(static_cast<uint16_t>(x0 + 10U), static_cast<uint16_t>(y0 + 8U), label, fg, bg, 16);
}

static void drawSeriesGraph(LcdBsp& lcd, Bsp::GuiEngine& gui, const int32_t* values,
                            uint8_t count, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1, int32_t minValue,
                            int32_t maxValue, uint16_t color)
{
    (void)lcd;
    if (count < 2) {
        return;
    }

    uint16_t lastX = graphXForIndex(0, count, x0, x1);
    uint16_t lastY = graphYForValue(values[0], minValue, maxValue, y0, y1);

    for (uint8_t i = 1; i < count; ++i) {
        const uint16_t nextX = graphXForIndex(i, count, x0, x1);
        const uint16_t nextY = graphYForValue(values[i], minValue, maxValue, y0, y1);
        gui.drawLine(lastX, lastY, nextX, nextY, color);
        lastX = nextX;
        lastY = nextY;
    }
}

static void drawSeriesGraph(LcdBsp& lcd, Bsp::GuiEngine& gui, const uint32_t* values,
                            uint8_t count, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1, uint32_t minValue,
                            uint32_t maxValue, uint16_t color)
{
    (void)lcd;
    if (count < 2) {
        return;
    }

    const int32_t minVal32 = static_cast<int32_t>(minValue);
    const int32_t maxVal32 = static_cast<int32_t>(maxValue);
    uint16_t lastX = graphXForIndex(0, count, x0, x1);
    uint16_t lastY = graphYForValue(static_cast<int32_t>(values[0]), minVal32, maxVal32, y0, y1);

    for (uint8_t i = 1; i < count; ++i) {
        const uint16_t nextX = graphXForIndex(i, count, x0, x1);
        const uint16_t nextY = graphYForValue(static_cast<int32_t>(values[i]), minVal32, maxVal32, y0, y1);
        gui.drawLine(lastX, lastY, nextX, nextY, color);
        lastX = nextX;
        lastY = nextY;
    }
}

} // namespace

LcdBsp::LcdBsp(SPI_HandleTypeDef* hspi,
               GPIO_TypeDef* csPort, uint16_t csPin,
               GPIO_TypeDef* rsPort, uint16_t rsPin,
               GPIO_TypeDef* rstPort, uint16_t rstPin,
               GPIO_TypeDef* ledPort, uint16_t ledPin)
    : m_hspi(hspi)
    , m_csPort(csPort)
    , m_csPin(csPin)
    , m_rsPort(rsPort)
    , m_rsPin(rsPin)
    , m_rstPort(rstPort)
    , m_rstPin(rstPin)
    , m_ledPort(ledPort)
    , m_ledPin(ledPin)
{
}

void LcdBsp::init()
{
    printf("[LCD] LCD init start\r\n");

    // 1. 动态启用 GPIO 端口时钟
    if (m_csPort == GPIOA || m_rsPort == GPIOA || m_rstPort == GPIOA || m_ledPort == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    if (m_csPort == GPIOB || m_rsPort == GPIOB || m_rstPort == GPIOB || m_ledPort == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    if (m_csPort == GPIOC || m_rsPort == GPIOC || m_rstPort == GPIOC || m_ledPort == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }

    // 2. 初始化 GPIO 控制引脚为推挽输出模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = m_csPin;
    HAL_GPIO_Init(m_csPort, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = m_rsPin;
    HAL_GPIO_Init(m_rsPort, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = m_rstPin;
    HAL_GPIO_Init(m_rstPort, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = m_ledPin;
    HAL_GPIO_Init(m_ledPort, &GPIO_InitStruct);

    csHigh();

    // 3. 等待 LCD 上电稳定，再执行硬件复位
    HAL_Delay(500);
    reset();

    // 4. 先打开背光并显式启用 SPI，避免寄存器直写卡在 RXNE 等待。
    ledOn();
    printf("[LCD] backlight on\r\n");
    if ((m_hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE) {
        __HAL_SPI_ENABLE(m_hspi);
    }
    (void)m_hspi->Instance->DR;
    (void)m_hspi->Instance->SR;
    printf("[LCD] spi enabled\r\n");

    // 5. 写入 ST7796S 驱动芯片的核心初始化寄存器序列
    writeCmd(0xF0); writeData(0xC3);
    writeCmd(0xF0); writeData(0x96);
    
    writeCmd(0x36); writeData(0xE8); // 设置为横屏模式 (BGR，从右往左)
    m_width = 480;
    m_height = 320;
    
    writeCmd(0x3A); writeData(0x05); // 16-bit 像素格式
    writeCmd(0xB0); writeData(0x80);
    
    writeCmd(0xB6);
    writeData(0x00);
    writeData(0x02);
    
    writeCmd(0xB5);
    writeData(0x02);
    writeData(0x03);
    writeData(0x00);
    writeData(0x04);
    
    writeCmd(0xB1);
    writeData(0x80);
    writeData(0x10);
    
    writeCmd(0xB4); writeData(0x00);
    writeCmd(0xB7); writeData(0xC6);
    writeCmd(0xC5); writeData(0x24);
    writeCmd(0xE4); writeData(0x31);
    
    writeCmd(0xE8);
    writeData(0x40);
    writeData(0x8A);
    writeData(0x00);
    writeData(0x00);
    writeData(0x29);
    writeData(0x19);
    writeData(0xA5);
    writeData(0x33);
    
    writeCmd(0xC2);
    writeCmd(0xA7);

    // Gamma 曲线设置
    writeCmd(0xE0);
    writeData(0xF0); writeData(0x09); writeData(0x13); writeData(0x12);
    writeData(0x12); writeData(0x2B); writeData(0x3C); writeData(0x44);
    writeData(0x4B); writeData(0x1B); writeData(0x18); writeData(0x17);
    writeData(0x1D); writeData(0x21);

    writeCmd(0xE1);
    writeData(0xF0); writeData(0x09); writeData(0x13); writeData(0x0C);
    writeData(0x0D); writeData(0x27); writeData(0x3B); writeData(0x44);
    writeData(0x4D); writeData(0x0B); writeData(0x17); writeData(0x17);
    writeData(0x1D); writeData(0x21);

    writeCmd(0x36); writeData(0xEC); // 横屏显示方向
    writeCmd(0xF0); writeData(0xC3);
    writeCmd(0xF0); writeData(0x69);
    
    writeCmd(0x13); // 开启正常显示模式
    writeCmd(0x11); // 退出睡眠模式
    HAL_Delay(120);
    writeCmd(0x29); // 开启显示屏
    printf("[LCD] init sequence done\r\n");
}

void LcdBsp::reset()
{
    rstLow();
    HAL_Delay(100);
    rstHigh();
    HAL_Delay(50);
}

// SPI 单字节发送（与参考工程 SPI_WriteByte 一致，诊断阶段加超时）
static bool spiWriteByte(SPI_TypeDef* SPIx, uint8_t data)
{
    if (!waitForSpiFlagSet(SPIx, SPI_SR_TXE)) {
        printf("[LCD] SPI timeout waiting TXE before 0x%02X\r\n", data);
        return false;
    }
    SPIx->DR = data;
    if (!waitForSpiFlagSet(SPIx, SPI_SR_RXNE)) {
        printf("[LCD] SPI timeout waiting RXNE after 0x%02X SR=0x%04lX CR1=0x%04lX\r\n",
               data,
               static_cast<unsigned long>(SPIx->SR),
               static_cast<unsigned long>(SPIx->CR1));
        return false;
    }
    (void)SPIx->DR;
    return true;
}

void LcdBsp::csLow()
{
    HAL_GPIO_WritePin(m_csPort, m_csPin, GPIO_PIN_RESET);
}

void LcdBsp::csHigh()
{
    HAL_GPIO_WritePin(m_csPort, m_csPin, GPIO_PIN_SET);
}

void LcdBsp::writeCmd(uint8_t cmd)
{
    csLow();
    rsLow();
    spiWriteByte(m_hspi->Instance, cmd);
    csHigh();
}

void LcdBsp::writeData(uint8_t data)
{
    csLow();
    rsHigh();
    spiWriteByte(m_hspi->Instance, data);
    csHigh();
}

void LcdBsp::writeData16(uint16_t data)
{
    csLow();
    rsHigh();
    spiWriteByte(m_hspi->Instance, static_cast<uint8_t>(data >> 8));
    spiWriteByte(m_hspi->Instance, static_cast<uint8_t>(data & 0xFF));
    csHigh();
}

void LcdBsp::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    writeCmd(0x2A); // Column Address Set
    writeData(x0 >> 8);
    writeData(x0 & 0xFF);
    writeData(x1 >> 8);
    writeData(x1 & 0xFF);

    writeCmd(0x2B); // Row Address Set
    writeData(y0 >> 8);
    writeData(y0 & 0xFF);
    writeData(y1 >> 8);
    writeData(y1 & 0xFF);

    writeCmd(0x2C); // Memory Write (GRAM)
}

void LcdBsp::clear(uint16_t color)
{
    fillRect(0, 0, m_width - 1, m_height - 1, color);
}

void LcdBsp::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= m_width || y >= m_height) return;
    setAddressWindow(x, y, x, y);
    csLow();
    rsHigh();
    spiWriteByte(m_hspi->Instance, static_cast<uint8_t>(color >> 8));
    spiWriteByte(m_hspi->Instance, static_cast<uint8_t>(color & 0xFF));
    csHigh();
}

void LcdBsp::fillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (x0 >= m_width || y0 >= m_height || x1 >= m_width || y1 >= m_height) return;
    if (x0 > x1 || y0 > y1) return;

    setAddressWindow(x0, y0, x1, y1);

    csLow();
    rsHigh();

    uint8_t high = static_cast<uint8_t>(color >> 8);
    uint8_t low = static_cast<uint8_t>(color & 0xFF);
    uint32_t numPixels = (x1 - x0 + 1) * (y1 - y0 + 1);

    SPI_TypeDef* SPIx = m_hspi->Instance;

    // 高频极速寄存器直接写入，消除 HAL 库状态自检的循环延时
    for (uint32_t i = 0; i < numPixels; ++i) {
        while (!(SPIx->SR & SPI_SR_TXE));
        SPIx->DR = high;
        while (!(SPIx->SR & SPI_SR_TXE));
        SPIx->DR = low;
    }

    while (SPIx->SR & SPI_SR_BSY);
    (void)SPIx->DR; // clear RXNE to prevent OVR overrun
    (void)SPIx->SR; // clear OVR flag
    csHigh();
    // 恢复全屏窗口，与参考工程一致
    writeCmd(0x2A);
    writeData(0); writeData(0);
    writeData((m_width - 1) >> 8); writeData((m_width - 1) & 0xFF);
    writeCmd(0x2B);
    writeData(0); writeData(0);
    writeData((m_height - 1) >> 8); writeData((m_height - 1) & 0xFF);
}

void LcdBsp::drawChar(uint16_t x, uint16_t y, char c, uint16_t fc, uint16_t bc, uint8_t size)
{
    if (x + size / 2 > m_width || y + size > m_height) return;

    uint8_t num = c - ' '; // 获取 ASCII 偏移后的字符索引
    setAddressWindow(x, y, x + size / 2 - 1, y + size - 1);

    csLow();
    rsHigh();

    uint8_t highFc = static_cast<uint8_t>(fc >> 8);
    uint8_t lowFc = static_cast<uint8_t>(fc & 0xFF);
    uint8_t highBc = static_cast<uint8_t>(bc >> 8);
    uint8_t lowBc = static_cast<uint8_t>(bc & 0xFF);

    SPI_TypeDef* SPIx = m_hspi->Instance;

    for (uint8_t pos = 0; pos < size; ++pos) {
        uint8_t temp = (size == 12) ? kAsc2_1206[num][pos] : kAsc2_1608[num][pos];
        for (uint8_t t = 0; t < size / 2; ++t) {
            bool pixelIsOn = (temp & 0x01);
            uint8_t high = pixelIsOn ? highFc : highBc;
            uint8_t low  = pixelIsOn ? lowFc  : lowBc;

            while (!(SPIx->SR & SPI_SR_TXE));
            SPIx->DR = high;
            while (!(SPIx->SR & SPI_SR_TXE));
            SPIx->DR = low;

            temp >>= 1;
        }
    }

    while (SPIx->SR & SPI_SR_BSY);
    (void)SPIx->DR; // clear RXNE to prevent OVR overrun
    (void)SPIx->SR; // clear OVR flag
    csHigh();
}

void LcdBsp::drawString(uint16_t x, uint16_t y, const char* str, uint16_t fc, uint16_t bc, uint8_t size)
{
    while (*str) {
        if (x + size / 2 > m_width) {
            x = 0;
            y += size;
        }
        if (y + size > m_height) {
            break;
        }
        drawChar(x, y, *str, fc, bc, size);
        x += size / 2;
        str++;
    }
}

void LcdBsp::drawScaledInt(uint16_t x, uint16_t y, int32_t value,
                           uint8_t intDigits, uint8_t fracDigits,
                           uint16_t fc, uint16_t bc, uint8_t size)
{
    char buf[20];
    int divisor = 1;
    for(int i=0; i<fracDigits; ++i) divisor *= 10;
    formatScaledInt(buf, "", value, "", divisor);
    drawString(x, y, buf, fc, bc, size);
}

void LcdBsp::drawCenteredString(uint16_t y, const char* str, uint16_t fc, uint16_t bc, uint8_t size)
{
    fillRect(0, y, m_width - 1, y + size - 1, bc);
    uint16_t len = 0;
    while (str[len]) {
        len++;
    }
    uint16_t strWidth = len * (size / 2);
    uint16_t x = 0;
    if (m_width > strWidth) {
        x = (m_width - strWidth) / 2;
    }
    drawString(x, y, str, fc, bc, size);
}

void LcdBsp::update(const App::ILcdDisplay::RenderData& data)
{
    // 如果页面发生改变，或者初次渲染，进行全屏擦除并强制全局重绘
    bool forceRedraw = (data.currentViewPage != m_lastPage);
    if (forceRedraw) {
        clear(kColorBlack);
        m_lastPage = data.currentViewPage;
    }

    // 1. 渲染头部栏
    if (forceRedraw) {
        char headerBuf[32];
        drawString(20, 15, "MicroCP Sensor Monitor", kColorYellow, kColorBlack, 16);
        drawString(20, 35, "--------------------------------", kColorGray, kColorBlack, 16);
        sprintf(headerBuf, "GROUP: %lu", (unsigned long)SYS_GROUP_NUMBER);
        drawString(250, 15, headerBuf, kColorGreen, kColorBlack, 16);
    }

    // 2. 根据不同的调试页面，渲染核心环境传感器状态
    if (data.currentViewPage == 0) {
        renderDebuggingPage0(data, forceRedraw);
    } else if (data.currentViewPage == 1) {
        renderDebuggingPage1(data, forceRedraw);
    } else {
        renderSettingsPage(data, forceRedraw);
    }

    // 3. 渲染底部调试控制信息栏 (报警、静音和当前页码)
    renderDebuggingFooter(data, forceRedraw);
}

void LcdBsp::renderDebuggingPage0(const App::ILcdDisplay::RenderData& data, bool forceRedraw)
{
    char buf[64];
    const uint16_t graphX0 = 20;
    const uint16_t graphY0 = 60;
    const uint16_t graphX1 = 460;
    const uint16_t graphY1 = 170;

    fillRect(graphX0, graphY0, graphX1, graphY1, kColorBlack);
    m_gui->drawRectBorder(graphX0, graphY0, graphX1, graphY1, kColorGray);

    if (!data.tempHumConnected || data.historyCount < 2) {
        drawCenteredString(110, "TEMP/HUMI GRAPH WAITING", kColorGray, kColorBlack, 16);
    } else {
        int32_t minValue = data.tempHistory[0];
        int32_t maxValue = data.tempHistory[0];
        for (uint8_t i = 1; i < data.historyCount; ++i) {
            minValue = (data.tempHistory[i] < minValue) ? data.tempHistory[i] : minValue;
            maxValue = (data.tempHistory[i] > maxValue) ? data.tempHistory[i] : maxValue;
        }
        minValue = (data.tempLowLimit < minValue) ? data.tempLowLimit : minValue;
        maxValue = (data.tempHighLimit > maxValue) ? data.tempHighLimit : maxValue;
        if (maxValue - minValue < 20) {
            maxValue = minValue + 20;
        }
        drawSeriesGraph(*this, *m_gui, data.tempHistory, data.historyCount,
                        graphX0 + 1U, graphY0 + 1U, graphX1 - 1U, graphY1 - 1U,
                        minValue, maxValue, kColorRed);
    }

    bool connChanged = (data.tempHumConnected != m_lastTempHumConn);
    if (forceRedraw || connChanged) {
        if (!data.tempHumConnected) {
            drawCenteredString(190, "TEMP/HUMI: Unconnected", kColorRed, kColorBlack, 16);
        }
        m_lastTempHumConn = data.tempHumConnected;
        m_lastTemp = -9999;
        m_lastHum = -9999;
    }

    if (data.tempHumConnected) {
        if (forceRedraw || data.temperature != m_lastTemp) {
            formatScaledInt(buf, "TEMP: ", data.temperature, " C", 10);
            drawString(20, 190, buf, kColorWhite, kColorBlack, 16);
            m_lastTemp = data.temperature;
        }

        if (forceRedraw) {
            char lowBuf[20];
            char highBuf[20];
            formatScaledInt(lowBuf, "", data.tempLowLimit, "", 10);
            formatScaledInt(highBuf, "", data.tempHighLimit, "", 10);
            sprintf(buf, "TEMP LIMIT: %s ~ %s C", lowBuf, highBuf);
            drawString(20, 232, buf, kColorGray, kColorBlack, 16);
        }

        if (forceRedraw || data.humidity != m_lastHum) {
            formatScaledInt(buf, "HUMI: ", data.humidity, " %", 10);
            drawString(20, 210, buf, kColorWhite, kColorBlack, 16);
            m_lastHum = data.humidity;
        }
    }
}

void LcdBsp::renderDebuggingPage1(const App::ILcdDisplay::RenderData& data, bool forceRedraw)
{
    char buf[64];
    const uint16_t graphX0 = 20;
    const uint16_t graphY0 = 60;
    const uint16_t graphX1 = 460;
    const uint16_t graphY1 = 170;

    fillRect(graphX0, graphY0, graphX1, graphY1, kColorBlack);
    m_gui->drawRectBorder(graphX0, graphY0, graphX1, graphY1, kColorGray);

    if (!data.pressureConnected || data.historyCount < 2) {
        drawCenteredString(110, "PRESSURE GRAPH WAITING", kColorGray, kColorBlack, 16);
    } else {
        uint32_t minValue = data.pressHistory[0];
        uint32_t maxValue = data.pressHistory[0];
        for (uint8_t i = 1; i < data.historyCount; ++i) {
            minValue = (data.pressHistory[i] < minValue) ? data.pressHistory[i] : minValue;
            maxValue = (data.pressHistory[i] > maxValue) ? data.pressHistory[i] : maxValue;
        }
        minValue = (data.pressLowLimit < minValue) ? data.pressLowLimit : minValue;
        maxValue = (data.pressHighLimit > maxValue) ? data.pressHighLimit : maxValue;
        if (maxValue <= minValue) {
            maxValue = minValue + 1000U;
        }
        drawSeriesGraph(*this, *m_gui, data.pressHistory, data.historyCount,
                        graphX0 + 1U, graphY0 + 1U, graphX1 - 1U, graphY1 - 1U,
                        minValue, maxValue, kColorBlue);
    }

    bool connChanged = (data.pressureConnected != m_lastPressConn);
    if (forceRedraw || connChanged) {
        if (!data.pressureConnected) {
            drawCenteredString(190, "PRESSURE: Unconnected", kColorRed, kColorBlack, 16);
        }
        m_lastPressConn = data.pressureConnected;
        m_lastPress = 0;
        m_lastAlt = -9999;
    }

    if (data.pressureConnected) {
        if (forceRedraw || data.pressure != m_lastPress) {
            formatPressureKpa(buf, "PRES: ", data.pressure, " kPa");
            drawString(20, 190, buf, kColorWhite, kColorBlack, 16);
            m_lastPress = data.pressure;
        }

        if (forceRedraw) {
            char lowBuf[32];
            char highBuf[32];
            formatPressureKpa(lowBuf, "PRES LIMIT: ", data.pressLowLimit, " ~ ");
            formatPressureKpa(highBuf, "", data.pressHighLimit, " kPa");
            sprintf(buf, "%s%s", lowBuf, highBuf);
            drawString(20, 232, buf, kColorGray, kColorBlack, 16);
        }

        if (forceRedraw || data.altitude != m_lastAlt) {
            formatScaledInt(buf, "ALTI: ", data.altitude, " m", 10);
            drawString(20, 210, buf, kColorWhite, kColorBlack, 16);
            m_lastAlt = data.altitude;
        }
    }
}

void LcdBsp::renderSettingsPage(const App::ILcdDisplay::RenderData& data, bool forceRedraw)
{
    (void)forceRedraw;

    char buf[64];
    const char* labels[4] = {"TEMP HIGH", "TEMP LOW", "PRESS HIGH", "PRESS LOW"};

    for (uint8_t row = 0; row < 4; ++row) {
        const uint16_t rowY0 = static_cast<uint16_t>(60U + row * 42U);
        const uint16_t rowY1 = static_cast<uint16_t>(rowY0 + 32U);
        fillRect(20, rowY0, 460, rowY1, kColorBlack);
        m_gui->drawRectBorder(20, rowY0, 460, rowY1, kColorGray);

        if (data.selectedThresholdField == row) {
            m_gui->drawRectBorder(18, rowY0 - 2U, 462, rowY1 + 2U, kColorYellow);
        }

        drawString(24, rowY0 + 8U, labels[row], kColorWhite, kColorBlack, 16);

        switch (row) {
            case 0:
                formatScaledInt(buf, "", data.pendingTempHighLimit, " C", 10);
                break;
            case 1:
                formatScaledInt(buf, "", data.pendingTempLowLimit, " C", 10);
                break;
            case 2:
                formatPressureKpa(buf, "", data.pendingPressHighLimit, " kPa");
                break;
            case 3:
                formatPressureKpa(buf, "", data.pendingPressLowLimit, " kPa");
                break;
            default:
                buf[0] = '\0';
                break;
        }

        drawString(220, rowY0 + 8U, buf, kColorWhite, kColorBlack, 16);
        drawBoxedButton(*this, 360, rowY0, 388, rowY1, "-", kColorWhite, kColorBlue);
        drawBoxedButton(*this, 400, rowY0, 428, rowY1, "+", kColorWhite, kColorBlue);
    }

    drawBoxedButton(*this, 40, 250, 190, 288, "CONFIRM", kColorGreen, kColorBlack);
    drawBoxedButton(*this, 300, 250, 450, 288, "CANCEL", kColorWhite, kColorRed);
}

void LcdBsp::renderDebuggingFooter(const App::ILcdDisplay::RenderData& data, bool forceRedraw)
{
    char buf[40];

    // --- 渲染报警状态指示栏 ---
    if (data.currentViewPage == 2) {
        if (forceRedraw) {
            sprintf(buf, "PAGE: %d/3", data.currentViewPage + 1);
            drawString(390, 15, buf, kColorYellow, kColorBlack, 16);
        }
        return;
    }

    if (forceRedraw || data.alarmState != m_lastAlarmState || data.isMuted != m_lastMute) {
        drawString(20, 220, "--------------------------------", kColorGray, kColorBlack, 16);
        
        const char* stateStr = "UNKNOWN";
        uint16_t stateColor = kColorWhite;

        switch (data.alarmState) {
            case Sys::AlarmState::NORMAL:
                stateStr = "NORMAL";
                stateColor = kColorGreen;
                break;
            case Sys::AlarmState::WARNING_TEMP:
                stateStr = "WARNING TEMP";
                stateColor = kColorRed;
                break;
            case Sys::AlarmState::WARNING_PRES:
                stateStr = "WARNING PRES";
                stateColor = kColorRed;
                break;
            case Sys::AlarmState::MUTED:
                stateStr = "MUTED ALARM";
                stateColor = kColorYellow;
                break;
            default:
                break;
        }

        sprintf(buf, "SYS STATUS: %-15s", stateStr);
        drawString(20, 245, buf, stateColor, kColorBlack, 16);

        sprintf(buf, "MUTE: %s", data.isMuted ? "ON " : "OFF");
        drawString(300, 245, buf, data.isMuted ? kColorYellow : kColorGray, kColorBlack, 16);

        m_lastAlarmState = data.alarmState;
        m_lastMute = data.isMuted;
    }

    if (forceRedraw) {
        drawBoxedButton(*this, 20, 276, 220, 312, "SETTINGS", kColorWhite, kColorBlack);
    }

    // --- 渲染页面页码栏 ---
    if (forceRedraw) {
        sprintf(buf, "PAGE: %d/3", data.currentViewPage + 1);
        drawString(390, 15, buf, kColorYellow, kColorBlack, 16);
    }
}

} // namespace Bsp
