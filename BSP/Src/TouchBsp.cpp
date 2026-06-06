// BSP/Src/TouchBsp.cpp
#include "TouchBsp.hpp"
#include <stdlib.h>

namespace Bsp {

// 72MHz 下粗略 1us 延时（~72 条指令周期）
static inline void delayUs(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 18; ++i) {
        __asm volatile ("nop");
    }
}

TouchBsp::TouchBsp(GPIO_TypeDef* clkPort, uint16_t clkPin,
                   GPIO_TypeDef* csPort,  uint16_t csPin,
                   GPIO_TypeDef* dinPort, uint16_t dinPin,
                   GPIO_TypeDef* outPort, uint16_t outPin,
                   GPIO_TypeDef* irqPort, uint16_t irqPin)
    : m_clkPort(clkPort), m_clkPin(clkPin)
    , m_csPort(csPort),   m_csPin(csPin)
    , m_dinPort(dinPort), m_dinPin(dinPin)
    , m_outPort(outPort), m_outPin(outPin)
    , m_irqPort(irqPort), m_irqPin(irqPin)
{
}

bool TouchBsp::init()
{
    csHigh();
    clkLow();
    dinLow();

    return true;
}

bool TouchBsp::isTouched()
{
    return HAL_GPIO_ReadPin(m_irqPort, m_irqPin) == GPIO_PIN_RESET;
}

uint16_t TouchBsp::readChannel(uint8_t cmd)
{
    uint16_t num = 0;

    clkLow();
    csLow();

    // 发送 8-bit 命令
    for (uint8_t i = 0; i < 8; ++i) {
        if (cmd & 0x80) {
            dinHigh();
        } else {
            dinLow();
        }
        cmd <<= 1;
        clkLow();
        delayUs(1);
        clkHigh();
        delayUs(1);
    }

    // 等待转换完成（XPT2046 最长 6us）
    delayUs(6);

    // 读取 12-bit 结果
    for (uint8_t i = 0; i < 12; ++i) {
        num <<= 1;
        clkHigh();
        delayUs(1);
        clkLow();
        delayUs(1);
        if (HAL_GPIO_ReadPin(m_outPort, m_outPin) == GPIO_PIN_SET) {
            num |= 1;
        }
    }

    csHigh();
    clkLow();

    return num;
}

uint16_t TouchBsp::readFiltered(uint8_t cmd)
{
    #define READ_TIMES 5
    uint16_t buf[READ_TIMES];

    for (uint8_t i = 0; i < READ_TIMES; ++i) {
        buf[i] = readChannel(cmd);
        delayUs(2);
    }

    // 冒泡排序
    for (uint8_t i = 0; i < READ_TIMES - 1; ++i) {
        for (uint8_t j = i + 1; j < READ_TIMES; ++j) {
            if (buf[i] > buf[j]) {
                uint16_t tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }
        }
    }

    // 去掉最低和最高值取平均
    uint16_t sum = 0;
    for (uint8_t i = 1; i < READ_TIMES - 1; ++i) {
        sum += buf[i];
    }
    return sum / (READ_TIMES - 2);
}

bool TouchBsp::readPosition(App::TouchPoint& point)
{
    if (!isTouched()) {
        point.valid = false;
        return false;
    }

    uint16_t rawX = readFiltered(0xD0); // X 通道
    uint16_t rawY = readFiltered(0x90); // Y 通道

    if (m_calibrated) {
        int32_t cx = (static_cast<int32_t>(rawX) - m_xMin) * 480 / (m_xMax - m_xMin);
        int32_t cy = (static_cast<int32_t>(rawY) - m_yMin) * 320 / (m_yMax - m_yMin);
        point.x = (cx < 0) ? 0 : ((cx > 479) ? 479 : static_cast<uint16_t>(cx));
        point.y = (cy < 0) ? 0 : ((cy > 319) ? 319 : static_cast<uint16_t>(cy));
    } else {
        point.x = rawX;
        point.y = rawY;
    }

    point.valid = true;
    return true;
}

void TouchBsp::setCalibration(int16_t xMin, int16_t xMax,
                              int16_t yMin, int16_t yMax)
{
    m_xMin = xMin;
    m_xMax = xMax;
    m_yMin = yMin;
    m_yMax = yMax;
    m_calibrated = true;
}

} // namespace Bsp
