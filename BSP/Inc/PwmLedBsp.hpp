// BSP/Inc/PwmLedBsp.hpp
#pragma once
#include "IIndicator.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

class PwmLedBsp : public App::IIndicator {
public:
    PwmLedBsp(TIM_HandleTypeDef* htim, uint32_t channel);
    
    void setNormalBreathing() override;
    void setWarningBlinking() override;
    void turnOff() override;
    
    // 由调度器周期性推进动画物理计算并更新 CCR 寄存器
    void updatePhysics(uint32_t elapsedMs) override;

private:
    TIM_HandleTypeDef* m_htim;
    uint32_t m_channel;
    
    enum class VisualMode : uint8_t {
        OFF = 0,
        BREATHING,
        BLINKING
    } m_mode{VisualMode::OFF};
    
    uint32_t m_timeAcc{0};  // 时间累加器 (ms)
    bool m_blinkState{false};
};

} // namespace Bsp
