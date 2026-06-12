// BSP/Src/PwmLedBsp.cpp
#include "PwmLedBsp.hpp"
#include "MathTables.hpp"

namespace Bsp {

namespace {

constexpr uint32_t kLutMax = 945;
constexpr uint32_t kBlinkOnPercent = 95;

uint32_t scaleToTimerPeriod(const TIM_HandleTypeDef* htim, uint32_t value, uint32_t valueMax)
{
    if (htim == nullptr || valueMax == 0) {
        return 0;
    }

    const uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim);
    return (period * value) / valueMax;
}

uint32_t scalePercentToTimerPeriod(const TIM_HandleTypeDef* htim, uint32_t percent)
{
    if (htim == nullptr) {
        return 0;
    }

    const uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim);
    return (period * percent) / 100U;
}

} // namespace

PwmLedBsp::PwmLedBsp(TIM_HandleTypeDef* htim, uint32_t channel)
    : m_htim(htim)
    , m_channel(channel)
{
}

void PwmLedBsp::setNormalBreathing()
{
    m_mode = VisualMode::BREATHING;
    m_timeAcc = 0;
    HAL_TIM_PWM_Start(m_htim, m_channel);
}

void PwmLedBsp::setWarningBlinking()
{
    m_mode = VisualMode::BLINKING;
    m_timeAcc = 0;
    HAL_TIM_PWM_Start(m_htim, m_channel);
}

void PwmLedBsp::turnOff()
{
    m_mode = VisualMode::OFF;
    HAL_TIM_PWM_Stop(m_htim, m_channel);
    __HAL_TIM_SET_COMPARE(m_htim, m_channel, 0);
}

void PwmLedBsp::updatePhysics(uint32_t elapsedMs)
{
    if (m_mode == VisualMode::OFF) return;

    m_timeAcc += elapsedMs;

    if (m_mode == VisualMode::BREATHING) {
        // 呼吸灯周期设为 2000ms (2s)，查表大小为 64
        uint32_t phase = m_timeAcc % 2000;
        uint32_t index = (phase * 64) / 2000;
        if (index >= 64) index = 63;
        
        const uint32_t duty = scaleToTimerPeriod(m_htim, PWM_SINE_LUT[index], kLutMax);
        
        __HAL_TIM_SET_COMPARE(m_htim, m_channel, duty);
    } 
    else if (m_mode == VisualMode::BLINKING) {
        // 5Hz 异常闪烁，周期为 200ms，每 100ms 翻转一次电平
        if (m_timeAcc >= 100) {
            m_timeAcc = 0;
            m_blinkState = !m_blinkState;
            __HAL_TIM_SET_COMPARE(m_htim,
                                  m_channel,
                                  m_blinkState ? scalePercentToTimerPeriod(m_htim, kBlinkOnPercent) : 0);
        }
    }
}

} // namespace Bsp
