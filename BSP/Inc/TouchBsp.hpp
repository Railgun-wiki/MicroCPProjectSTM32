// BSP/Inc/TouchBsp.hpp
#pragma once
#include "ITouch.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

class TouchBsp : public App::ITouch {
public:
    TouchBsp(GPIO_TypeDef* clkPort,  uint16_t clkPin,
             GPIO_TypeDef* csPort,   uint16_t csPin,
             GPIO_TypeDef* dinPort,  uint16_t dinPin,
             GPIO_TypeDef* outPort,  uint16_t outPin,
             GPIO_TypeDef* irqPort,  uint16_t irqPin);

    bool init() override;
    bool isTouched() override;
    bool readPosition(App::TouchPoint& point) override;

    // 校准参数设置（app_entry.cpp 在初始化后调用）
    void setCalibration(int16_t xMin, int16_t xMax,
                        int16_t yMin, int16_t yMax);

private:
    GPIO_TypeDef* m_clkPort;  uint16_t m_clkPin;
    GPIO_TypeDef* m_csPort;   uint16_t m_csPin;
    GPIO_TypeDef* m_dinPort;  uint16_t m_dinPin;
    GPIO_TypeDef* m_outPort;  uint16_t m_outPin;
    GPIO_TypeDef* m_irqPort;  uint16_t m_irqPin;

    int16_t m_xMin{0}, m_xMax{4095};
    int16_t m_yMin{0}, m_yMax{4095};
    bool m_calibrated{false};

    // Bit-bang SPI 助手
    inline void clkLow()  { HAL_GPIO_WritePin(m_clkPort, m_clkPin, GPIO_PIN_RESET); }
    inline void clkHigh() { HAL_GPIO_WritePin(m_clkPort, m_clkPin, GPIO_PIN_SET); }
    inline void csLow()   { HAL_GPIO_WritePin(m_csPort,  m_csPin,  GPIO_PIN_RESET); }
    inline void csHigh()  { HAL_GPIO_WritePin(m_csPort,  m_csPin,  GPIO_PIN_SET); }
    inline void dinLow()  { HAL_GPIO_WritePin(m_dinPort, m_dinPin, GPIO_PIN_RESET); }
    inline void dinHigh() { HAL_GPIO_WritePin(m_dinPort, m_dinPin, GPIO_PIN_SET); }
    uint16_t readChannel(uint8_t cmd);
    uint16_t readFiltered(uint8_t cmd);
};

} // namespace Bsp
