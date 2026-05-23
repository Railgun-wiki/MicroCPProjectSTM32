// BSP/Src/ButtonBsp.cpp
#include "ButtonBsp.hpp"

namespace Bsp {

ButtonBsp::ButtonBsp(GPIO_TypeDef* gpioPort, uint16_t gpioPin)
    : m_port(gpioPort)
    , m_pin(gpioPin)
{
}

bool ButtonBsp::isPressed()
{
    if (m_triggered) {
        m_triggered = false;
        return true;
    }
    return false;
}

void ButtonBsp::scanTick()
{
    GPIO_PinState currentPin = HAL_GPIO_ReadPin(m_port, m_pin);

    if (currentPin == GPIO_PIN_RESET) { // 按键低电平按下
        if (m_lastState) {
            m_debounceCounter++;
            if (m_debounceCounter >= 2) { // 连续 20ms 稳定低电平，判定有效按下
                m_triggered = true;
                m_lastState = false; // 锁定按键状态，防止连击
            }
        }
    } else { // 按键高电平释放
        m_debounceCounter = 0;
        m_lastState = true; // 解锁状态
    }
}

} // namespace Bsp
