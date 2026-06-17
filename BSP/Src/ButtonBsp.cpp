// BSP/Src/ButtonBsp.cpp
#include "ButtonBsp.hpp"
#include "sys.hpp"

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
    uint32_t now = HAL_GetTick();

    if (currentPin == GPIO_PIN_RESET) { // 按键低电平按下
        m_releaseCounter = 0; // 重置释放计数
        if (m_lastState) {
            m_debounceCounter++;
            if (m_debounceCounter >= 5) { // 连续 50ms 稳定低电平，判定有效按下
                // 1秒内防连击锁定
                if (now - m_lastTriggerTimeMs >= 1000U) {
                    m_triggered = true;
                    m_lastTriggerTimeMs = now;
                    SYS_LOG("Button GPIO Pin 0x%04X pressed.", m_pin);
                } else {
                    SYS_LOG("Button GPIO Pin 0x%04X press ignored (lockout period active).", m_pin);
                }
                m_lastState = false; // 锁定按键状态，防止连击
            }
        }
    } else { // 按键高电平释放
        m_debounceCounter = 0; // 重置按下计数
        if (!m_lastState) {
            m_releaseCounter++;
            if (m_releaseCounter >= 5) { // 连续 50ms 稳定高电平，判定有效释放
                m_lastState = true; // 解锁状态
                m_releaseCounter = 0;
                SYS_LOG("Button GPIO Pin 0x%04X released.", m_pin);
            }
        }
    }
}

} // namespace Bsp
