// BSP/Inc/ButtonBsp.hpp
#pragma once
#include "IButton.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

class ButtonBsp : public App::IButton {
public:
    ButtonBsp(GPIO_TypeDef* gpioPort, uint16_t gpioPin);
    
    bool isPressed() override;
    
    // 由调度器按 10ms 周期调用的扫描方法
    void scanTick() override;

private:
    GPIO_TypeDef* m_port;
    uint16_t m_pin;
    
    uint16_t m_debounceCounter{0};
    uint16_t m_releaseCounter{0};
    uint32_t m_lastTriggerTimeMs{0};
    bool m_triggered{false};
    bool m_lastState{true}; // 默认高电平未按下
};

} // namespace Bsp
