// BSP/Inc/ButtonBsp.hpp
#pragma once
#include "IButton.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

class ButtonBsp : public App::IButton {
public:
    ButtonBsp(GPIO_TypeDef* gpioPort, uint16_t gpioPin);
    
    bool isPressed() override;
    
    // 定时器 10ms 中断扫描方法
    void scanTick() override;

private:
    GPIO_TypeDef* m_port;
    uint16_t m_pin;
    
    uint16_t m_debounceCounter{0};
    bool m_triggered{false};
    bool m_lastState{true}; // 默认高电平未按下
};

} // namespace Bsp
